#include "http_server.h"
#include "http_handlers.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const httpd_uri_t index_uri = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = index_handler,
    .user_ctx = nullptr
};

static const httpd_uri_t recognize_uri = {
    .uri = "/recognize",
    .method = HTTP_POST,
    .handler = recognize_handler,
    .user_ctx = nullptr
};

void start_http_server(){
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = HTTP_PORT;
    config.ctrl_port = HTTP_PORT + 10;
    config.core_id = 0;
    config.lru_purge_enable = true;

    ESP_LOGI(HTTP_TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(HTTP_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &index_uri);
        httpd_register_uri_handler(server, &recognize_uri);
    }
}