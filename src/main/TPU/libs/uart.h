// uart_task.h
#pragma once

#include <cstdint>
#include <cstddef>

extern "C"
{
#include "tab.h"
}

extern volatile uint8_t g_run_inference;


void StartUartTask();
void SendInferenceResult(uint8_t *result_data, size_t len);