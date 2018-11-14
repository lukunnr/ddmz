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

// Copyright 2015-2016 Espressif Systems (Shanghai) PTE LTD
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef _IOT_TOUCHPAD_H_
#define _IOT_TOUCHPAD_H_
#include "driver/touch_pad.h"
#include "esp_audio_log.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void* tp_handle_t;
typedef void* tp_slide_handle_t;
typedef void* tp_matrix_handle_t;

typedef void (* tp_cb)(void *);           /**< callback function of touchpad */
typedef void (* tp_matrix_cb)(void *, uint8_t, uint8_t);      /**< callback function of touchpad matrix */

typedef enum {
    TOUCHPAD_SINGLE_TRIGGER,        /**< touchpad event will only trigger once no matter how long you press the touchpad */
    TOUCHPAD_SERIAL_TRIGGER,        /**< number of touchpad evetn ttiggerred will be in direct proportion to the duration of press*/
} touchpad_trigger_t;

typedef enum {
    TOUCHPAD_EVENT_PUSH,            /**< touch pad push event */
    TOUCHPAD_EVENT_RELEASE,         /**< touch pad release event */
    TOUCHPAD_EVENT_TAP,             /**< touch pad quick tap event */
    TOUCHPAD_EVENT_LONG_PRESS,      /**< touch pad long press event */
} touchpad_event_t;

typedef struct {
    tp_handle_t handle;
    touch_pad_t num;
    touchpad_event_t event;
} touchpad_msg_t;

typedef enum {
    TOUCHPAD_CB_PUSH = 0,        /**< touch pad push callback */
    TOUCHPAD_CB_RELEASE,         /**< touch pad release callback */
    TOUCHPAD_CB_TAP,             /**< touch pad quick tap callback */
    TOUCHPAD_CB_SLIDE,           /**< touch pad slide type callback */
    TOUCHPAD_CB_MAX,
} tp_cb_type_t;

/**
  * @brief  Enable scope debug function. Print the touch sensor raw data to "DataScope" tool via UART.
  *         "DataScope" tool is touch sensor tune tool. User can monitor the data of each touch channel,
  *         evaluate the touch system's touch performance (sensitivity, SNR, stability, channel coupling)
  *         and determine the threshold for each channel.
  *
  * @attention 1. Choose a UART port that will only be used for scope debug.
  * @attention 2. Use this feature only during the testing phase.
  * @attention 3. "DataScope" tool can be downloaded from Espressif's official website.
  *
  * @param  uart_num The uart port to send touch sensor raw data.
  * @param  tx_io_num set UART TXD IO.
  * @param  rx_io_num set UART RXD IO.
  * @param  baud_rate set debug port baud rate.
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param uart_num is error
  */
esp_err_t iot_tp_scope_debug_init(uint8_t uart_num, int tx_io_num, int rx_io_num, int baud_rate);

/**
  * @brief  create single button device
  *
  * @param  touch_pad_num refer to struct touch_pad_t
  * @param  sensitivity The max change rate of the reading value when a touch occurs.
  *         i.e., (non-trigger value - trigger value) / non-trigger value.
  *         Decreasing this threshold appropriately gives higher sensitivity.
  *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
  *
  * @return
  *     tp_handle_t: Touch pad handle.
  */
tp_handle_t iot_tp_create(touch_pad_t touch_pad_num, float sensitivity);

/**
  * @brief  delete touchpad device
  *
  * @param  tp_handle Touch pad handle.
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_delete(tp_handle_t tp_handle);

/**
  * @brief  add callback function
  *
  * @param  tp_handle Touch pad handle.
  * @param  cb_type the type of callback to be added
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_add_cb(tp_handle_t tp_handle, tp_cb_type_t cb_type, tp_cb cb, void  *arg);

/**
  * @brief  set serial tigger
  *
  * @param  tp_handle Touch pad handle.
  * @param  trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
  * @param  interval_ms evetn would trigger every interval_ms
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_set_serial_trigger(tp_handle_t tp_handle, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_cb cb, void *arg);

/**
  * @brief  add custom callback function
  *
  * @param  tp_handle Touch pad handle.
  * @param  press_sec the callback function would be called pointed seconds
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: fail
  */
esp_err_t iot_tp_add_custom_cb(tp_handle_t tp_handle, uint32_t press_sec, tp_cb cb, void  *arg);

/**
  * @brief  get the number of a touchpad
  *
  * @param  tp_handle Touch pad handle.
  *
  * @return  touchpad number
  */
touch_pad_t iot_tp_num_get(tp_handle_t tp_handle);

