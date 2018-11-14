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

#include <stdio.h>
#include <string.h>
#include "driver/i2c.h"
#include "esp_log.h"
#include "Is31fl3216.h"
#include "i2c_bus.h"

char *IS31_TAG = "IS31";

#define IS31_ERROR_CHECK(con) if(!(con)) {ESP_LOGE(IS31_TAG,"err line: %d", __LINE__); return ESP_FAIL;}
#define IS31_PARAM_CHECK(con) if(!(con)) {ESP_LOGE(IS31_TAG,"Parameter error, "); return ESP_FAIL;}
#define IS31_CHECK_I2C_RES(res) if(ret == ESP_FAIL) {ESP_LOGE(IS31_TAG, "Is31fl3216[%s]: FAIL\n", __FUNCTION__);} \
                                else if(ret == ESP_ERR_TIMEOUT) {ESP_LOGE(IS31_TAG, "Is31fl3216[%s]: TIMEOUT\n", __FUNCTION__);}

#define IS31FL3216_WRITE_BIT    0x00
#define IS31FL3216_READ_BIT     0x01
#define IS31FL3216_ACK_CHECK_EN 1

#define I2C_MASTER_SCL_IO    23        /*!< gpio number for I2C master clock */
#define I2C_MASTER_SDA_IO    18        /*!< gpio number for I2C master data  */
#define I2C_MASTER_NUM     I2C_NUM_0   /*!< I2C port number for master dev */
#define I2C_MASTER_TX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_RX_BUF_DISABLE   0  /*!< I2C master do not need buffer */
#define I2C_MASTER_FREQ_HZ    100000   /*!< I2C master clock frequency */


typedef struct {
    i2c_bus_handle_t bus;
    uint16_t addr;
} Is31fl3216Dev;


uint8_t Is31Value[10] = {0x80, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};

/**
 * @brief set software shutdown mode
 */
static esp_err_t Is31fl3216WriteReg(Is31fl3216Handle handle, Is31fl3216Reg regAddr, uint8_t *data, uint8_t data_num)
{
    Is31fl3216Dev *led = (Is31fl3216Dev *) handle;
    IS31_PARAM_CHECK(NULL != data);
    int ret = i2c_bus_write_bytes(I2C_MASTER_NUM, IS31FL3216_ADDRESS | IS31FL3216_WRITE_BIT, &regAddr, 1, data, data_num);
    // i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    // i2c_master_start(cmd);
    // i2c_master_write_byte(cmd, IS31FL3216_ADDRESS | IS31FL3216_WRITE_BIT, IS31FL3216_ACK_CHECK_EN);
    // i2c_master_write_byte(cmd, regAddr, IS31FL3216_ACK_CHECK_EN);
    // i2c_master_write(cmd, data, data_num, IS31FL3216_ACK_CHECK_EN);
    // i2c_master_stop(cmd);
    // int ret = i2c_bus_cmd_begin(led->bus, cmd, 500 / portTICK_RATE_MS);
    // IS31_CHECK_I2C_RES(ret);
    // i2c_cmd_link_delete(cmd);
    return ret;
}


/**
 * @brief change channels PWM duty cycle data register
 */
static esp_err_t is31fl3218SChannelDutyByBits(Is31fl3216Handle handle, uint32_t byBits, uint8_t duty)
{
    int ret, i;
    for (i = 0; i < IS31FL3216_CH_NUM_MAX; i++) {
        if ((byBits >> i) & 0x1) {
            int ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_PWM_16 + (IS31FL3216_CH_NUM_MAX - i - 1), &duty, 1);
            if (ret == ESP_OK) {
                //PASS
            } else {
                IS31_CHECK_I2C_RES(ret);
                return ret;
            }
        }
    }
    return ESP_OK;
}

/**
 * @brief Load PWM Register and LED Control Registers data
 */
