#ifndef MAIN_H
#define MAIN_H

// HTTP SERVER SETTINGS
#define HTTP_PORT    80


// WIFI CREDENTIALS AND SETTINGS
#define WIFI_SSID                       "GPON-2.4G-0e343b"
#define WIFI_PASS                       "8c0e343b"
#define WIFI_CONN_MAX_RETRIES           5
#define WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK


// OV2640 CAMERA PINS
#define PWDN_GPIO_NUM       32
#define RESET_GPIO_NUM      -1
#define XCLK_GPIO_NUM       0
#define PCLK_GPIO_NUM       22

#define SIOD_GPIO_NUM       26
#define SIOC_GPIO_NUM       27

#define Y9_GPIO_NUM         35
#define Y8_GPIO_NUM         34
#define Y7_GPIO_NUM         39
#define Y6_GPIO_NUM         36
#define Y5_GPIO_NUM         21
#define Y4_GPIO_NUM         19
#define Y3_GPIO_NUM         18
#define Y2_GPIO_NUM         5

#define VSYNC_GPIO_NUM      25
#define HREF_GPIO_NUM       23

#define LED_GPIO_NUM        4


#endif