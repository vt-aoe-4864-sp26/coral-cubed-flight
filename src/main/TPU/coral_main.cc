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
#include "libs/base/filesystem.h"
#include "libs/tensorflow/utils.h"
#include <vector>

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
          const char* res_str = "";
          size_t res_len = 0;

          if (results.size() > 0)
          {
            if (infer_type == 2) { res_str = "DENBY??!"; res_len = 8; }
            else { res_str = "DENBY!"; res_len = 6; }
          }
          else
          {
            if (infer_type == 2) { res_str = "NO_DENBY!"; res_len = 9; }
            else { res_str = "NO_DENBY?!"; res_len = 10; }
          }
          
          char filepath[64];
          snprintf(filepath, sizeof(filepath), "/inference_%d.txt", g_last_inference_msg_id);
          coralmicro::LfsWriteFile(filepath, (const uint8_t*)res_str, res_len);
        }
      }
    }

    if (g_fetch_inference_msg_id > 0)
    {
      uint16_t fetch_id = g_fetch_inference_msg_id;
      g_fetch_inference_msg_id = 0;
      
      char filepath[64];
      snprintf(filepath, sizeof(filepath), "/inference_%d.txt", fetch_id);
      
      std::vector<uint8_t> result_data;
      if (coralmicro::LfsReadFile(filepath, &result_data))
      {
          char out_buf[256];
          int len = snprintf(out_buf, sizeof(out_buf), "ID %d: %.*s", fetch_id, (int)result_data.size(), result_data.data());
          SendInferenceResult((uint8_t*)out_buf, len);
      }
      else
      {
          char out_buf[256];
          int len = snprintf(out_buf, sizeof(out_buf), "ID %d: NOT_FOUND", fetch_id);
          SendInferenceResult((uint8_t*)out_buf, len);
      }
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Sleep for 100ms
  }
}