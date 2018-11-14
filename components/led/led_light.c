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
#include "esp_audio_log.h"
#include "led.h"
#include "userconfig.h"


#define LED_TAG "LED_LIGHT"

typedef struct {
    uint32_t period;  // unit Hz
    uint32_t duty;    // unit 0~100,off time
} LedFreq;

typedef struct {
    led_work_mode_t    state;
    LedFreq             freq;
} LedWorkFreq;

typedef enum {
    PwmChannel0 = 0,
    PwmChannel1,
    PwmChannel2,
    PwmChannel3,
    PwmChannel4,
    PwmChannel5,
    PwmChannel6,
    PwmChannel7,
} PwmChannel;

typedef struct {
    PwmChannel    ch;
    ledc_timer_t  timer;
    uint32_t      pin;
    led_work_mode_t  curtState;
} LedPwmPara;

// net is green led.
// sys is red led.
static LedWorkFreq arryLedFreq[] = {
    {LedWorkState_NetSetting,       {2, 40     }   },
    {LedWorkState_NetConnectOk,     {1, 100    }   },
    {LedWorkState_NetDisconnect,    {1, 80     }   },
    {0,                             {0,   0    }   },
};

LedPwmPara arryLedPara[] = {
#if (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRAT_V4_2_BOARD || defined CONFIG_ESP_LYRATD_FT_V1_0_BOARD || defined CONFIG_ESP_LYRATD_FT_DOSS_V1_0_BOARD)
    {PwmChannel0,   LEDC_TIMER_0,   GPIO_LED_GREEN,   0xFF},
#ifndef CONFIG_ESP_LYRAT_V4_3_BOARD
    {PwmChannel1,   LEDC_TIMER_1,   GPIO_LED_RED,   0xFF},
#endif // CONFIG_ESP_LYRAT_V4_3_BOARD
    {0xFF,          0xFF,        0xFF,                0xFF},
#endif // (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRAT_V4_2_BOARD)
};

esp_err_t LedLightSet(int num, led_work_mode_t state)
{
    if ((num > 1) || (state > LedWorkState_Unknown)) {
        ESP_AUDIO_LOGE(LED_TAG, "Para err.num=%d,state=%d", num, state);
        return -1;
    }
    if (arryLedPara[num].curtState != state) {
        uint32_t precision = 1 << LEDC_TIMER_12_BIT;
        uint32_t fre = arryLedFreq[state].freq.period;
        uint32_t duty = precision * arryLedFreq[state].freq.duty / 100;
        arryLedPara[num].curtState = state;

        ledc_timer_config_t timer_conf = {
#if IDF_3_0 == 1
            .duty_resolution = LEDC_TIMER_12_BIT,                //set timer counter bit number
#else
            .bit_num = LEDC_TIMER_12_BIT,                        //set timer counter bit number
#endif
            .freq_hz = fre,                                      //set frequency of pwm, here, 1000Hz
            .speed_mode = LEDC_HIGH_SPEED_MODE,                  //timer mode,
            .timer_num = arryLedPara[num].timer & 3,             //timer number
        };
        ledc_timer_config(&timer_conf);                          //setup timer.
        ledc_channel_config_t ledc_conf = {
            .channel = arryLedPara[num].ch,                      //set LEDC channel 0
            .duty = duty,                                        //set the duty for initialization.(duty range is 0 ~ ((2**bit_num)-1)
            .gpio_num = arryLedPara[num].pin,                    //GPIO number
            .intr_type = LEDC_INTR_DISABLE,                      //GPIO INTR TYPE, as an example, we enable fade_end interrupt here.
            .speed_mode = LEDC_HIGH_SPEED_MODE,                  //set LEDC mode, from ledc_mode_t
            .timer_sel = arryLedPara[num].timer & 3,             //set LEDC timer source, if different channel use one timer, the frequency and bit_num of these channels should be the same
        };
        ledc_channel_config(&ledc_conf);                         //setup the configuration
    }
    return ESP_OK;
}

esp_err_t LedLightInit(void)
{
#if (defined CONFIG_ESP_LYRAT_V4_3_BOARD || defined CONFIG_ESP_LYRAT_V4_2_BOARD || defined CONFIG_ESP_LYRATD_FT_V1_0_BOARD || defined CONFIG_ESP_LYRATD_FT_DOSS_V1_0_BOARD)
    gpio_config_t  io_conf;
    memset(&io_conf, 0, sizeof(io_conf));
#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    io_conf.pin_bit_mask     = BIT(GPIO_LED_GREEN);
#else
    io_conf.pin_bit_mask     = BIT(GPIO_LED_RED) | BIT(GPIO_LED_GREEN);
#endif
    io_conf.mode             = GPIO_MODE_OUTPUT;

#if IDF_3_0
    io_conf.intr_type = GPIO_INTR_DISABLE;
#else
    io_conf.intr_type = GPIO_PIN_INTR_DISABLE;
#endif
    gpio_config(&io_conf);
#ifdef CONFIG_ESP_LYRAT_V4_3_BOARD
    gpio_set_level(GPIO_LED_GREEN, 0x01); // green
#else
#if CONFIG_CLASSIC_BT_ENABLED
    gpio_set_level(GPIO_LED_RED, 0x01); // bt red
    gpio_set_level(GPIO_LED_GREEN, 0x00); // green
#else
    gpio_set_level(GPIO_LED_GREEN, 0x01); // wifi green
    gpio_set_level(GPIO_LED_RED, 0x00); // red
#endif /* CONFIG_CLASSIC_BT_ENABLED */
#endif /* CONFIG_ESP_LYRAT_V4_3_BOARD */
#endif /* CONFIG_ESP_LYRAT_V4_3_BOARD or CONFIG_ESP_LYRAT_V4_2_BOARD */
    return ESP_OK;
}

esp_err_t LedLightDeinit(void)
{
    // TODO
    return ESP_OK;
}
