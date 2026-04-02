// Copyright 2022 Google LLC

#include <stdio.h> 
#include <vector>

#include "libs/base/filesystem.h"
#include "libs/base/led.h"
#include "libs/base/console_m7.h" // <--- THE SILVER BULLET
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

constexpr int kTensorArenaSize = 8 * 1024 * 1024;
STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, kTensorArenaSize);

volatile bool g_run_inference = false;

// ==============================================================
// ERROR PANIC ROUTINE
// ==============================================================
[[noreturn]] void Panic(const char* message) {
    printf("\r\nFATAL ERROR: %s\r\n", message);
    while (true) {
        // Rapidly flash the green User LED to indicate a dead boot
        LedSet(Led::kUser, true);
        vTaskDelay(pdMS_TO_TICKS(100));
        LedSet(Led::kUser, false);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

// ==============================================================
// CONSOLE M7 LISTENER TASK
// ==============================================================
void UartListenerTask(void* param) {
    printf("ConsoleM7 Listener Task Started. Awaiting CDH commands...\r\n");
    
    uint8_t state = 0;
    char c;
    
    while (true) {
        // Read safely from the FreeRTOS-managed console buffer!
        if (ConsoleM7::GetSingleton()->Read(&c, 1) == 1) {
            
            // Tab.c Payload State Machine: [OPCODE] -> [VAR_CODE] -> [LEN] -> [ENABLE]
            if (state == 0 && c == 0x02) { state = 1; }      // COMMON_DATA_OPCODE
            else if (state == 1 && c == 0x0A) { state = 2; } // VAR_CODE_CORAL_INFER
            else if (state == 2 && c == 0x01) { state = 3; } // Length
            else if (state == 3) {
                if (c == 0x01) { 
                    printf("\r\n>>> COMMAND RX: START INFERENCE <<<\r\n");
                    g_run_inference = true; 
                } 
                else if (c == 0x02) { 
                    printf("\r\n>>> COMMAND RX: STOP INFERENCE <<<\r\n");
                    g_run_inference = false; 
                }
                state = 0; 
            } else {
                state = (c == 0x02) ? 1 : 0;
            }
        } else {
            // Yield to FreeRTOS scheduler if no bytes are on the wire
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}

[[noreturn]] void Main() {
  printf("EdgeTPU Hardware Interface Booting...\r\n");

  // Spin up the listener
  xTaskCreate(UartListenerTask, "UartListener", configMINIMAL_STACK_SIZE * 4, nullptr, 3, nullptr);

  LedSet(Led::kStatus, false);
  LedSet(Led::kUser, false);

  // 1. Check File System
  std::vector<uint8_t> model;
  if (!LfsReadFile(kModelPath, &model)) {
    Panic("Failed to load .tflite model. Check CMakeLists.txt DATA flag!");
  }

  // 2. Boot EdgeTPU
  auto tpu_context = EdgeTpuManager::GetSingleton()->OpenDevice();
  if (!tpu_context) {
    Panic("Failed to get EdgeTpu context");
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
    Panic("AllocateTensors() failed");
  }

  if (interpreter.inputs().size() != 1) {
    Panic("Model must have only one input tensor");
  }

  auto* input_tensor = interpreter.input_tensor(0);
  int model_height = input_tensor->dims->data[1];
  int model_width = input_tensor->dims->data[2];

  bool is_running = false; 

  printf("Boot complete. Awaiting CDH UART Commands.\r\n");

  while (true) {
    bool trigger_state = g_run_inference;

    // STATE: STOP -> START
    if (trigger_state && !is_running) {
      LedSet(Led::kStatus, true);
      CameraTask::GetSingleton()->SetPower(true);
      CameraTask::GetSingleton()->Enable(CameraMode::kStreaming);
      is_running = true;
    }
    // STATE: START -> STOP
    else if (!trigger_state && is_running) {
      LedSet(Led::kStatus, false);
      LedSet(Led::kUser, false); 
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
        printf("Camera GetFrame failed!\r\n");
        vTaskDelay(pdMS_TO_TICKS(50));
        continue; // Don't crash, just try again next frame
      }

      if (interpreter.Invoke() != kTfLiteOk) {
        Panic("EdgeTPU Invoke() failed!");
      }

      if (auto results = tensorflow::GetDetectionResults(&interpreter, kThreshold, kTopK); !results.empty()) {
        LedSet(Led::kUser, true);
      } else {
        LedSet(Led::kUser, false);
      }
    } else {
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