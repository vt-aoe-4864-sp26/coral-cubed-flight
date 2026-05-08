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
  // Ensure the results directory exists
  coralmicro::LfsMakeDirs("/results");

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
        printf("Running inference on image: %s (saving as '%s')\r\n", img_path, g_inference_name);
        if (runner.RunInferenceFromLfs(img_path))
        {
          printf("Inference successful.\r\n");

          auto results = runner.GetDetectionResults();
          printf("Found %d detection(s).\r\n", (int)results.size());
          const char* res_str = "";
          size_t res_len = 0;

          if (results.size() > 0)
          {
            if (infer_type == 2) { res_str = "DENBY?!!"; res_len = 8; }
            else { res_str = "DENBY!"; res_len = 6; }
          }
          else
          {
            if (infer_type == 2) { res_str = "NO_DENBY!"; res_len = 9; }
            else { res_str = "NO_DENBY?!"; res_len = 10; }
          }
          
          char filepath[64];
          snprintf(filepath, sizeof(filepath), "/results/%.8s.txt", g_inference_name);
          printf("Saving result '%s' to %s\r\n", res_str, filepath);
          if (coralmicro::LfsWriteFile(filepath, (const uint8_t*)res_str, res_len)) {
            printf("File write OK.\r\n");
          } else {
            printf("ERROR: Failed to write result file!\r\n");
          }
        }
        else {
          printf("ERROR: Inference failed!\r\n");
        }
      }
      else {
        printf("ERROR: Model runner invalid!\r\n");
      }
    }

    if (g_fetch_name[0] != '\0')
    {
      char name_copy[9];
      strncpy(name_copy, g_fetch_name, 9);
      g_fetch_name[0] = '\0'; // clear flag

      char filepath[64];
      snprintf(filepath, sizeof(filepath), "/results/%.8s.txt", name_copy);
      printf("Fetching result from %s\r\n", filepath);
      
      std::vector<uint8_t> result_data;
      if (coralmicro::LfsReadFile(filepath, &result_data))
      {
          printf("File read OK. Content: %.*s\r\n", (int)result_data.size(), result_data.data());
          char out_buf[256];
          int len = snprintf(out_buf, sizeof(out_buf), "%.8s: %.*s", name_copy, (int)result_data.size(), result_data.data());
          SendInferenceResult((uint8_t*)out_buf, len);
      }
      else
      {
          printf("ERROR: Result file not found!\r\n");
          char out_buf[256];
          int len = snprintf(out_buf, sizeof(out_buf), "%.8s: NOT_FOUND", name_copy);
          SendInferenceResult((uint8_t*)out_buf, len);
      }
    }

    if (g_clear_results > 0)
    {
      g_clear_results = 0;
      printf("Clearing all stored inference results...\r\n");
      lfs_dir_t dir;
      struct lfs_info info;
      if (lfs_dir_open(coralmicro::Lfs(), &dir, "/results") >= 0) {
        while (lfs_dir_read(coralmicro::Lfs(), &dir, &info) > 0) {
          if (info.type == LFS_TYPE_REG) {
            char filepath[64];
            snprintf(filepath, sizeof(filepath), "/results/%s", info.name);
            lfs_remove(coralmicro::Lfs(), filepath);
            printf("Deleted: %s\r\n", filepath);
          }
        }
        lfs_dir_close(coralmicro::Lfs(), &dir);
      }
      printf("Results cleared.\r\n");
    }

    if (g_list_results > 0)
    {
      g_list_results = 0;
      printf("Listing results...\r\n");
      lfs_dir_t dir;
      struct lfs_info info;
      char list_buf[512] = {0};
      int list_len = 0;
      if (lfs_dir_open(coralmicro::Lfs(), &dir, "/results") >= 0) {
        while (lfs_dir_read(coralmicro::Lfs(), &dir, &info) > 0) {
          if (info.type == LFS_TYPE_REG) {
            // Strip the .txt extension to get back the 8-char name
            char name[9] = {0};
            strncpy(name, info.name, 8);
            int written = snprintf(list_buf + list_len, sizeof(list_buf) - list_len, "%s\n", name);
            if (written > 0) list_len += written;
            printf("  Found: %s\r\n", name);
          }
        }
        lfs_dir_close(coralmicro::Lfs(), &dir);
      }
      if (list_len == 0) {
        strncpy(list_buf, "EMPTY", sizeof(list_buf));
        list_len = 5;
      }
      SendInferenceResult((uint8_t*)list_buf, list_len);
    }

    vTaskDelay(pdMS_TO_TICKS(100)); // Sleep for 100ms
  }
}