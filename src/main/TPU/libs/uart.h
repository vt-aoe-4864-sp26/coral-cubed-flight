// uart_task.h
#pragma once

#include <cstdint>
#include <cstddef>

extern "C"
{
#include "tab.h"
}

extern volatile uint8_t g_run_inference;
extern volatile uint16_t g_last_inference_msg_id;
extern volatile uint16_t g_fetch_inference_msg_id;


void StartUartTask();
void SendInferenceResult(uint8_t *result_data, size_t len);