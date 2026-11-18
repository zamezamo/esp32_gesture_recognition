#include "http_handlers.h"
#include "hand_gesture_model.h"

#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#include "html.h"

#define PART_BOUNDARY "123456789000000000000987654321"

static const char* STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

static camera_fb_t *camera_fb = nullptr;

esp_err_t index_handler(httpd_req_t *req){
    httpd_resp_set_type(req, "text/html");

    return httpd_resp_send(req, INDEX_HTML, HTTPD_RESP_USE_STRLEN);
}

esp_err_t stream_handler(httpd_req_t *req){
    camera_fb_t *fb = camera_fb;
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
            if(fb->format != PIXFORMAT_JPEG){
                res = frame2jpg(fb, 80, &jpg_buf, &jpg_buf_len) ? ESP_OK : ESP_FAIL;
                esp_camera_fb_return(fb);
                fb = nullptr;
            } else {
                jpg_buf = fb->buf;
                jpg_buf_len = fb->len;
            }
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

esp_err_t recognize_handler(httpd_req_t *req){
    // TODO: create recognize handler
    return httpd_resp_send_500(req);
}