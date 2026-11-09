#include "hand_gesture_model.h"
#include "hand_gesture_model_int8.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"

#include "esp_heap_caps.h"
#include "esp_camera.h"

static uint8_t *tensor_arena = nullptr;

const tflite::Model* model = nullptr;
static tflite::TfLiteTensor* input = nullptr;

static tflite::MicroInterpreter* tf_interpreter = nullptr;

const char kCategoryLabels[kCategoryCount] = {
    'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'K', 'L', 'M', 'N', 'O', 'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y'
};

void init_hand_gesture_model(){
    model = tflite::GetModel(hand_gesture_model);

    if (model->version() != TFLITE_SCHEMA_VERSION) {
        MicroPrintf(
            "Model provided is schema version %d not equal "
            "to supported version %d.",
            model->version(), TFLITE_SCHEMA_VERSION
        );
        return;
    }

    tensor_arena = (uint8_t *)heap_caps_malloc(
        kTensorArenaSize,
        MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT
    );

    if (!tensor_arena) {
        MicroPrintf("Failed to allocate tensor arena");
        return;
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
        return;
    }

    input = tf_interpreter->input(0);

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

void predict_hand_gesture(camera_fb_t* fb){
    if (!input){
        MicroPrintf("Failed initialize hand gesture model inputs.");
        return;
    }

    copy_inputs_to_tensor(minimize_img(fb), input);

    if (kTfLiteOk != tf_interpreter->Invoke()) {
        MicroPrintf("Invoke failed.");
        return;
    }

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
