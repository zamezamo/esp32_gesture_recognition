#include "camera.h"

#include "esp_err.h"
#include "esp_camera.h"

void init_camera(){
    static camera_config_t camera_config;
    camera_config.pin_pwdn  = PWDN_GPIO_NUM;
    camera_config.pin_reset = RESET_GPIO_NUM;
    camera_config.pin_xclk = XCLK_GPIO_NUM;
    camera_config.pin_sccb_sda = SIOD_GPIO_NUM;
    camera_config.pin_sccb_scl = SIOC_GPIO_NUM;

    camera_config.pin_d7 = Y9_GPIO_NUM;
    camera_config.pin_d6 = Y8_GPIO_NUM;
    camera_config.pin_d5 = Y7_GPIO_NUM;
    camera_config.pin_d4 = Y6_GPIO_NUM;
    camera_config.pin_d3 = Y5_GPIO_NUM;
    camera_config.pin_d2 = Y4_GPIO_NUM;
    camera_config.pin_d1 = Y3_GPIO_NUM;
    camera_config.pin_d0 = Y2_GPIO_NUM;
    camera_config.pin_vsync = VSYNC_GPIO_NUM;
    camera_config.pin_href = HREF_GPIO_NUM;
    camera_config.pin_pclk = PCLK_GPIO_NUM;

    camera_config.xclk_freq_hz = 20000000;
    camera_config.ledc_timer = LEDC_TIMER_0;
    camera_config.ledc_channel = LEDC_CHANNEL_0;

    camera_config.pixel_format = PIXFORMAT_GRAYSCALE;
    camera_config.frame_size = FRAMESIZE_128X128;

    camera_config.grab_mode = CAMERA_GRAB_LATEST;
    camera_config.fb_location = CAMERA_FB_IN_PSRAM;
    camera_config.jpeg_quality = 10;
    camera_config.fb_count = 2;

    //initialize the camera
    esp_err_t ret = esp_camera_init(&camera_config);
    ESP_ERROR_CHECK(ret);

    sensor_t* s = esp_camera_sensor_get();
    s->set_lenc(s, false);
    s->set_whitebal(s, true);
    s->set_wb_mode(s, false);
    s->set_denoise(s, true);
    s->set_gain_ctrl(s, true);
    s->set_gainceiling(s, GAINCEILING_2X);
    s->set_exposure_ctrl(s, true);
    s->set_aec2(s, true);
    s->set_wpc(s, true);
    s->set_raw_gma(s, true);
}