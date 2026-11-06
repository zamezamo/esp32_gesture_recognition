#ifndef WIFI_H_
#define WIFI_H_

constexpr const char *WIFI_TAG = "wifi_ap";

// WIFI CREDENTIALS AND SETTINGS
#define WIFI_SSID                       "esp32"
#define WIFI_PASS                       "12345678"
#define WIFI_CONN_MAX_RETRIES           5
#define WIFI_SCAN_AUTH_MODE_THRESHOLD   WIFI_AUTH_WPA2_PSK

void init_nvs();
void wifi_init_sta();

#endif