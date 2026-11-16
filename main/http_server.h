#ifndef HTTP_SERVER_H_
#define HTTP_SERVER_H_

#include "sdkconfig.h"

constexpr const char *HTTP_TAG = "http_server";

// HTTP SERVER SETTINGS
// #define HTTP_PORT    80
#define HTTP_PORT    CONFIG_HTTP_PORT

void start_http_server();

#endif