esp_err_t Is31fl3216UpdateReg(Is31fl3216Handle handle)
{
    IS31_PARAM_CHECK(NULL != handle);
    uint8_t m = 0;
    return Is31fl3216WriteReg(handle, IS31FL3216_REG_UPDATE, &m, 1);
}

/**
 * @brief set software shutdown mode
 */
esp_err_t Is31fl3216Power(Is31fl3216Handle handle, Is31fl3216Pwr mode)
{
    IS31_PARAM_CHECK(NULL != handle);
    if (IS31FL3216_PWR_SHUTDOWN == mode) {
        Is31Value[IS31FL3216_REG_CONFIG] = (Is31Value[IS31FL3216_REG_CONFIG] & (~(1 << 7))) | (1 << 7);
    } else if (IS31FL3216_PWR_NORMAL == mode) {
        Is31Value[IS31FL3216_REG_CONFIG] = (Is31Value[IS31FL3216_REG_CONFIG] & (~(1 << 7)));
    } else {
        return ESP_FAIL;
    }
    int ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_CONFIG, (uint8_t *) &Is31Value[IS31FL3216_REG_CONFIG], 1);
    return ret;
}

esp_err_t Is31fl3216WorkModeSet(Is31fl3216Handle handle, Is31fl3216WorkMode mode)
{
    IS31_PARAM_CHECK(NULL != handle);
    if (IS31FL3216_MODE_PWM == mode) {
        Is31Value[IS31FL3216_REG_CONFIG] = (Is31Value[IS31FL3216_REG_CONFIG] & (~(3 << 5)));
    } else if (IS31FL3216_MODE_AUTO_FRAME == mode) {
        Is31Value[IS31FL3216_REG_CONFIG] = (Is31Value[IS31FL3216_REG_CONFIG] & (~(3 << 5))) | (1 << 5);
    } else if (IS31FL3216_MODE_FRAME == mode) {
        Is31Value[IS31FL3216_REG_CONFIG] = (Is31Value[IS31FL3216_REG_CONFIG] & (~(3 << 5))) | (2 << 5);
    } else {
        return ESP_FAIL;
    }
    int ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_CONFIG, (uint8_t *) &Is31Value[IS31FL3216_REG_CONFIG], 1);
    return ret;
}


/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t Is31fl3216ChDutySet(Is31fl3216Handle handle, Is31PwmChannel chBits, uint8_t duty)
{
    esp_err_t ret;
    IS31_PARAM_CHECK(NULL != handle);
    ret = is31fl3218SChannelDutyByBits(handle, chBits, duty);
    if (ret != ESP_OK) {
        IS31_CHECK_I2C_RES(ret);
        return ret;
    }
    ret = Is31fl3216UpdateReg(handle);
    if (ret != ESP_OK) {
        IS31_CHECK_I2C_RES(ret);
        return ret;
    }
    return ESP_OK;
}

/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t Is31fl3216ChEnable(Is31fl3216Handle handle, Is31PwmChannel chBits)
{
    int ret = 0;
    IS31_PARAM_CHECK(NULL != handle);
    uint16_t value = 0;
    for (int i = 0; i < IS31FL3216_CH_NUM_MAX; ++i) {
        if ((chBits >> i) & 0x01) {
            value |= (1 << i);
        }
    }
    uint16_t *pValue = &Is31Value[IS31FL3216_REG_LED_CTRL_H];
    // ESP_LOGI(IS31_TAG, "%s,chBits=%x, value:%04x, p:%04x", __func__, chBits, value, *pValue);
    Is31Value[IS31FL3216_REG_LED_CTRL_H] |= value >> 8;
    Is31Value[IS31FL3216_REG_LED_CTRL_L] |= value;
    ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_LED_CTRL_H, &Is31Value[IS31FL3216_REG_LED_CTRL_H], 2);
    return ret;
}

/**
 * @brief change channels PWM duty cycle data register
 */
