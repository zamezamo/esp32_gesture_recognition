#include "hand_gesture_model.h"
#include "hand_gesture_model_int8.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"

#include "esp_heap_caps.h"
#include "esp_camera.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <atomic>

static uint8_t *tensor_arena = nullptr;

const tflite::Model* model = nullptr;
static tflite::TfLiteTensor* input = nullptr;

static tflite::MicroInterpreter* tf_interpreter = nullptr;

static QueueHandle_t task_hand_gesture_queue = nullptr;

static TaskHandle_t task_hand_gesture_handle = nullptr;
static std::atomic<bool> task_hand_gesture_handle_ready{false};

static constexpr uint32_t kInferenceStackBytes = 12 * 1024;

const char kCategoryLabels[kCategoryCount] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y'
};

static bool init_hand_gesture_model(){
    model = tflite::GetModel(hand_gesture_model);

    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf(
            "Model provided is schema version %d not equal to supported version %d",
            model->version(), TFLITE_SCHEMA_VERSION
        );
        return false;
    }

    tensor_arena = (uint8_t *)heap_caps_malloc(
        kTensorArenaSize,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!tensor_arena) {
        MicroPrintf("Failed to allocate tensor arena");
        return false;
    }

    static tflite::MicroMutableOpResolver<11> micro_op_resolver;
    micro_op_resolver.AddConv2D(tflite::Register_CONV_2D_INT8());
    micro_op_resolver.AddMaxPool2D(tflite::Register_MAX_POOL_2D_INT8());
    micro_op_resolver.AddSoftmax(tflite::Register_SOFTMAX_INT8());
    micro_op_resolver.AddFullyConnected(tflite::Register_FULLY_CONNECTED_INT8());
    micro_op_resolver.AddMul();
    micro_op_resolver.AddAdd();
    micro_op_resolver.AddShape();
    micro_op_resolver.AddPack();
    micro_op_resolver.AddStridedSlice();
    micro_op_resolver.AddReshape();
    micro_op_resolver.AddQuantize();

    static tflite::MicroInterpreter static_interpreter(
        model, micro_op_resolver, tensor_arena, kTensorArenaSize
    );
    tf_interpreter = &static_interpreter;

    TfLiteStatus allocate_status = tf_interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        MicroPrintf("AllocateTensors() failed");
        return false;
    }

    input = tf_interpreter->input(0);

    if (!input || input->type != kTfLiteUInt8 || input->bytes != kMaxImageSize) {
        MicroPrintf("Unexpected input tensor (expected uint8 28x28x1)");
        return false;
    }

    MicroPrintf("Model ready, arena used: %u bytes", (unsigned)tf_interpreter->arena_used_bytes());
             
    return true;
}

// using bilinear interpolation to minimize hand img from camera for model inputs
uint8_t* minimize_img(camera_fb_t *fb){
    static uint8_t minimized_img[kMaxImageSize];

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

            minimized_img[y * kNumCols + x] = (uint8_t)(value + 0.5f);
        }
    }

    return minimized_img;
}

void copy_inputs_to_tensor(const uint8_t *image_minimized, TfLiteTensor* input){
    const float scale = input->params.scale;
    const int zero_point = input->params.zero_point;

    for (int i = 0; i < kMaxImageSize; i++){
        float pixel = image_minimized[i] / 255.0f;
        int value = (int)roundf(pixel / scale) + zero_point;

        value = std::clamp(value, 0, 255);
        input->data.uint8[i] = (uint8_t)value;
    }
}

static void predict_hand_gesture(camera_fb_t* fb){

    copy_inputs_to_tensor(minimize_img(fb), input);

    if (kTfLiteOk != tf_interpreter->Invoke()) {
        MicroPrintf("Invoke failed.");
        return;
    }

    TfLiteTensor *output = tf_interpreter->output(0);

    int best_index = 0;
    uint8_t best_value = output->data.uint8[0];

    for (int i = 1; i < kCategoryCount; i++) {
        uint8_t value = output->data.uint8[i];

        if (value > best_value) {
            best_value = value;
            best_index = i;
        }
    }

    float probability = (static_cast<int>(best_value) - output->params.zero_point) * output->params.scale;

    MicroPrintf(
        "%c: %.3f", kCategoryLabels[best_index], std::clamp(probability, 0.0f, 1.0f)
    );

}

static void hand_gesture_task(void*) {

    ESP_LOGI(TFLITE_TAG, "Hand gesture recognizition task on core %d", xPortGetCoreID());

    if (!init_hand_gesture_model()) {
        ESP_LOGE(TFLITE_TAG, "Model initialization failed, task disabled");

        task_hand_gesture_handle = nullptr;
        vTaskDelete(nullptr);

        return;
    }

    task_hand_gesture_handle_ready.store(true, std::memory_order_release);

    camera_fb_t *fb = nullptr;

    for (;;) {
        if (xQueueReceive(task_hand_gesture_queue, &fb, portMAX_DELAY) == pdTRUE) {
            predict_hand_gesture(fb);
        }
    }
}

void init_hand_gesture_task() {
#if CONFIG_FREERTOS_NUMBER_OF_CORES < 2
    ESP_LOGE(TFLITE_TAG, "Dual-core FreeRTOS is required");
    return;
#endif

    task_hand_gesture_queue = xQueueCreate(1, sizeof(camera_fb_t *));
    if (!task_hand_gesture_queue) {
        ESP_LOGE(TFLITE_TAG, "Creating hand gesture task queue failed");

        return;
    }

    BaseType_t created = xTaskCreatePinnedToCore(
        hand_gesture_task,
        "gesture_inference",
        kInferenceStackBytes,
        nullptr,
        2,
        &task_hand_gesture_handle,
        1
    );
    if (created != pdPASS) {
        vQueueDelete(task_hand_gesture_queue);
        task_hand_gesture_queue = nullptr;

        ESP_LOGE(TFLITE_TAG, "Creating hand gesture task failed");

        return;
    }
}

void get_hand_gesture_frame(const camera_fb_t* fb) {
    if (!task_hand_gesture_queue || !task_hand_gesture_handle_ready.load(std::memory_order_acquire)) {
        ESP_LOGE(TFLITE_TAG, "Hang gesture task queue not ready");

        return;
    }

    if (xQueueOverwrite(task_hand_gesture_queue, &fb) != pdTRUE){
        ESP_LOGE(TFLITE_TAG, "Error adding hand frame to queue");

        return;
    }
}
