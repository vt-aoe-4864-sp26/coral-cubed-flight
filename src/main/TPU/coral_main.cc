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
  printf("Coral Cubed Payload Initializing...\r\n");

  coralmicro::LedSet(coralmicro::Led::kStatus, true);

  // Store the context here to keep the Edge TPU awake!
  auto tpu_context = coral_cubed::InitEdgeTpu();
  if (!tpu_context)
  {
    // printf("TPU Not Initialized...\r\n");
  }

  StartUartTask();
  printf("TPU Initialized\r\n");

  g_run_inference = 0;

  while (true)
  {
    if (g_run_inference > 0)
    {
      uint8_t infer_type = g_run_inference;
      g_run_inference = 0;

      coral_cubed::ModelRunner runner(coral_cubed::kModelPath, tensor_arena, coral_cubed::kTensorArenaSize);

      if (runner.IsValid())
      {
        const char* img_path = (infer_type == 2) ? coral_cubed::kImageBlkPath : coral_cubed::kImageDenbyPath;
        if (runner.RunInferenceFromLfs(img_path))
        {

          auto results = runner.GetDetectionResults();

          // printf("Inference OK! Found %d face(s).\r\n", results.size());
          // Loop through and print the results using basic C-types (no std::string!)
          if (results.size() > 0)
          {
            if (infer_type == 2) SendInferenceResult((uint8_t *)"DENBY??!", 8);
            else SendInferenceResult((uint8_t *)"DENBY!", 6);
          }
          else
          {
            if (infer_type == 2) SendInferenceResult((uint8_t *)"NO_DENBY!", 9);
            else SendInferenceResult((uint8_t *)"NO_DENBY?!", 10);
          }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Sleep for 100ms
  }
}