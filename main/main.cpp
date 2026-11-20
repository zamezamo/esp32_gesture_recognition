#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "camera.h"
#include "wifi.h"
#include "http_server.h"
#include "stream_server.h"
#include "hand_gesture_model.h"
#include "http_handlers.h"

// #define CORE0       0
// // only define xCoreID CORE1 as 1 if this is a multiple core processor target, else define it as tskNO_AFFINITY
// #define CORE1       ((CONFIG_FREERTOS_NUMBER_OF_CORES > 1) ? 1 : tskNO_AFFINITY)

// #define TASK_PRIORITY_HIGH 3

extern "C" void app_main(void)
{
    init_camera();

    vTaskDelay(pdMS_TO_TICKS(1000));

    init_nvs();
    wifi_init_sta();

    init_hand_gesture_task();
    init_recognize_http_worker();

    start_stream_server();
    start_http_server();
}