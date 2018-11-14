/*
 * ESPRESSIF MIT License
 *
 * Copyright (c) 2018 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>
 *
 * Permission is hereby granted for use on all ESPRESSIF SYSTEMS products, in which case,
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

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "EspAudioAlloc.h"
#include "led.h"

// XXX static variable is not good.
static led_config_t *my_Led = NULL;
led_handle_t LedInit(led_config_t *cfg)
{
    if (cfg) {
        led_config_t *led = EspAudioAlloc(1, sizeof(led_config_t));
        if (led == NULL) {
            return NULL;
        }
        led->led_init = cfg->led_init;
        led->set = cfg->set;
        led->led_deinit = cfg->led_deinit;
        led->led_init();
        my_Led = led;
        return led;
    }
    return NULL;

}
esp_err_t LedIndicatorSet(int index, led_work_mode_t state)
{
    // if (handle) {
        led_config_t *led = (led_config_t *)my_Led;
        esp_err_t ret = led->set(index, state);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
    // }
    return ESP_FAIL;
}

esp_err_t LedDeinit(led_handle_t handle)
{
    if (handle) {
        free(handle);
        return ESP_OK;
    }
    return ESP_FAIL;
}
