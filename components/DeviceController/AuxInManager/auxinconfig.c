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

#include "auxinconfig.h"
#include "userconfig.h"
#include "esp_audio_log.h"

#define TIME                1000
#define AUXIN_TAG           "AUXIN_CONFIG"

static xTimerHandle TimerRec;
static xTimerHandle TimerMode;
static xTimerHandle TimerAux;

static void timer_cb(TimerHandle_t xTimer)
{
    if (xTimer == TimerRec)
        ets_printf("TimerRec Timer Callback\n");
    else if (xTimer == TimerMode)
        ets_printf("TimerMode Timer Callback\n");
    else if (xTimer == TimerAux)
        ets_printf("TimerMode Timer Callback\n");
}

static int timer_init()
{
    TimerRec = xTimerCreate("timer0", TIME / portTICK_RATE_MS, pdFALSE, (void *) 0, timer_cb);
    TimerMode = xTimerCreate("timer1", TIME / portTICK_RATE_MS, pdFALSE, (void *) 0, timer_cb);
    TimerAux = xTimerCreate("timer1", TIME / portTICK_RATE_MS, pdFALSE, (void *) 0, timer_cb);
    if (!TimerRec || !TimerMode || !TimerAux) {
        ESP_AUDIO_LOGE(AUXIN_TAG, "timer create err\n");
        return -1;
    }
    xTimerStart(TimerRec, TIME / portTICK_RATE_MS);
    xTimerStart(TimerMode, TIME / portTICK_RATE_MS);
    xTimerStart(TimerAux, TIME / portTICK_RATE_MS);

    return 0;
}

static void timer_delete()
{
    xTimerDelete(TimerRec, TIME / portTICK_RATE_MS);
    TimerRec = NULL;
    xTimerDelete(TimerMode, TIME / portTICK_RATE_MS);
    TimerMode = NULL;
    xTimerDelete(TimerAux, TIME / portTICK_RATE_MS);
    TimerAux = NULL;
}

static void IRAM_ATTR auxin_gpio_intr_handler(void *arg)
{
    xQueueHandle queue = (xQueueHandle) arg;
    int gpioNum = 1;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (queue)
        xQueueSendFromISR(queue, &gpioNum, &xHigherPriorityTaskWoken);
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}

int auxin_status_detect()
{
    vTaskDelay(1000 / portTICK_RATE_MS);
    int ret = 0;
#if (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRAT_V4_2_BOARD)
    ret = gpio_get_level(GPIO_AUXIN_DETECT);
#endif
    return ret;
}

void auxin_intr_init(xQueueHandle queue)
{
#if (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRAT_V4_2_BOARD)
    gpio_config_t  io_conf = {0};
    //Aux_in
#if IDF_3_0
    io_conf.intr_type = GPIO_INTR_ANYEDGE;
#else
    io_conf.intr_type = GPIO_PIN_INTR_ANYEGDE;
#endif
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = BIT(GPIO_AUXIN_DETECT);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;
    gpio_config(&io_conf);
    gpio_isr_handler_add(GPIO_AUXIN_DETECT, auxin_gpio_intr_handler, (void *) queue);
#endif
}

