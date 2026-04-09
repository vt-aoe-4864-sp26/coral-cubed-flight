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
#include "libs/uart.h"
#include "libs/inference.h"
#include "configs.h"

// ===== APPS ===== //

STATIC_TENSOR_ARENA_IN_SDRAM(tensor_arena, coral_cubed::kTensorArenaSize);

extern "C" [[noreturn]] void app_main(void *param)
{
  (void)param;
  //printf("Coral Cubed Payload Initializing...\r\n");

  coralmicro::LedSet(coralmicro::Led::kStatus, true);

  // Store the context here to keep the Edge TPU awake!
  auto tpu_context = coral_cubed::InitEdgeTpu();
  if (!tpu_context)
  {
    //printf("TPU Not Initialized...\r\n");
  }

  StartUartTask();

  while (true)
  {
    if (g_run_inference)
    {
      g_run_inference = false;
      //printf("Running Demo Inference...\r\n");

      coral_cubed::ModelRunner runner(coral_cubed::kModelPath, tensor_arena, coral_cubed::kTensorArenaSize);

      if (runner.IsValid())
      {
        if (runner.RunInferenceFromLfs(coral_cubed::kImagePath))
        {
          auto results = runner.GetDetectionResults();
          //printf("%s\r\n", coralmicro::tensorflow::FormatDetectionOutput(results).c_str());
          SendInferenceResult((uint8_t *)"INFER_OK", 8);
          
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Sleep for 100ms
  }
}