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
#include "driver/gpio.h"
#include "driver/ledc.h"

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/timers.h"
#include "esp_types.h"
#include "esp_log.h"
#include "led.h"
#include "userconfig.h"
#include "Is31fl3216.h"
#include "board.h"

#define LED_TAG "LED_BAR"
#define ONE_FRAME_BYTE_SIZE 18

typedef struct {
    uint32_t period;  // unit ms
    uint32_t duty;    // unit 0~100% on time
} LedFreq;

typedef struct {
    led_work_mode_t     state;
    LedFreq             freq;
} LedWorkFreq;

typedef enum {
    LED_FRAME_NUM_1,
    LED_FRAME_NUM_2,
    LED_FRAME_NUM_3,
    LED_FRAME_NUM_4,
    LED_FRAME_NUM_5,
    LED_FRAME_NUM_6,
    LED_FRAME_NUM_7,
    LED_FRAME_NUM_8,
    LED_FRAME_NUM_MAX,
} LedFrameNum;

static LedWorkFreq arryLedFreq[] = {
    {LedWorkState_NetSetting,       {1000, 80     }   },
    {LedWorkState_NetConnectOk,     {1000, 100    }   },
    {LedWorkState_NetDisconnect,    {1000, 50     }   },
    {0,                             {0,    0      }   },
};

static uint8_t SysWifiGreenLed[ONE_FRAME_BYTE_SIZE] = {0x20, 0x00,
                                                       0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                                      };
static uint8_t SysBtRedLed[ONE_FRAME_BYTE_SIZE] = {0x10, 0x00,
                                                   0x00, 0x00, 0x00, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                                  };

static uint8_t SysWakeup_On[ONE_FRAME_BYTE_SIZE] = {0x0F, 0xFF,
                                                    0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
                                                   };
static uint8_t SysWakeup_Off[ONE_FRAME_BYTE_SIZE] = {0x0F, 0xFF,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                                    };

static uint8_t LedFrameClear[ONE_FRAME_BYTE_SIZE] = {0x00, 0x00,
                                                     0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
                                                    };
static Is31fl3216Handle led;

uint8_t LedFrames[8][ONE_FRAME_BYTE_SIZE] = {
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
};

