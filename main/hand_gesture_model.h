#ifndef HAND_GESTURE_MODEL_H_
#define HAND_GESTURE_MODEL_H_

#include "tensorflow/lite/micro/micro_interpreter.h"

constexpr const char *TFLITE_TAG = "tflite_model";

// TFLITE MODEL PARAMETERS
constexpr int kTensorArenaSize = 512 * 1024;

constexpr int kCategoryCount = 24;
constexpr int kNumCols = 28;
constexpr int kNumRows = 28;
constexpr int kNumChannels = 1;

constexpr int kMaxImageSize = kNumCols * kNumRows * kNumChannels;

extern const char kCategoryLabels[kCategoryCount];

extern tflite::MicroInterpreter* tf_interpreter;

void init_hand_gesture_model();
#endif