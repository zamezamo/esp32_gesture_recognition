#include <cstdio>
#include <cstring>
#include <string>

#include "main.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_netif.h"
#include "esp_http_server.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "esp_cache.h"
#include "driver/i2c_master.h"
#include "esp_cam_ctlr.h"
#include "esp_cam_ctlr_dvp.h"

#define CORE0       0
// only define xCoreID CORE1 as 1 if this is a multiple core processor target, else define it as tskNO_AFFINITY
#define CORE1       ((CONFIG_FREERTOS_NUMBER_OF_CORES > 1) ? 1 : tskNO_AFFINITY)

#define TASK_PRIORITY_HIGH 3

/* FreeRTOS event group to signal when we are connected*/
static EventGroupHandle_t wifi_event_group;

/* The event group allows multiple bits for each event, but we only care about two events:
 * - we are connected to the AP with an IP
 * - we failed to connect after the maximum amount of retries */
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1


#define CAMERA_HRES             (240)
#define CAMERA_VRES             (240)
#define RGB565_BITS_PER_PIXEL   (8)

#define DVP_CAM_XCLK_FREQ_HZ       (20000000)
#define DVP_CAM_DATA_WIDTH         (8)
#define DVP_CAM_D0_IO               (5)
#define DVP_CAM_D1_IO               (18)
#define DVP_CAM_D2_IO               (19)
#define DVP_CAM_D3_IO               (21)
#define DVP_CAM_D4_IO               (36)
#define DVP_CAM_D5_IO               (39)
#define DVP_CAM_D6_IO               (34)
#define DVP_CAM_D7_IO               (35)
#define EXAMPLE_DVP_CAM_XCLK_IO            (0)
#define EXAMPLE_DVP_CAM_PCLK_IO            (4)
#define EXAMPLE_DVP_CAM_DE_IO              (22)
#define EXAMPLE_DVP_CAM_VSYNC_IO           (37)
#define EXAMPLE_DVP_CAM_HSYNC_IO           (-1)

static const char *WIFI_TAG = "wifi_ap";
static const char *HTTP_TAG = "http_server";

static int wifi_retry_num = 0;

void *cam_frame_buffer = NULL;
size_t cam_frame_buffer_size = 0;

// event handler for wifi and ip connection
static void esp_wifi_event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data){
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (wifi_retry_num < WIFI_CONN_MAX_RETRIES) {
            esp_wifi_connect();
            wifi_retry_num++;
            ESP_LOGI(WIFI_TAG, "Retry to connect to the AP");
        } else {
            xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(WIFI_TAG,"Connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(WIFI_TAG, "Got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        wifi_retry_num = 0;
        
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

void wifi_init_sta(){
    wifi_event_group = xEventGroupCreate();

    // init lwip
    ESP_ERROR_CHECK(esp_netif_init());

    // creating system event cycle
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    // init wifi with default parameters
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // register event instance handlers
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &esp_wifi_event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &esp_wifi_event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // configuring wi-fi
    wifi_config_t wifi_config = {};
    wifi_config.sta.threshold.authmode = WIFI_SCAN_AUTH_MODE_THRESHOLD;
    std::strcpy(reinterpret_cast<char*>(wifi_config.sta.ssid), WIFI_SSID);
    std::strcpy(reinterpret_cast<char*>(wifi_config.sta.password), WIFI_PASS);
    
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // init wifi driver
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(WIFI_TAG, "Wifi initialized.");

    /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
     * number of re-tries (WIFI_FAIL_BIT). The bits are set by event_handler() (see above) */
    EventBits_t bits = xEventGroupWaitBits(wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    /* xEventGroupWaitBits() returns the bits before the call returned, hence we can test which event actually
     * happened. */
    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(WIFI_TAG, "Connected to SSID:%s password:%s", WIFI_SSID, WIFI_PASS);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(WIFI_TAG, "Failed to connect to SSID:%s, password:%s", WIFI_SSID, WIFI_PASS);
    } else {
        ESP_LOGE(WIFI_TAG, "UNEXPECTED EVENT");
    }
}

static esp_err_t hello_get_handler(httpd_req_t *req){
    std::string response = "Hello!\n";
    
    httpd_resp_set_type(req, "text/plain");
    httpd_resp_send(req, response.c_str(), response.size());

    return ESP_OK;
}

static const httpd_uri_t hello_uri = {
    .uri = "/hello",
    .method = HTTP_GET,
    .handler = hello_get_handler,
    .user_ctx = NULL
};

static httpd_handle_t start_http_server(){
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = HTTP_PORT;
    config.lru_purge_enable = true;

    ESP_LOGI(HTTP_TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(HTTP_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello_uri);
    }

    return server;
}

//Initialize NVS
void init_nvs(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void init_camera(){
    cam_frame_buffer_size = CAMERA_HRES * CAMERA_VRES * RGB565_BITS_PER_PIXEL;

    esp_cam_ctlr_handle_t cam_handle = NULL;

    esp_cam_ctlr_dvp_pin_config_t pin_cfg = {
        .data_width = EXAMPLE_DVP_CAM_DATA_WIDTH,
        .data_io = {
            DVP_CAM_D0_IO,
            DVP_CAM_D1_IO,
            DVP_CAM_D2_IO,
            DVP_CAM_D3_IO,
            DVP_CAM_D4_IO,
            DVP_CAM_D5_IO,
            DVP_CAM_D6_IO,
            DVP_CAM_D7_IO,
        },
        .vsync_io = EXAMPLE_DVP_CAM_VSYNC_IO,
        .de_io = EXAMPLE_DVP_CAM_DE_IO,
        .pclk_io = EXAMPLE_DVP_CAM_PCLK_IO,
        .xclk_io = EXAMPLE_DVP_CAM_XCLK_IO, // Set XCLK pin to generate XCLK signal
    };
}

extern "C" void app_main(void)
{
    init_nvs();
    start_http_server();
    wifi_init_sta();
}