esp_err_t Is31fl3216ChDisable(Is31fl3216Handle handle, Is31PwmChannel chBits)
{
    int ret = 0;
    IS31_PARAM_CHECK(NULL != handle);
    uint16_t value = ((uint16_t)Is31Value[IS31FL3216_REG_LED_CTRL_H]) << 8;
    value |= Is31Value[IS31FL3216_REG_LED_CTRL_L];
    for (int i = 0; i < IS31FL3216_CH_NUM_MAX; ++i) {
        if ((chBits >> i) & 0x01) {
            value = value & (~(1 << i));
        }
    }
    uint16_t *pValue = &Is31Value[IS31FL3216_REG_LED_CTRL_H];
    Is31Value[IS31FL3216_REG_LED_CTRL_H] = value >> 8;
    Is31Value[IS31FL3216_REG_LED_CTRL_L] = value;
    // ESP_LOGI(IS31_TAG, "%s, chBits=%x, value:%04x, p:%04x", __func__, chBits, value, *pValue);
    ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_LED_CTRL_H, &Is31Value[IS31FL3216_REG_LED_CTRL_H], 2);
    return ret;
}

esp_err_t Is31fl3216CurModeSet(Is31fl3216Handle handle, Is31fl3216CurMode mode)
{
    IS31_PARAM_CHECK(NULL != handle);
    if (IS31FL3216_CUR_MODE_REXT == mode) {
        Is31Value[IS31FL3216_REG_CONFIG] = (Is31Value[IS31FL3216_REG_CONFIG] & (~(1 << 4)));
    } else if (IS31FL3216_CUR_MODE_AUDIO == mode) {
        Is31Value[IS31FL3216_REG_CONFIG] = (Is31Value[IS31FL3216_REG_CONFIG] & (~(1 << 4))) | (1 << 4);
    } else {
        return ESP_FAIL;
    }
    int ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_CONFIG, (uint8_t *) &Is31Value[IS31FL3216_REG_CONFIG], 1);
    return ret;
}

esp_err_t Is31fl3216CurValueSet(Is31fl3216Handle handle, Is31fl3216CurValue value)
{
    IS31_PARAM_CHECK(NULL != handle);
    Is31Value[IS31FL3216_REG_LED_EFFECT] = (Is31Value[IS31FL3216_REG_LED_EFFECT] & (~(7 << 4))) | value << 4;
    esp_err_t ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_LED_EFFECT, &Is31Value[IS31FL3216_REG_LED_EFFECT], 1);
    return ret;
}

esp_err_t Is31fl3216AgsValueSet(Is31fl3216Handle handle, Is31fl3216AgsValue value)
{
    IS31_PARAM_CHECK(NULL != handle);
    Is31Value[IS31FL3216_REG_LED_EFFECT] = (Is31Value[IS31FL3216_REG_LED_EFFECT] & (~(7 << 0))) | value << 0;
    esp_err_t ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_LED_EFFECT, &Is31Value[IS31FL3216_REG_LED_EFFECT], 1);
    return ret;
}

esp_err_t Is31fl3216AgcCfg(Is31fl3216Handle handle, uint32_t en)
{
    IS31_PARAM_CHECK(NULL != handle);
    Is31Value[IS31FL3216_REG_LED_EFFECT] = (Is31Value[IS31FL3216_REG_LED_EFFECT] & (~(1 << 3))) | en << 3;
    esp_err_t ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_LED_EFFECT, &Is31Value[IS31FL3216_REG_LED_EFFECT], 1);
    return ret;
}

esp_err_t Is31fl3216CascadeModeSet(Is31fl3216Handle handle, Is31fl3216CascadeMode mode)
{
    IS31_PARAM_CHECK(NULL != handle);
    Is31Value[IS31FL3216_REG_LED_EFFECT] = (Is31Value[IS31FL3216_REG_LED_EFFECT] & (~(1 << 7))) | mode << 7;
    esp_err_t ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_LED_EFFECT, &Is31Value[IS31FL3216_REG_LED_EFFECT], 1);
    return ret;
}

