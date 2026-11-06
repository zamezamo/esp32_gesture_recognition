#include "http_handlers.h"
#include "hand_gesture_model.h"

#include "esp_http_server.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_camera.h"

#include <stdint.h>
#include <stddef.h>
#include <math.h>

struct jpg_chunking_t{
    httpd_req_t *req;
    size_t len;
};

static size_t jpg_encode_stream(void * arg, size_t index, const void* data, size_t len){
    jpg_chunking_t *j = (jpg_chunking_t *)arg;
    if(!index){
        j->len = 0;
    }
    if(httpd_resp_send_chunk(j->req, (const char *)data, len) != ESP_OK){
        return 0;
    }
    j->len += len;
    return len;
}

void copy_img_to_tensor(const uint8_t *image_resized, TfLiteTensor *input){
    const float scale = input->params.scale;
    const int zero_point = input->params.zero_point;

    for (int i = 0; i < kMaxImageSize; i++){
        float pixel = image_resized[i] / 255.0f;
        int value = (int)roundf(pixel / scale) + zero_point;

        value = std::clamp(value, 0, 255);
        input->data.uint8[i] = (uint8_t)value;
    }
}

static uint8_t* resize_img(camera_fb_t *fb){
    static uint8_t resized_img[kMaxImageSize];

    const int src_width = fb->width;
    const int src_height = fb->height;

    const float scale_x = (float)src_width / kNumCols;
    const float scale_y = (float)src_height / kNumRows;

    for (int y = 0; y < kNumRows; y++){
        float src_y = (y + 0.5f) * scale_y - 0.5f;
        src_y = fmaxf(0.0f, fminf(src_y, src_height - 1));

        int y0 = (int)src_y;
        int y1 = (y0 + 1 < src_height) ? y0 + 1 : y0;

        float dy = src_y - y0;

        for (int x = 0; x < kNumCols; x++) {
            float src_x = (x + 0.5f) * scale_x - 0.5f;
            src_x = fmaxf(0.0f, fminf(src_x, src_width - 1));

            int x0 = (int)src_x;
            int x1 = (x0 + 1 < src_width) ? x0 + 1 : x0;

            float dx = src_x - x0;

            float p00 = fb->buf[y0 * src_width + x0];
            float p01 = fb->buf[y0 * src_width + x1];

            float p10 = fb->buf[y1 * src_width + x0];
            float p11 = fb->buf[y1 * src_width + x1];

            float top = p00 * (1.0f - dx) + p01 * dx;
            float bottom = p10 * (1.0f - dx) + p11 * dx;

            float value = top * (1.0f - dy) + bottom * dy;

            resized_img[y * kNumCols + x] = (uint8_t)(value + 0.5f);
        }
    }

    return resized_img;
}

esp_err_t img_handler(httpd_req_t *req){
    camera_fb_t *fb = NULL;
    esp_err_t res = ESP_OK;
    size_t fb_len = 0;
    int64_t fr_start = esp_timer_get_time();

    fb = esp_camera_fb_get();
    if (!fb) {
        ESP_LOGE(CAMERA_TAG, "Camera capture failed");
        httpd_resp_send_500(req);
        return ESP_FAIL;
    }
    res = httpd_resp_set_type(req, "image/jpeg");

    if(res == ESP_OK){
        res = httpd_resp_set_hdr(req, "Content-Disposition", "inline; filename=capture.jpg");
    }

    if(res == ESP_OK){
        if(fb->format == PIXFORMAT_JPEG){
            fb_len = fb->len;
            res = httpd_resp_send(req, (const char *)fb->buf, fb->len);
        } else {
            jpg_chunking_t jchunk = {req, 0};
            res = frame2jpg_cb(fb, 100, jpg_encode_stream, &jchunk) ? ESP_OK : ESP_FAIL;
            httpd_resp_send_chunk(req, NULL, 0);
            fb_len = jchunk.len;
        }
    }

    esp_camera_fb_return(fb);
    int64_t fr_end = esp_timer_get_time();
    ESP_LOGI(CAMERA_TAG, "JPG: %uKB %ums", (uint32_t)(fb_len/1024), (uint32_t)((fr_end - fr_start)/1000));

    copy_img_to_tensor(resize_img(fb), tf_interpreter->input(0));

    if (kTfLiteOk != tf_interpreter->Invoke()) {
        MicroPrintf("Invoke failed.");
    } else {
        int best_index = 0;
        uint8_t best_value = tf_interpreter->output(0)->data.uint8[0];

        for (int i = 1; i < kCategoryCount; i++) {
            uint8_t value = tf_interpreter->output(0)->data.uint8[i];

            if (value > best_value) {
                best_value = value;
                best_index = i;
            }
        }

        float probability = (static_cast<int>(best_value) - tf_interpreter->output(0)->params.zero_point) * tf_interpreter->output(0)->params.scale;

        MicroPrintf(
            "%c: %.3f", kCategoryLabels[best_index], std::clamp(probability, 0.0f, 1.0f)
        );

    }

    return res;
}