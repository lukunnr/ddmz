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
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "driver/adc.h"
#include "esp_system.h"
#include "esp_adc_cal.h"
#include "string.h"
#include "AdcButton.h"
#include "sdkconfig.h"

#define V_REF   1100
#define ADC1_TEST_CHANNEL (ADC1_CHANNEL_3)      //GPIO 34
//#define V_REF_TO_GPIO  //Remove comment on define to route v_ref to GPIO

#define ADC_SAMPLES_NUM 10
#define CLICK_TIME_THRESHOLD 6 // unit is 50*6ms



//uint32_t baseVccVoltage = 3080; //mv
uint32_t baseVccVoltage = 3106; //mv

uint16_t stepLevel[2][7] = { { 0, 14, 27, 42, 55, 68, 99 },
                            {0, 220, 384, 525, 673, 830, 990}};
                          //{ 0, 15, 30, 47, 67, 74, 99 }



typedef struct {
    int activeId;
    int clickCnt;
    int onlyClick;
} BtnDecription;

static BtnDecription btn[USER_KEY_MAX];

static TimerHandle_t releaseTmr;
static TimerHandle_t pressTmr;
static TimerHandle_t longPressTmr;
static int timerStartflag;
static int firstId = -1;
static ButtonCallback btnCallback;


int getAdcVoltage(int channle)
{
    uint32_t data[ADC_SAMPLES_NUM] = { 0 };
    uint32_t adc = 0;
    uint32_t sum = 0;
    int tmp = 0;
    esp_adc_cal_characteristics_t characteristics;
    esp_adc_cal_get_characteristics(V_REF, ADC_ATTEN_11db, ADC_WIDTH_12Bit,
                                    &characteristics);
    for (int i = 0; i < ADC_SAMPLES_NUM; ++i) {
        data[i] = adc1_to_voltage(channle, &characteristics);
    }
    for (int j = 0; j < ADC_SAMPLES_NUM - 1; j++) {
        for (int i = 0; i < ADC_SAMPLES_NUM - j - 1; i++) {
            if (data[i] > data[i + 1]) {
                tmp = data[i];
                data[i] = data[i + 1];
                data[i + 1] = tmp;
            }
        }
    }
    for (int num = 1; num < ADC_SAMPLES_NUM - 1; num++)
        sum += data[num];
    return (sum / (ADC_SAMPLES_NUM - 2));
}

int getButtonId(int adc)
{
    int m = -1;
    for (int i = 0; i < USER_KEY_MAX; ++i) {
        if ((adc > ((stepLevel[1][i] * baseVccVoltage) / 1000))
            && (adc <= ((stepLevel[1][i + 1] * baseVccVoltage) / 1000))) {
            m = i;
            break;
        }
    }
    return m;
}

void resetBtn(void)
{
    memset(btn, 0, sizeof(btn));
    for (int i = 0; i < USER_KEY_MAX; ++i) {
        btn[i].activeId = -1;
    }
}

void buttonReleaseCb(xTimerHandle tmr)
{
    BtnDecription *btn = (BtnDecription *) pvTimerGetTimerID(tmr);
    int actId = -1;
    for (int i = 0; i < 6; ++i) {
        if (btn[i].activeId > -1) {
            actId = i;
            break;
        }
    }
    // printf("RleaseTimer,ID:%d\r\n", actId);
    btn[actId].activeId = -1;
    btn[actId].clickCnt = 0;
    if (btnCallback) {
        btnCallback(actId, BTN_STATE_RELEASE);
    }
    firstId = -1;
    xTimerStop(longPressTmr, 0);
    xTimerStop(pressTmr, 0);
    timerStartflag = 0;
}

