#include "http_handlers.h"
#include "hand_gesture_model.h"

#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdint.h>
#include <stddef.h>
#include <math.h>
#include <atomic>

#include "html.h"

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

QueueHandle_t recognize_http_queue = nullptr;
std::atomic<bool> recognize_busy{false};
constexpr uint32_t kHttpWorkerStackBytes = 6 * 1024;

esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");

    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t *fb = nullptr;
    esp_err_t res = ESP_OK;

    size_t jpg_buf_len = 0;
    uint8_t *jpg_buf = nullptr;
    char part_buf[128];

    res = httpd_resp_set_type(req, STREAM_CONTENT_TYPE);
    if (res != ESP_OK){
        return res;
    }

    for(;;){
        fb = esp_camera_fb_get();
        if (!fb){
            ESP_LOGE(CAMERA_TAG, "Camera capture failed");
            res = ESP_FAIL;
        } else {
            res = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len) ? ESP_OK : ESP_FAIL;
            esp_camera_fb_return(fb);
            fb = nullptr;
        }

        // boundary
        if (res == ESP_OK){
            res = httpd_resp_send_chunk(req, STREAM_BOUNDARY, strlen(STREAM_BOUNDARY));
        }

        // jpeg header
        if (res == ESP_OK){
            size_t header_len = snprintf(part_buf, sizeof(part_buf), STREAM_PART, jpg_buf_len);
            res = httpd_resp_send_chunk(req, part_buf, header_len);
        }

        // jpeg data
        if (res == ESP_OK){
            res = httpd_resp_send_chunk(req, reinterpret_cast<const char*>(jpg_buf), jpg_buf_len);
        }

        if (fb){
            esp_camera_fb_return(fb);
            fb = nullptr;
            jpg_buf = nullptr;
        } else if (jpg_buf) {
            free(jpg_buf);
            jpg_buf = nullptr;
        }

        if (res != ESP_OK){
            ESP_LOGE(CAMERA_TAG, "Send frame failed");
            break;
        }
    }

    return res;
}

esp_err_t respond_json(httpd_req_t *req, const char *status, const char *json) {
    esp_err_t err = httpd_resp_set_status(req, status);
    if (err == ESP_OK) 
        err = httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) 
        err = httpd_resp_set_hdr(req, "Cache-Control", "no-store");
    if (err == ESP_OK) 
        err = httpd_resp_sendstr(req, json);
    return err;
}

void recognize_gesture(httpd_req_t *req){

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb){
        respond_json(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"camera_capture_failed\"}");
        return;
    }

    get_hand_gesture_frame(fb);
    esp_camera_fb_return(fb);

    HandGestureResult result{};

    const esp_err_t err = wait_hand_gesture_result(&result, RECOGNIZE_HANDLER_TIMEOUT_MS);

    if (err == ESP_ERR_TIMEOUT) {
        respond_json(req, "504 Gateway Timeout", "{\"ok\":false,\"error\":\"inference_timeout\"}");
        return;
    }
    if (err != ESP_OK) {
        respond_json(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"inference_failed\"}");
        return;
    }

    char json[256];
    snprintf(json, sizeof(json), "{\"ok\":true, \"gesture\":\"%c\", \"confidence\":%.3f}", result.label, result.probability);
    respond_json(req, "200 OK", json);
}

void recognize_http_worker(void *){
    ESP_LOGI(CAMERA_TAG, "Recognize HTTP worker on Core %d", xPortGetCoreID());

    httpd_req_t *async_req = nullptr;

    for(;;){
        if (xQueueReceive(recognize_http_queue, &async_req, portMAX_DELAY) != pdTRUE){
            continue;
        }
        if (!async_req) {
            continue;
        }

        recognize_gesture(async_req);

        const esp_err_t err = httpd_req_async_handler_complete(async_req);
        if (err != ESP_OK) {
            ESP_LOGE(CAMERA_TAG, "Async http request error");
        }
        recognize_busy.store(false, std::memory_order_release);
    }
}

void init_recognize_http_worker() {
    recognize_http_queue = xQueueCreate(1, sizeof(httpd_req_t *));
    if (!recognize_http_queue) {
        ESP_LOGE(TFLITE_TAG, "Creating http worker queue failed");

        return;
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        recognize_http_worker, 
        "recognize_http", 
        kHttpWorkerStackBytes,
        nullptr, 
        3, 
        nullptr, 
        0
    );

    if (created != pdPASS) {
        vQueueDelete(recognize_http_queue);
        recognize_http_queue = nullptr;

        ESP_LOGE(TFLITE_TAG, "Creating http worker failed");

        return;
    }

}

esp_err_t recognize_handler(httpd_req_t *req){
    if (!recognize_http_queue){
        return respond_json(req, "503 Service Unavailable", "{\"ok\":false,\"error\":\"model_not_ready\"}");
    }

    if (recognize_busy.exchange(true, std::memory_order_acq_rel)) {
        return respond_json(req, "503 Service Unavailable","{\"ok\":false,\"error\":\"recognition_busy\"}");
    }

    httpd_req_t *async_req = nullptr;
    esp_err_t err = httpd_req_async_handler_begin(req, &async_req);

    if (err != ESP_OK) {
        recognize_busy.store(false, std::memory_order_release);
        return respond_json(req, "500 Internal Server Error", "{\"ok\":false,\"error\":\"async_start_failed\"}");
    }

    if (xQueueSend(recognize_http_queue, &async_req, 0) != pdTRUE) {
        respond_json(async_req, "503 Service Unavailable", "{\"ok\":false,\"error\":\"worker_busy\"}");
        httpd_req_async_handler_complete(async_req);
        recognize_busy.store(false, std::memory_order_release);
    }

    return ESP_OK;

}