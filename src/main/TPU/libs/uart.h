// uart_task.h
#pragma once

#include <cstdint>
#include <cstddef>

extern "C"
{
#include "tab.h"
}

extern volatile uint8_t g_run_inference;
extern char g_inference_name[9];  // 8-char name + null terminator
extern char g_fetch_name[9];      // 8-char name + null terminator
extern volatile uint8_t g_clear_results;
extern volatile uint8_t g_list_results;


void StartUartTask();
void SendInferenceResult(uint8_t *result_data, size_t len);