void buttonPressCb(xTimerHandle tmr)
{
    BtnDecription *btn = (BtnDecription *) pvTimerGetTimerID(tmr);
    int actId = -1;
    for (int i = 0; i < 6; ++i) {
        if (btn[i].activeId > -1) {
            actId = i;
            break;
        }
    }

    int id = getButtonId(getAdcVoltage(ADC1_CHANNEL_3));
    // printf("PressTimer,act:%d,adc:%d\r\n", actId,
    //        getAdcVoltage(ADC1_CHANNEL_3));
    if (id == actId) {
        btn[actId].clickCnt++;
        if (btn[actId].clickCnt > CLICK_TIME_THRESHOLD) {
            if (actId == USER_KEY_REC) {
                // it's a click
                xTimerStop(tmr, 0);
                xTimerStop(longPressTmr, 0);
                printf("PressTimer,ID:%d cnt:%d, Click\r\n", actId,
                       btn[actId].clickCnt);
                btn[actId].activeId = 0;
                btn[actId].clickCnt = 0;
                if (btnCallback) {
                    btnCallback(actId, BTN_STATE_CLICK);
                }
            } else {
                xTimerStop(tmr, 0);
                printf("PressTimer,ID:%d, will be long press\r\n", actId);
            }
        } else {
            // to do nothing
        }
    } else {
        if (btn[actId].clickCnt > 0) {
            // it's a click
            xTimerStop(tmr, 0);
            xTimerStop(longPressTmr, 0);
            printf("PressTimer,ID:%d cnt:%d, Click\r\n", actId,
                   btn[actId].clickCnt);
            btn[actId].activeId = 0;
            btn[actId].clickCnt = 0;
            if (btnCallback) {
                btnCallback(actId, BTN_STATE_CLICK);
            }
            //Maybe need to active the release timer for click release
        } else {
            // Invalid click
            xTimerStop(tmr, 0);
            xTimerStop(longPressTmr, 0);
            // printf("PressTimer,ID:%d, cnt:%d,invalid click\r\n", actId,
            // btn[actId].clickCnt);
            btn[actId].clickCnt = 0;
            btn[actId].activeId = 0;
            firstId = -1;
        }
    }
}

void buttonLongPressCb(xTimerHandle tmr)
{
    xTimerStop(tmr, 0);
    BtnDecription *btn = (BtnDecription *) pvTimerGetTimerID(tmr);
    int actId = -1;
    for (int i = 0; i < 6; ++i) {
        if (btn[i].activeId > -1) {
            actId = i;
            break;
        }
    }
    // printf("LongPressTimer,ID:%d\r\n", actId);
    int id = getButtonId(getAdcVoltage(ADC1_CHANNEL_3));
    if ((id == actId)) {
        printf("LongPressTimer,ID:%d, it's a long press\r\n", actId);
        btn[actId].clickCnt = 0;
        btn[actId].activeId = 0;
        if (btnCallback) {
            btnCallback(actId, BTN_STATE_PRESS);
        }
    } else {
        // Invalid click
        btn[actId].clickCnt = 0;
        btn[actId].activeId = 0;
        // printf("LongPressTimer,ID:%d, invalid long press\r\n", actId);
    }
}

void buttonTask(void *para)
{
    adc1_config_width(ADC_WIDTH_12Bit);
    adc1_config_channel_atten(ADC1_CHANNEL_3, ADC_ATTEN_11db);
    xQueueHandle queCheckId = xQueueCreate(2, sizeof(uint32_t));

    releaseTmr = xTimerCreate("release", 80 / portTICK_PERIOD_MS,
                              pdFALSE, &btn, buttonReleaseCb);
    pressTmr = xTimerCreate("press", 50 / portTICK_PERIOD_MS,
                            pdTRUE, &btn, buttonPressCb);
    longPressTmr = xTimerCreate("longpress", 3000 / portTICK_PERIOD_MS,
                                pdFALSE, &btn, buttonLongPressCb);
    printf("btn:%p\r\n", &btn);
    resetBtn();
    while (1) {
        // Read ADC and obtain result in mV
        int adc = getAdcVoltage(ADC1_CHANNEL_3);
        // printf("adc: %d\r\n", adc);
        int id = getButtonId(adc);
        if (-1 == id) {
            if (timerStartflag) {
                // printf("ID:%d,stop timer\r\n", firstId);
                xTimerStart(releaseTmr, 0);
                timerStartflag = 0;
            }
        } else {
            if (firstId != id) {
                firstId = id;
                btn[firstId].activeId = firstId;
                // printf("ID:start %d\r\n", firstId);
                xTimerStart(pressTmr, 0);
                if (firstId != USER_KEY_REC) {
                    xTimerStart(longPressTmr, 0);
                }

                timerStartflag = 1;
            } else {
                xTimerStop(releaseTmr, 0);
            }
            // printf("ID:%d, firstID:%d\r\n", id, firstId);
        }
        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void adc_btn_init(ButtonCallback cb)
{
    btnCallback = cb;
#ifndef CONFIG_MEMMAP_SMP
    xTaskCreate(buttonTask, "buttonTask", 2500, NULL, 10, NULL);
#else
    xTaskCreatePinnedToCore(buttonTask, "buttonTask", 2500, NULL, 10, NULL, 1);
#endif
}


void adc_btn_test(void)
{
    adc_btn_init(NULL);
}
