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

#include <stdio.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/gpio.h"
#include "esp_types.h"
#include "esp_audio_log.h"

#include "userconfig.h"
#include "MediaHal.h"

#define TIME                500
#define AUXIN_TAG           "HEADPHONE_CONFIG"

static xTimerHandle TimerHeadPhone;

#if (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
static void hp_timer_cb(TimerHandle_t xTimer)
{
    int res;
    res = gpio_get_level(GPIO_HEADPHONE_DETECT);
    ets_printf("TimerHeadPhone Timer Callback: %d\n", res);
    MediaHalPaPwr(res);
}

static int hp_timer_init()
{
    TimerHeadPhone = xTimerCreate("hp_timer0", TIME / portTICK_RATE_MS, pdFALSE, (void *) 0, hp_timer_cb);
    if (TimerHeadPhone == NULL) {
        ESP_AUDIO_LOGE(AUXIN_TAG, "hp_timer create err\n");
        return -1;
    }
    xTimerStart(TimerHeadPhone, TIME / portTICK_RATE_MS);

    return 0;
}

static void hp_timer_delete()
{
    xTimerDelete(TimerHeadPhone, TIME / portTICK_RATE_MS);
    TimerHeadPhone = NULL;
}


static void IRAM_ATTR headphone_gpio_intr_handler(void *arg)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    xTimerResetFromISR(TimerHeadPhone, &xHigherPriorityTaskWoken);
    if ( xHigherPriorityTaskWoken != pdFALSE ) {
        portYIELD_FROM_ISR();
    }
}

int headphone_status_detect()
{
    return gpio_get_level(GPIO_HEADPHONE_DETECT);
}

#endif /* (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRATD_MSC_V2_2_BOARD) */

void headphone_intr_init()
{
    ESP_AUDIO_LOGE(AUXIN_TAG, "headphone_intr_init create 1\n");
#if (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRATD_MSC_V2_2_BOARD)
    gpio_config_t io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    //Aux_in
#if IDF_3_0
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
#else
    io_conf.intr_type = GPIO_PIN_INTR_ANYEGDE;
#endif
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = GPIO_HEADPHONE_DETECT_SEL;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    hp_timer_init();
    gpio_isr_handler_add(GPIO_HEADPHONE_DETECT, headphone_gpio_intr_handler, NULL);
    ESP_AUDIO_LOGE(AUXIN_TAG, "headphone_intr_init create 2\n");
#endif/* (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRATD_MSC_V2_2_BOARD) */
}
