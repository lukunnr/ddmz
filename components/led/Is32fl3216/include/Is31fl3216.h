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

#ifndef _IOT_IS31FL3216_H_
#define _IOT_IS31FL3216_H_

#include "driver/i2c.h"
#include "i2c_bus.h"

#define IS31FL3216_CH_NUM_MAX 16
#define IS31FL3216_ADDRESS 0xE8                 /*!< I2C Addr*/

typedef enum {
    IS31FL3216_PWR_NORMAL = 0,       /**< Normal operation */
    IS31FL3216_PWR_SHUTDOWN,        /**< Software shutdown mode */
    IS31FL3216_PWR_MAX,
} Is31fl3216Pwr;

typedef enum {
    IS31FL3216_MODE_PWM = 0,          /**< PWM control mode operation*/
    IS31FL3216_MODE_AUTO_FRAME,       /**< Auto frame play mode operation */
    IS31FL3216_MODE_FRAME,            /**< Audio frame mode operation */
    IS31FL3216_MODE_MAX,
} Is31fl3216WorkMode;

typedef enum {
    IS31FL3216_CUR_MODE_REXT = 0,      /**< Output current is set by register */
    IS31FL3216_CUR_MODE_AUDIO,        /**< Output current is modulated by audio signal */
    IS31FL3216_CUR_MODE_MAX,
} Is31fl3216CurMode;

typedef enum {
    IS31FL3216_CUR_1_00 = 0,      /**< Output Current Selection */
    IS31FL3216_CUR_0_75,
    IS31FL3216_CUR_0_50,
    IS31FL3216_CUR_0_25,
    IS31FL3216_CUR_2_00,
    IS31FL3216_CUR_1_75,
    IS31FL3216_CUR_1_50,
    IS31FL3216_CUR_1_25,
    IS31FL3216_CUR_MAX,
} Is31fl3216CurValue;

typedef enum {
    IS31FL3216_CASCADE_MASTER = 0,      /**< Chip Cascade Mode */
    IS31FL3216_CASCADE_SLAVE,
} Is31fl3216CascadeMode;

typedef enum {
    IS31FL3216_AGS_0DB = 0,      /**< Audio Gain Selection */
    IS31FL3216_AGS_3DB,
    IS31FL3216_AGS_6DB,
    IS31FL3216_AGS_9DB,
    IS31FL3216_AGS_12DB,
    IS31FL3216_AGS_15DB,
    IS31FL3216_AGS_18DB,
    IS31FL3216_AGS_21DB,
    IS31FL3216_AGS_MAX,
} Is31fl3216AgsValue;

typedef enum {
    IS31FL3216_TIME_32MS = 0,      /**< Frame Delay Time */
    IS31FL3216_TIME_64MS,
    IS31FL3216_TIME_128MS,
    IS31FL3216_TIME_256MS,
    IS31FL3216_TIME_512MS,
    IS31FL3216_TIME_1024MS,
    IS31FL3216_TIME_2048MS,
    IS31FL3216_TIME_4096MS,
    IS31FL3216_TIME_MAX,
} Is31fl3216DelayTime;


typedef enum {
    IS31FL3216_REG_CONFIG = 0x00,       /* Configuration register */
    IS31FL3216_REG_LED_CTRL_H = 0x01,   /* LED control register OUT9-OUT16 enable bit */
    IS31FL3216_REG_LED_CTRL_L = 0x02,   /* LED control register OUT1-OUT8 enable bit */
    IS31FL3216_REG_LED_EFFECT = 0x03,   /* Set the output current and the audio gain */
    IS31FL3216_REG_CH_CONFIG = 0x04,    /* Set the operating mode of OUT9~OUT16 */
    IS31FL3216_REG_GPIO_CONFIG = 0x05,  /* Set the operating mode of OUT9~OUT16 as the GPIO port */
    IS31FL3216_REG_OUTPUT = 0x06,       /* Set the logic level of OUT9~OUT16 as the output port */
    IS31FL3216_REG_INPUT_CTRL = 0x07,   /* Set the interrupt function of OUT9~OUT16 */
    IS31FL3216_REG_STATE = 0x08,        /* Store the state of OUT9~OUT16 as the input port */
    IS31FL3216_REG_ADC_RATE = 0x09,     /* Set the ADC sample rate of the input signal */

    IS31FL3216_REG_PWM_16 = 0x10,       /* Set the PWM duty cycle data */
    IS31FL3216_REG_PWM_15,
    IS31FL3216_REG_PWM_14,
    IS31FL3216_REG_PWM_13,
    IS31FL3216_REG_PWM_12,
    IS31FL3216_REG_PWM_11,
    IS31FL3216_REG_PWM_10,
    IS31FL3216_REG_PWM_09,
    IS31FL3216_REG_PWM_08,
    IS31FL3216_REG_PWM_07,
    IS31FL3216_REG_PWM_06,
    IS31FL3216_REG_PWM_05,
    IS31FL3216_REG_PWM_04,
    IS31FL3216_REG_PWM_03,
    IS31FL3216_REG_PWM_02,
    IS31FL3216_REG_PWM_01,

    IS31FL3216_REG_FRAME1_CTRL = 0x20, /* Store the data of 8 frames */
    IS31FL3216_REG_FRAME1_PWM = 0x22,
    IS31FL3216_REG_FRAME2_CTRL = 0x32,
    IS31FL3216_REG_FRAME2_PWM = 0x34,
    IS31FL3216_REG_FRAME3_CTRL = 0x44,
    IS31FL3216_REG_FRAME3_PWM = 0x46,
    IS31FL3216_REG_FRAME4_CTRL = 0x56,
    IS31FL3216_REG_FRAME4_PWM = 0x58,
    IS31FL3216_REG_FRAME5_CTRL = 0x68,
    IS31FL3216_REG_FRAME5_PWM = 0x6A,
    IS31FL3216_REG_FRAME6_CTRL = 0x7A,
    IS31FL3216_REG_FRAME6_PWM = 0x7C,
    IS31FL3216_REG_FRAME7_CTRL = 0x8C,
    IS31FL3216_REG_FRAME7_PWM = 0x8E,
    IS31FL3216_REG_FRAME8_CTRL = 0x9E,
    IS31FL3216_REG_FRAME8_PWM = 0xA0,

    IS31FL3216_REG_UPDATE = 0xB0,       /* Load PWM Register data */
    IS31FL3216_REG_FRAME_DELAY = 0xB6,  /* Set the delay time between each frame */
    IS31FL3216_REG_FRAME_START = 0xB7,  /* Set the start frame in Auto Frame Play Mode */
    IS31FL3216_REG_MAX,
} Is31fl3216Reg;