/**
  * @brief  Set the trigger threshold of touchpad.
  *
  * @param  tp_handle Touch pad handle.
  * @param  threshold Should be less than the max change rate of touch.
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_set_threshold(tp_handle_t tp_handle, float threshold);

/**
  * @brief  Get the trigger threshold of touchpad.
  *
  * @param  tp_handle Touch pad handle.
  * @param  threshold value
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_get_threshold(const tp_handle_t tp_handle, float *threshold);

/**
  * @brief  Get the IIR filter interval of touch sensor when touching.
  *
  * @param  tp_handle Touch pad handle.
  * @param  filter_ms
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_get_touch_filter_interval(const tp_handle_t tp_handle, uint32_t *filter_ms);

/**
  * @brief  Get the IIR filter interval of touch sensor when idle.
  *
  * @param  tp_handle Touch pad handle.
  * @param  filter_ms
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_get_idle_filter_interval(const tp_handle_t tp_handle, uint32_t *filter_ms);

/**
  * @brief  Get filtered touch sensor counter value from IIR filter process.
  *
  * @param  tp_handle Touch pad handle.
  * @param  touch_value_ptr pointer to the value read 
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t iot_tp_read(tp_handle_t tp_handle, uint16_t *touch_value_ptr);

/**
  * @brief  Get raw touch sensor counter value from IIR filter process.
  *
  * @param  tp_handle Touch pad handle.
  * @param  touch_value_ptr pointer to the value read
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_handle is NULL
  */
esp_err_t tp_read_raw(const tp_handle_t tp_handle, uint16_t *touch_value_ptr);

/**
  * @brief  Create touchpad slide device.
  *
  * @param  num number of touchpads the slide uses
  * @param  tps the array of touchpad num
  * @param  pos_range Set the range of the slide position. (0 ~ 255)
  * @param  p_sensitivity Data list, the list stores the max change rate of the reading value when a touch occurs.
  *         i.e., (non-trigger value - trigger value) / non-trigger value.
  *         Decreasing this threshold appropriately gives higher sensitivity.
  *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
  *
  * @return
  *     NULL: error of input parameter
  *     tp_slide_handle_t: slide handle
  */
tp_slide_handle_t iot_tp_slide_create(uint8_t num, const touch_pad_t *tps, uint8_t pos_range, const float *p_sensitivity);

/**
  * @brief  Get relative position of touch.
  *
  * @param  tp_slide_handle
  *
  * @return  relative position of your touch on slide. The range is 0 ~  pos_range.
  */
uint8_t iot_tp_slide_position(tp_slide_handle_t tp_slide_handle);

/**
  * @brief  delete touchpad slide device
  *
  * @param  tp_slide_handle
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_slide_handle is NULL
  */
esp_err_t iot_tp_slide_delete(tp_slide_handle_t tp_slide_handle);

/**
  * @brief  create touchpad matrix device
  *
  * @param  x_num number of touch sensor on x axis
  * @param  y_num number of touch sensor on y axis
  * @param  x_tps the array of touch sensor num on x axis
  * @param  y_tps the array of touch sensor num on y axis
  * @param  p_sensitivity Data list(x list + y list), the list stores the max change rate of the reading value
  *         when a touch occurs. i.e., (non-trigger value - trigger value) / non-trigger value.
  *         Decreasing this threshold appropriately gives higher sensitivity.
  *         If the value is less than 0.1 (10%), leave at least 4 decimal places.
  *
  * @return
  *     NULL: error of input parameter
  *     tp_matrix_handle_t: matrix handle
  */
tp_matrix_handle_t iot_tp_matrix_create(uint8_t x_num, uint8_t y_num, const touch_pad_t *x_tps, \
        const touch_pad_t *y_tps, const float *p_sensitivity);

/**
  * @brief  delete touchpad matrix device
  *
  * @param  tp_matrix_hd
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: the param tp_matrix_hd is NULL
  */
esp_err_t iot_tp_matrix_delete(tp_matrix_handle_t tp_matrix_hd);

/**
  * @brief  add callback function
  *
  * @param  tp_matrix_hd 
  * @param  cb_type the type of callback to be added
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: error of input parameter
  */
esp_err_t iot_tp_matrix_add_cb(tp_matrix_handle_t tp_matrix_hd, tp_cb_type_t cb_type, tp_matrix_cb cb, void *arg);

/**
  * @brief  add custom callback function
  *
  * @param  tp_matrix_hd 
  * @param  press_sec the callback function would be called after pointed seconds
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: error of input parameter
  */
esp_err_t iot_tp_matrix_add_custom_cb(tp_matrix_handle_t tp_matrix_hd, uint32_t press_sec, tp_matrix_cb cb, void *arg);

/**
  * @brief  set serial tigger
  *
  * @param  tp_matrix_hd 
  * @param  trigger_thres_sec serial event would trigger after trigger_thres_sec seconds
  * @param  interval_ms evetn would trigger every interval_ms
  * @param  cb the callback function
  * @param  arg the argument of callback function
  *
  * @return
  *     - ESP_OK: succeed
  *     - ESP_FAIL: error of input parameter
  */
esp_err_t iot_tp_matrix_set_serial_trigger(tp_matrix_handle_t tp_matrix_hd, uint32_t trigger_thres_sec, uint32_t interval_ms, tp_matrix_cb cb, void *arg);

#endif
