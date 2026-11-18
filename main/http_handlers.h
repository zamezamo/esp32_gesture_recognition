#ifndef HTTP_HANDLERS_H_
#define HTTP_HANDLERS_H_

#include "esp_err.h"
#include "esp_http_server.h"

constexpr const char *CAMERA_TAG = "cam_ov2640";

esp_err_t index_handler(httpd_req_t *req);
esp_err_t stream_handler(httpd_req_t *req);
esp_err_t recognize_handler(httpd_req_t *req);

#endif