typedef enum {
    IS31FL3216_CH_1 = 0x0001,      /**< channel by bit shit */
    IS31FL3216_CH_2 = 0x0002,
    IS31FL3216_CH_3 = 0x0004,
    IS31FL3216_CH_4 = 0x0008,
    IS31FL3216_CH_5 = 0x0010,
    IS31FL3216_CH_6 = 0x0020,
    IS31FL3216_CH_7 = 0x0040,
    IS31FL3216_CH_8 = 0x0080,
    IS31FL3216_CH_9 = 0x0100,
    IS31FL3216_CH_10 = 0x0200,
    IS31FL3216_CH_11 = 0x0400,
    IS31FL3216_CH_12 = 0x0800,
    IS31FL3216_CH_13 = 0x1000,
    IS31FL3216_CH_14 = 0x2000,
    IS31FL3216_CH_15 = 0x4000,
    IS31FL3216_CH_16 = 0x8000,
    IS31FL3216_CH_ALL = 0xFFFF,
} Is31PwmChannel;

typedef void *Is31fl3216Handle;

/**
 * @brief The Shutdown Register sets software shutdown mode of IS31FL3216.
 * @param handle  led dev handle
 * @param mode  shutdown mode or Normal
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 *     - ESP_ERR_TIMEOUT  timeout
 */
esp_err_t Is31fl3216Power(Is31fl3216Handle handle, Is31fl3216Pwr mode);

/**
 * @brief IS31FL3216 will reset all registers to default value.
 * @param handle  led dev handle
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL error
 *     - ESP_ERR_TIMEOUT  timeout
 */
esp_err_t Is31fl3216ChDutySet(Is31fl3216Handle handle, Is31PwmChannel chBits, uint8_t duty);

esp_err_t Is31fl3216WorkModeSet(Is31fl3216Handle handle, Is31fl3216WorkMode mode);
esp_err_t Is31fl3216ChEnable(Is31fl3216Handle handle, Is31PwmChannel chBits);
esp_err_t Is31fl3216ChDisable(Is31fl3216Handle handle, Is31PwmChannel chBits);
esp_err_t Is31fl3216CurModeSet(Is31fl3216Handle handle, Is31fl3216CurMode mode);
esp_err_t Is31fl3216CurValueSet(Is31fl3216Handle handle, Is31fl3216CurValue value);
esp_err_t Is31fl3216AgsValueSet(Is31fl3216Handle handle, Is31fl3216AgsValue value);
esp_err_t Is31fl3216AgcCfg(Is31fl3216Handle handle, uint32_t en);
esp_err_t Is31fl3216CascadeModeSet(Is31fl3216Handle handle, Is31fl3216CascadeMode mode);
esp_err_t Is31fl3216UpdateReg(Is31fl3216Handle handle);
esp_err_t Is31fl3216SampleRateSet(Is31fl3216Handle handle, uint32_t value);

esp_err_t Is31fl3216FrameTimeSet(Is31fl3216Handle handle, Is31fl3216DelayTime time);
esp_err_t Is31fl3216FirstFrameSet(Is31fl3216Handle handle, uint32_t frame);
esp_err_t Is31fl3216FrameValueSet(Is31fl3216Handle handle, uint32_t num, uint8_t *data, uint32_t len);

esp_err_t Is31fl3216Reset(Is31fl3216Handle handle);

/**
 * @brief Create and init sensor object and return a led handle
 *
 * @param bus I2C bus object handle
 * @return
 *     - NULL Fail
 *     - Others Success
 */
Is31fl3216Handle Is31fl3216Initialize(void);

/**
 * @brief Delete and release a LED object
 * @param sensor object handle of Is31fl3216
 * @param del_bus Whether to delete the I2C bus
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Fail
 */
esp_err_t Is31fl3216Finalize(Is31fl3216Handle handle);
#endif

