/*
  * ESPRESSIF MIT License
  *
  * Copyright (c) 2017 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
  *
  * Permission is hereby granted for use on ESPRESSIF SYSTEMS products only, in which case,
  * it is free of charge, to any person obtaining a copy of this software and associated
  * documentation files (the "Software"), to deal in the Software without restriction, including
  * without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
  * and/or sell copies of the Software, and to permit persons to whom the Software is furnished
  * to do so, subject to the following conditions:
  *
  * The above copyright notice and this permission notice shall be included in all copies or
  * substantial portions of the Software.
  *
  * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  * FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  * COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  * IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  *
  */

#define IS31FL3216_TEST_CODE 1
#if IS31FL3216_TEST_CODE

#include <stdio.h>
#include "driver/i2c.h"
#include "Is31fl3216.h"
#include "i2c_bus.h"

Is31fl3216Handle handle;


void Is31fl3216_test_task(void *pvParameters)
{
    int ret;
    uint8_t duty_data[16] = {0};
    int j = 1;
    printf("Disable all led\n");
    Is31fl3216ChDisable(handle, IS31FL3216_CH_ALL);
    j = 1;
    for (int i = 0; i < 16; ++i) {
        Is31fl3216ChDutySet(handle, j << i, 128);
    }
    j = 1;
    printf("Enable one by one\n");
    for (int i = 0; i < 16; ++i) {
        Is31fl3216ChEnable(handle, j << i);
        vTaskDelay(500 / portTICK_RATE_MS);
    }
    vTaskDelay(1000 / portTICK_RATE_MS);
    printf("Disable one by one\n");
    j = 0x8000;
    for (int i = 0; i < 16; ++i) {
        Is31fl3216ChDisable(handle, j >> i);
        vTaskDelay(500 / portTICK_RATE_MS);
    }
    while (1) {
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    vTaskDelete(NULL);
}

void is31f13xxx_test()
{
    handle = Is31fl3216Initialize();
    Is31fl3216_test_task(NULL);
}
#endif