esp_err_t Is31fl3216SampleRateSet(Is31fl3216Handle handle, uint32_t value)
{
    IS31_PARAM_CHECK(NULL != handle);
    uint8_t dat = value;
    esp_err_t ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_ADC_RATE, &dat, 1);
    return ret;
}

esp_err_t Is31fl3216FrameTimeSet(Is31fl3216Handle handle, Is31fl3216DelayTime time)
{
    IS31_PARAM_CHECK(NULL != handle);
    uint8_t dat = time << 5;
    esp_err_t ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_FRAME_DELAY, &dat, 1);
    return ret;
}

esp_err_t Is31fl3216FirstFrameSet(Is31fl3216Handle handle, uint32_t frame)
{
    IS31_PARAM_CHECK(NULL != handle);
    uint8_t dat = frame << 5;
    esp_err_t ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_FRAME_START, &dat, 1);
    return ret;
}

esp_err_t Is31fl3216FrameValueSet(Is31fl3216Handle handle, uint32_t num, uint8_t *data, uint32_t len)
{
    IS31_PARAM_CHECK(NULL != handle);
    IS31_PARAM_CHECK(NULL != data);
    uint8_t startAddr = IS31FL3216_REG_FRAME1_CTRL + (num - 1) * 18;
    esp_err_t ret = Is31fl3216WriteReg(handle, startAddr, data, len);
    return ret;
}

esp_err_t Is31fl3216Reset(Is31fl3216Handle handle)
{
    int ret = 0;
    uint8_t dat = 0x00;
    IS31_PARAM_CHECK(NULL != handle);
    ret = Is31fl3216Power(handle, IS31FL3216_PWR_NORMAL);
    if (ret) {
        return ret;
    }
    for (int i = 0; i < IS31FL3216_CH_NUM_MAX; ++i) {
        ret = Is31fl3216ChDutySet(handle, 1 << i, 0);
        if (ret) {
            return ret;
        }
    }
    ret = Is31fl3216ChEnable(handle, IS31FL3216_CH_ALL);
    if (ret) {
        return ret;
    }
    ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_LED_EFFECT, &dat, 1);
    if (ret) {
        return ret;
    }
    ret = Is31fl3216WriteReg(handle, IS31FL3216_REG_CH_CONFIG, &dat, 1);
    return ret;
}


/**
 * @brief i2c master initialization
 */

Is31fl3216Handle Is31fl3216Initialize(void)
{
    i2c_config_t conf = {0};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = I2C_MASTER_SDA_IO;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_io_num = I2C_MASTER_SCL_IO;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = I2C_MASTER_FREQ_HZ;
    Is31fl3216Dev *led = (Is31fl3216Dev *) calloc(1, sizeof(Is31fl3216Dev));
    led->bus = i2c_bus_create(I2C_MASTER_NUM, &conf);
    SET_PERI_REG_BITS(PERIPHS_IO_MUX_GPIO18_U, FUN_DRV, 3, FUN_DRV_S);
    SET_PERI_REG_BITS(PERIPHS_IO_MUX_GPIO23_U, FUN_DRV, 3, FUN_DRV_S);
    led->addr = IS31FL3216_ADDRESS;
    IS31_ERROR_CHECK(ESP_OK == Is31fl3216Power(led, IS31FL3216_PWR_NORMAL));
    IS31_ERROR_CHECK(ESP_OK == Is31fl3216CurModeSet(led, IS31FL3216_CUR_MODE_REXT));
    IS31_ERROR_CHECK(ESP_OK == Is31fl3216CurValueSet(led, IS31FL3216_CUR_0_75));
    return (Is31fl3216Handle) led;
}

esp_err_t Is31fl3216Finalize(Is31fl3216Handle handle)
{
    Is31fl3216Dev *dev = (Is31fl3216Dev *) handle;
    if (dev->bus) {
        i2c_bus_delete(dev->bus);
        dev->bus = NULL;
    }
    free(dev);
    return ESP_OK;
}
