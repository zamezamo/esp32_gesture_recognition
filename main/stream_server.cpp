#include "stream_server.h"
#include "http_handlers.h"

#include "esp_http_server.h"
#include "esp_log.h"

static const httpd_uri_t stream_uri = {
    .uri = "/stream",
    .method = HTTP_GET,
    .handler = stream_handler,
    .user_ctx = nullptr
};

void start_stream_server(){
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    config.server_port = STREAM_PORT;
    config.ctrl_port = STREAM_PORT + 10;
    config.core_id = 0;
    config.lru_purge_enable = true;

    ESP_LOGI(STREAM_TAG, "Starting server on port: '%d'", config.server_port);

    if (httpd_start(&server, &config) == ESP_OK) {
        // Set URI handlers
        ESP_LOGI(STREAM_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &stream_uri);
    }
}