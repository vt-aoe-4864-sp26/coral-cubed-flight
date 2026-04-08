// coral_main.cc
// Primary application file for running the Coral Micro/Pico Payload Board
//
// Written by Jack Rathert
// Other contributors: None
//
// See the top-level LICENSE file for the license.

// Standard libraries
#include <cstdio>

// coralmicro libraries
#include "libs/base/console_m7.h"
#include "libs/base/led.h"
#include "libs/base/tasks.h"
#include "libs/tensorflow/utils.h"

// local apps
#include "uart.h"
#include "inference.h"

// ===== CONFIG ===== //
constexpr char kModelPath[] = "/models/ssd_mobilenet_v2_face_quant_postprocess_edgetpu.tflite";
constexpr char kImagePath[] = "/images/denby.rgb";
constexpr int kTensorArenaSize = 8 * 1024 * 1024;

// ===== APPS ===== //

extern "C" [[noreturn]] void app_main(void *param)
{
  (void)param;
  printf("Coral Cubed Payload Initializing...\r\n");

  // Turn on Status LED to show the board is powered
  coralmicro::LedSet(coralmicro::Led::kStatus, true);

  // Spin up the TAB Protocol UART Task
  StartUartTask();

  // The main task can now become your primary control loop, or
  // you can spawn a separate InferenceTask here.
  while (true)
  {
    // Example: If an inference task finishes, you would call:
    // uint8_t mock_results[] = {0xDE, 0xAD, 0xBE, 0xEF};
    // SendInferenceResult(mock_results, sizeof(mock_results));

    vTaskDelay(pdMS_TO_TICKS(1000)); // Sleep for 1 second
  }
}


