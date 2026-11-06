#include "hand_gesture_model.h"
#include "hand_gesture_model_int8.h"

#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/micro/micro_log.h"
#include "tensorflow/lite/micro/micro_mutable_op_resolver.h"
#include "tensorflow/lite/micro/system_setup.h"

#include "esp_heap_caps.h"

static uint8_t *tensor_arena = nullptr;

const tflite::Model* model = nullptr;
tflite::MicroInterpreter* tf_interpreter = nullptr;

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

}
