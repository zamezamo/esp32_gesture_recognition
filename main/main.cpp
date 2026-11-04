#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "camera.h"
#include "wifi.h"
#include "http_server.h"

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

    start_http_server();
}