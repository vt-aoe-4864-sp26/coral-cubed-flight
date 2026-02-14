#include <cstdio>

#include "libs/base/led.h"
#include "third_party/freertos_kernel/include/FreeRTOS.h"
#include "third_party/freertos_kernel/include/task.h"

extern "C" [[noreturn]] void app_main(void *param) {
  (void)param;
  // Turn on Status LED to show the board is on.
  LedSet(coralmicro::Led::kStatus, true);

  printf("Hello out-of-tree world!\r\n");
  vTaskSuspend(nullptr);
}