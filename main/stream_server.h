#ifndef STREAM_SERVER_H_
#define STREAM_SERVER_H_

#include "sdkconfig.h"

constexpr const char *STREAM_TAG = "stream_server";

// STREAM SERVER SETTINGS
// #define STREAM_PORT    81
#define STREAM_PORT    CONFIG_HTTP_PORT + 1

void start_stream_server();

#endif