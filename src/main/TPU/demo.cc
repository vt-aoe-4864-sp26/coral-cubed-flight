// Copyright 2022 Google LLC
// (License omitted for brevity, keeping original license applies)

#include <vector>

#include "libs/base/filesystem.h"
#include "libs/base/gpio.h"      // ADDED: For GPIO control
#include "libs/base/led.h"
#include "libs/camera/camera.h"
#include "libs/tensorflow/detection.h"
#include "libs/tensorflow/utils.h"
#include "libs/tpu/edgetpu_manager.h"
#include "libs/tpu/edgetpu_op.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_error_reporter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_interpreter.h"
#include "third_party/tflite-micro/tensorflow/lite/micro/micro_mutable_op_resolver.h"

namespace coralmicro {
namespace {
constexpr char kModelPath[] =
    "/models/ssd_mobilenet_v2_face_quant_postprocess_edgetpu.tflite";
constexpr int kTopK = 5;
constexpr float kThreshold = 0.5;

// ADDED: Define your trigger pin. 
// kUart1Cts is commonly available on the side headers. 
// Check libs/base/gpio.h for the exact enum if you choose a different pin.
constexpr coralmicro::Gpio kTriggerPin = coralmicro::Gpio::kUartCts;

// An area of memory to use for input, output, and intermediate arrays.
constexpr int kTensorArenaSize = 8 * 1024 * 1024;
STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, kTensorArenaSize);

[[noreturn]] void Main() {
  printf("Hardware Triggered Face Detection Example!\r\n");
  
  // ADDED: Initialize the GPIO pin as an input
  GpioSetMode(kTriggerPin, GpioMode::kInputPullDown);

  // Keep Status LED off until triggered
  LedSet(Led::kStatus, false);

  std::vector<uint8_t> model;
  if (!LfsReadFile(kModelPath, &model)) {
    printf("ERROR: Failed to load %s\r\n", kModelPath);
    vTaskSuspend(nullptr);
  }

  auto tpu_context = EdgeTpuManager::GetSingleton()->OpenDevice();
  if (!tpu_context) {
    printf("ERROR: Failed to get EdgeTpu context\r\n");
    vTaskSuspend(nullptr);
  }

  tflite::MicroErrorReporter error_reporter;
  tflite::MicroMutableOpResolver<3> resolver;
  resolver.AddDequantize();
  resolver.AddDetectionPostprocess();
  resolver.AddCustom(kCustomOp, RegisterCustomOp());

  tflite::MicroInterpreter interpreter(tflite::GetModel(model.data()), resolver,
                                      tensor_arena, kTensorArenaSize,
                                      &error_reporter);
  if (interpreter.AllocateTensors() != kTfLiteOk) {
    printf("ERROR: AllocateTensors() failed\r\n");
    vTaskSuspend(nullptr);
  }

  if (interpreter.inputs().size() != 1) {
    printf("ERROR: Model must have only one input tensor\r\n");
    vTaskSuspend(nullptr);
  }

  auto* input_tensor = interpreter.input_tensor(0);
  int model_height = input_tensor->dims->data[1];
  int model_width = input_tensor->dims->data[2];

  // ADDED: State tracker for our run loop
  bool is_running = false; 

  while (true) {
    // ADDED: Read the hardware pin state (High = true, Low = false)

    bool pin_state = GpioGet(kTriggerPin);

    // ADD THIS: Print the pin state to the serial console every 10 loops (1 second)
    static int debug_counter = 0;
    if (debug_counter++ % 10 == 0) {
      printf("DEBUG: Trigger pin state is currently: %s\r\n", pin_state ? "HIGH (3.3V)" : "LOW (0V)");
    }

    // STATE TRANSITION: LOW -> HIGH (Start)
    if (pin_state && !is_running) {
      printf("Trigger HIGH: Powering up camera and starting inference...\r\n");
      LedSet(Led::kStatus, true);
      CameraTask::GetSingleton()->SetPower(true);
      CameraTask::GetSingleton()->Enable(CameraMode::kStreaming);
      is_running = true;
    }
    // STATE TRANSITION: HIGH -> LOW (Stop)
    else if (!pin_state && is_running) {
      printf("Trigger LOW: Stopping inference and idling...\r\n");
      LedSet(Led::kStatus, false);
      LedSet(Led::kUser, false); // Clear face detect LED just in case
      CameraTask::GetSingleton()->Disable();
      CameraTask::GetSingleton()->SetPower(false);
      is_running = false;
    }

    // MAIN BEHAVIOR
    if (is_running) {
      CameraFrameFormat fmt{CameraFormat::kRgb,
                            CameraFilterMethod::kBilinear,
                            CameraRotation::k270,
                            model_width,
                            model_height,
                            false,
                            tflite::GetTensorData<uint8_t>(input_tensor)};
      
      if (!CameraTask::GetSingleton()->GetFrame({fmt})) {
        printf("Failed to capture image\r\n");
        vTaskSuspend(nullptr);
      }

      if (interpreter.Invoke() != kTfLiteOk) {
        printf("Failed to invoke\r\n");
        vTaskSuspend(nullptr);
      }

      if (auto results =
              tensorflow::GetDetectionResults(&interpreter, kThreshold, kTopK);
          !results.empty()) {
        printf("Found %d face(s):\r\n%s\r\n", results.size(),
               tensorflow::FormatDetectionOutput(results).c_str());
        LedSet(Led::kUser, true);
      } else {
        LedSet(Led::kUser, false);
      }
    } else {
      // ADDED: Sleep to yield to FreeRTOS and save power while idling
      vTaskDelay(pdMS_TO_TICKS(100));
    }
  }
}

}  // namespace
}  // namespace coralmicro

extern "C" void app_main(void* param) {
  (void)param;
  coralmicro::Main();
}