// 0-1: enable bit for LED 16-LED 1
// 2-17: PWM value for LED 16- LED 1
static const uint8_t LightAudioFrames[8][ONE_FRAME_BYTE_SIZE] = {
    {0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF},
    {0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0x3F, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0x3F, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
    {0xFF, 0x3F, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF},
};

static void fillIndicatorFrames( void *dst, void *src, int period, int onDuty)
{
    short dst_sw = *(short *)dst;
    int src_sw = *(int *)src;
    ESP_LOGI(LED_TAG, "Indicator, dst_sw:%04x, src_sw:%08x", dst_sw, src_sw);
    if ((src_sw == 0x10) || (src_sw == 0x20)) {
        dst_sw = (dst_sw & 0xFFF0);
    }
    dst_sw |= src_sw;

    int m = 0;
    if (onDuty < 10) {
        m = 0;
    } else if ((onDuty >= 10) && (onDuty < 20)) {
        m = 1;
    } else if ((onDuty >= 20) && (onDuty < 30)) {
        m = 2;
    } else if ((onDuty >= 30) && (onDuty < 50)) {
        m = 3;
    } else if ((onDuty >= 50) && (onDuty < 60)) {
        m = 4;
    } else if ((onDuty >= 60) && (onDuty < 70)) {
        m = 5;
    } else if ((onDuty >= 70) && (onDuty < 80)) {
        m = 6;
    } else if ((onDuty >= 80) && (onDuty < 90)) {
        m = 7;
    } else if ((onDuty >= 90) && (onDuty <= 100)) {
        m = 8;
    } else {
        // TODO nothing
    }
    if ((src_sw == 0x10) || (src_sw == 0x20)) {
        // For indicators, Green led and red led.
        for (int i = 0; i < m; ++i) {
            memcpy(&LedFrames[i], &dst_sw, sizeof(dst_sw));
            memcpy(&LedFrames[i][2], &src[2], 4);
        }
        for (; m < 8; m++) {
            memset(&LedFrames[m][2], 0, 16);
        }
    } else {
        // 12 led for lightbar
        for (int i = 0; i < m; ++i) {
            memcpy(&LedFrames[i], &dst_sw, sizeof(dst_sw));
            memcpy(&LedFrames[i][6], &src[6], (ONE_FRAME_BYTE_SIZE - 6));
        }
    }
#if 0
    // For debug
    for (int i = 0; i < 8; ++i) {
        printf("\n2-");
        for (int n = 0; n < 18; ++n) {
            printf("%02x ", LedFrames[i][n]);
        }
    }
    printf("\n\n\n");
#endif
}

static void generateOneFrame(uint8_t *dst, Is31PwmChannel chBits)
{
    uint16_t chCtrl = 0;
    if (NULL == dst) {
        return;
    }
    for (int i = 0; i < IS31FL3216_CH_NUM_MAX; ++i) {
        if ((chBits >> i) & 0x01) {
            dst[IS31FL3216_CH_NUM_MAX + 2 - i - 1] = 0xFF;
        }
    }
    dst[0] = chBits >> 8;
    dst[1] = chBits;
    printf("\r\n");
    for (int i = 0; i < 18; ++i) {
        printf("%02x ", dst[i]);
    }
    printf("\r\n");
}


esp_err_t LedBarSet(int num, led_work_mode_t state)
{
    if ((num > 1) || (state >= LedWorkState_Unknown)) {
        ESP_LOGE(LED_TAG, "Para err.num=%d,state=%d", num, state);
        return ESP_FAIL;
    }
    // Enter into Auto frame mode
    Is31fl3216Reset(led);
    Is31fl3216WorkModeSet(led, IS31FL3216_MODE_AUTO_FRAME);

    // Configure delay time
    if (LedWorkState_NetSetting == state) {
        Is31fl3216FrameTimeSet(led, IS31FL3216_TIME_64MS);
    } else if (LedWorkState_NetConnectOk == state) {
        Is31fl3216FrameTimeSet(led, IS31FL3216_TIME_128MS);
    } else if (LedWorkState_NetDisconnect == state) {
        Is31fl3216FrameTimeSet(led, IS31FL3216_TIME_32MS);
    }
    if (num) {
        fillIndicatorFrames(LedFrames, SysBtRedLed, arryLedFreq[state].freq.period, arryLedFreq[state].freq.duty);
    } else {
        fillIndicatorFrames(LedFrames, SysWifiGreenLed, arryLedFreq[state].freq.period, arryLedFreq[state].freq.duty);
    }
    Is31fl3216FrameValueSet(led, 1, &LedFrames, sizeof(LedFrames));
    Is31fl3216FirstFrameSet(led, 0);
    return ESP_OK;
}

void LedIndicatorWakeup(int on)
{
    if (on > 1) {
        ESP_LOGE(LED_TAG, "Para err. %d", on);
        return;
    }
    // Enter into Auto frame mode
    Is31fl3216Reset(led);
    Is31fl3216WorkModeSet(led, IS31FL3216_MODE_AUTO_FRAME);
#if 0
    /* Not used now */
    Configure delay time for blink
    int state = LedWorkState_NetSetting;
if (LedWorkState_NetSetting == state) {
        Is31fl3216FrameTimeSet(led, IS31FL3216_TIME_64MS);
    } else if (LedWorkState_NetConnectOk == state) {
        Is31fl3216FrameTimeSet(led, IS31FL3216_TIME_128MS);
    } else if (LedWorkState_NetDisconnect == state) {
        Is31fl3216FrameTimeSet(led, IS31FL3216_TIME_32MS);
    }
    fillIndicatorFrames(NULL, SysWakeup, 1000, 50);
    Is31fl3216FrameValueSet(led, 1, &LedFrames, sizeof(LedFrames));
    Is31fl3216FirstFrameSet(led, 0);
    vTaskDelay(1200 / portTICK_RATE_MS);
#endif

    if (on) {
        fillIndicatorFrames(LedFrames, SysWakeup_On, 1000, 100);
    } else {
        fillIndicatorFrames(LedFrames, SysWakeup_Off, 1000, 100);
    }
    Is31fl3216FrameValueSet(led, 1, &LedFrames, sizeof(LedFrames));
    Is31fl3216FirstFrameSet(led, 0);
}

void LedLightBarLToR(int time)
{
    Is31fl3216WorkModeSet(led, IS31FL3216_MODE_PWM);
    Is31fl3216ChDisable(led, IS31FL3216_CH_ALL);
    int j = 1;
    for (int i = 0; i < 12; ++i) {
        if (i == 6 || i == 7) {
            continue;
        }
        Is31fl3216ChDutySet(led, j << i, 128);
    }
    j = 1;
    for (int i = 0; i < 12; ++i) {
        if (i == 7 || i == 6) {
            continue;
        }
        Is31fl3216ChEnable(led, j << i);
        vTaskDelay(time / portTICK_RATE_MS);
    }
    j = 0x8000;
    for (int i = 0; i < 12; ++i) {
        if (i == 9 || i == 8) {
            continue;
        }
        Is31fl3216ChDisable(led, j >> i);
        vTaskDelay(time / portTICK_RATE_MS);
    }
}

void LedLightBarByAudio(void)
{
    Is31fl3216Reset(led);
    Is31fl3216WorkModeSet(led, IS31FL3216_MODE_FRAME);
    Is31fl3216SampleRateSet(led, 0xB4); // Set adc sample rate
    Is31fl3216FrameValueSet(led, 1, &LightAudioFrames, sizeof(LightAudioFrames));
    Is31fl3216FirstFrameSet(led, 0);
}

void testLightBar()
{
    Is31fl3216Reset(led);
    Is31fl3216WorkModeSet(led, IS31FL3216_MODE_AUTO_FRAME);
    Is31fl3216FrameTimeSet(led, IS31FL3216_TIME_512MS);
    uint8_t frame0[18] = {0};
    int k = 0;
    int m = 0;
    for (int i = 0; i < 120; ++i) {
        if (i == 12) {
            i = 0;
        }
        generateOneFrame(frame0, 1 << i);

        m = 1;
        Is31fl3216FrameValueSet(led, k++, &frame0, sizeof(frame0));
        memset(frame0, 0, sizeof(frame0));
        if (k == 8) {
            k = 0;
        }
        if (m == 0) {
            Is31fl3216FirstFrameSet(led, 0);
        }
        vTaskDelay(1000 / portTICK_RATE_MS);
    }
}

esp_err_t LedBarInit(void)
{
#if (defined CONFIG_ESP_LYRATD_MSC_V2_1_BOARD || defined CONFIG_ESP_LYRATD_MSC_V2_2_BOARD )
    gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
    io_conf.pin_bit_mask     =  BIT(MICRO_SEMI_RESET_CTRL) | BIT(BOARD_RESET_CTRL) | BIT(5);
    io_conf.mode             = GPIO_MODE_OUTPUT;
    io_conf.pull_up_en       = GPIO_PULLUP_DISABLE;
    io_conf.intr_type        = GPIO_PIN_INTR_DISABLE ;
    gpio_config(&io_conf);

    gpio_set_level(BOARD_RESET_CTRL, 0x00); // disable reset
    gpio_set_level(MICRO_SEMI_RESET_CTRL, 0x00); // reset microsemi dsp
    ets_delay_us(100);
    gpio_set_level(MICRO_SEMI_RESET_CTRL, 0x01); // reset microsemi dsp

    led =  Is31fl3216Initialize();
    // testLightBar();
    // LedLightBarLToR(20);
    // while (1) {
    //     LedIndicatorSet(LedIndexNet, LedWorkState_NetDisconnect);
    //     vTaskDelay(5000 / portTICK_RATE_MS);
    //     LedIndicatorSet(1, LedWorkState_NetDisconnect);
    //     vTaskDelay(5000 / portTICK_RATE_MS);
    //     LedIndicatorWakeup(1);
    //     vTaskDelay(5000 / portTICK_RATE_MS);
    //     LedIndicatorWakeup(0);
    //     vTaskDelay(5000 / portTICK_RATE_MS);
    // }
#endif
    return ESP_OK;
}

esp_err_t LedBarDeinit(void)
{
    // TODO
    return ESP_OK;
}
