#ifndef HTTP_HANDLERS_H_
#define HTTP_HANDLERS_H_

#include "esp_err.h"
#include "esp_http_server.h"

constexpr const char *CAMERA_TAG = "cam_ov2640";

esp_err_t img_handler(httpd_req_t *req);

#endif

