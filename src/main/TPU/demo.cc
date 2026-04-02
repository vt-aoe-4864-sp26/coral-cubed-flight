// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cstdio>
#include "libs/base/console_m7.h"
#include "libs/base/led.h"
#include "libs/base/tasks.h"

// Reads character input from the serial console.
// Send '1' to turn the user LED on, '0' to turn it off.
//
// To build and flash from coralmicro root:
//    bash build.sh
//    python3 scripts/flashtool.py -e console_read_write

extern "C" [[noreturn]] void app_main(void* param) {
  (void)param;
  printf("Console Read Write Example!\r\n");

  // Turn on Status LED to show the board is on.
  LedSet(coralmicro::Led::kStatus, true);

  printf("Send '1' to turn user LED on, '0' to turn it off.\r\n");

  char ch;
  while (true) {
    int bytes = coralmicro::ConsoleM7::GetSingleton()->Read(&ch, 1);
    if (bytes == 1) {
      coralmicro::ConsoleM7::GetSingleton()->Write(&ch, 1);  // echo back

      if (ch == '1') {
        LedSet(coralmicro::Led::kUser, true);
        printf("User LED ON\r\n");
      } else if (ch == '0') {
        LedSet(coralmicro::Led::kUser, false);
        printf("User LED OFF\r\n");
      }
    }
    taskYIELD();
  }
}