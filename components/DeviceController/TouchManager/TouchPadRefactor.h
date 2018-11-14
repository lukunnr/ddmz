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

#pragma once

#include "esp_intr.h"
#include "esp_err.h"
#include "esp_intr_alloc.h"
#include "soc/touch_channel.h"

#define TOUCH_PAD_MEASURE_CYCLE_DEFAULT_REFACTOR (0x7fff)  /*!<The timer frequency is 8Mhz, the max value is 0x7fff */
#define TOUCH_PAD_MEASURE_WAIT_DEFAULT_REFACTOR  (0xFF)    /*!<The timer frequency is 8Mhz, the max value is 0xff */
#define TOUCH_FSM_MODE_DEFAULT_REFACTOR          (TOUCH_FSM_MODE_SW)  /*!<The touch FSM my be started by the software or timer */

/**
 * @brief Initialize touch module.
 * @note  The default FSM mode is 'TOUCH_FSM_MODE_SW'. If you want to use interrupt trigger mode,
 *        then set it using function 'touch_pad_set_fsm_mode' to 'TOUCH_FSM_MODE_TIMER' after calling 'touch_pad_init'.
 * @return
 *     - ESP_OK Success
 *     - ESP_FAIL Touch pad init error
 */
esp_err_t touch_pad_init_refactor();

/**
 * @brief Un-install touch pad driver.
 * @return
 *     - ESP_OK   Success
 *     - ESP_FAIL Touch pad driver not initialized
 */
esp_err_t touch_pad_deinit_refactor();

/**
 * @brief Configure touch pad interrupt threshold.
 * @param touch_num touch pad index
 * @param threshold interrupt threshold,
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG if argument wrong
 *     - ESP_FAIL if touch pad not initialized
 */
esp_err_t touch_pad_config_refactor(touch_pad_t touch_num, uint16_t threshold);

/**
 * @brief get touch sensor counter value.
 *        Each touch sensor has a counter to count the number of charge/discharge cycles.
 *        When the pad is not 'touched', we can get a number of the counter.
 *        When the pad is 'touched', the value in counter will get smaller because of the larger equivalent capacitance.
 * @note This API requests hardware measurement once. If IIR filter mode is enabled,,
 *       please use 'touch_pad_read_raw_data' interface instead.
 * @param touch_num touch pad index
 * @param touch_value pointer to accept touch sensor value
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Touch pad error
 *     - ESP_FAIL Touch pad not initialized
 */
esp_err_t touch_pad_read_refactor(touch_pad_t touch_num, uint16_t * touch_value);

/**
 * @brief get filtered touch sensor counter value by IIR filter.
 * @note touch_pad_filter_start has to be called before calling touch_pad_read_filtered.
 *       This function can be called from ISR
 *
 * @param touch_num touch pad index
 * @param touch_value pointer to accept touch sensor value
 * @return
 *     - ESP_OK Success
 *     - ESP_ERR_INVALID_ARG Touch pad error
 *     - ESP_ERR_INVALID_STATE Touch pad not initialized
 *     - ESP_FAIL Touch pad not initialized
 */
esp_err_t touch_pad_read_filtered_refactor(touch_pad_t touch_num, uint16_t *touch_value);

/**
 * @brief get raw data (touch sensor counter value) from IIR filter process.
 *        Need not request hardware measurements.
 * @note touch_pad_filter_start has to be called before calling touch_pad_read_raw_data.
 *       This function can be called from ISR
 *
 * @param touch_num touch pad index
 * @param touch_value pointer to accept touch sensor value
 *
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG Touch pad error
 *      - ESP_ERR_INVALID_STATE Touch pad not initialized
 *      - ESP_FAIL Touch pad not initialized
 */
esp_err_t touch_pad_read_raw_data(touch_pad_t touch_num, uint16_t *touch_value);

/**
  * @brief Callback function that is called after each IIR filter calculation.
  * @note This callback is called in timer task in each filtering cycle.
  * @note This callback should not be blocked.
  * @param raw_value  The latest raw data(touch sensor counter value) that
  *        points to all channels(raw_value[0..TOUCH_PAD_MAX-1]).
  * @param filtered_value  The latest IIR filtered data(calculated from raw data) that
  *        points to all channels(filtered_value[0..TOUCH_PAD_MAX-1]).
  *
  */
typedef void (* filter_cb_t)(uint16_t *raw_value, uint16_t *filtered_value);

/**
 * @brief Register the callback function that is called after each IIR filter calculation.
 * @note The 'read_cb' callback is called in timer task in each filtering cycle.
 * @param read_cb  Pointer to filtered callback function.
 *                 If the argument passed in is NULL, the callback will stop.
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG set error
 */
esp_err_t touch_pad_set_filter_read_cb(filter_cb_t read_cb);


/**
 * @brief Set touch sensor measurement and sleep time
 * @param sleep_cycle  The touch sensor will sleep after each measurement.
 *                     sleep_cycle decide the interval between each measurement.
 *                     t_sleep = sleep_cycle / (RTC_SLOW_CLK frequency).
 *                     The approximate frequency value of RTC_SLOW_CLK can be obtained using rtc_clk_slow_freq_get_hz function.
 * @param meas_cycle The duration of the touch sensor measurement.
 *                   t_meas = meas_cycle / 8M, the maximum measure time is 0xffff / 8M = 8.19 ms
 * @return
 *      - ESP_OK on success
 */
esp_err_t touch_pad_set_meas_time_refactor(uint16_t sleep_cycle, uint16_t meas_cycle);


/**
 * @brief start touch pad filter function
 *      This API will start a filter to process the noise in order to prevent false triggering
 *      when detecting slight change of capacitance.
 *      Need to call touch_pad_filter_start before all touch filter APIs
 *
 *      If filter is not initialized, this API will initialize the filter with given period.
 *      If filter is already initialized, this API will update the filter period.
 * @note This filter uses FreeRTOS timer, which is dispatched from a task with
 *       priority 1 by default on CPU 0. So if some application task with higher priority
 *       takes a lot of CPU0 time, then the quality of data obtained from this filter will be affected.
 *       You can adjust FreeRTOS timer task priority in menuconfig.
 * @param filter_period_ms filter calibration period, in ms
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_ARG parameter error
 *      - ESP_ERR_NO_MEM No memory for driver
 *      - ESP_ERR_INVALID_STATE driver state error
 */
esp_err_t touch_pad_filter_start_refactor(uint32_t filter_period_ms);

/**
 * @brief stop touch pad filter function
 *        Need to call touch_pad_filter_start before all touch filter APIs
 * @return
 *      - ESP_OK Success
 *      - ESP_ERR_INVALID_STATE driver state error
 */
esp_err_t touch_pad_filter_stop_refactor();

esp_err_t touch_pad_set_filter_period_refactor(uint32_t new_period_ms);

