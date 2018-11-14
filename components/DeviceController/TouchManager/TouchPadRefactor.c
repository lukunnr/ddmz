// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at

//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <esp_types.h>
#include <stdlib.h>
#include <ctype.h>
#include "rom/ets_sys.h"
#include "esp_log.h"
#include "soc/rtc_io_reg.h"
#include "soc/rtc_io_struct.h"
#include "soc/sens_reg.h"
#include "soc/sens_struct.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/rtc_cntl_struct.h"
#include "soc/syscon_reg.h"
#include "soc/syscon_struct.h"
#include "freertos/FreeRTOS.h"
#include "freertos/xtensa_api.h"
#include "freertos/semphr.h"
#include "freertos/timers.h"
#include "esp_intr_alloc.h"
#include "sys/lock.h"
#include "driver/rtc_cntl.h"
#include "driver/gpio.h"
#include "TouchPadRefactor.h"

#ifndef NDEBUG
// Enable built-in checks in queue.h in debug builds
#define INVARIANTS
#endif
#include "rom/queue.h"


#define ADC_FSM_RSTB_WAIT_DEFAULT     (8)
#define ADC_FSM_START_WAIT_DEFAULT    (5)
#define ADC_FSM_STANDBY_WAIT_DEFAULT  (100)
#define ADC_FSM_TIME_KEEP             (-1)
#define ADC_MAX_MEAS_NUM_DEFAULT      (255)
#define ADC_MEAS_NUM_LIM_DEFAULT      (1)
#define SAR_ADC_CLK_DIV_DEFUALT       (2)
#define ADC_PATT_LEN_MAX              (16)
#define TOUCH_PAD_FILTER_FACTOR_DEFAULT   (4)   // IIR filter coefficient.
#define TOUCH_PAD_SHIFT_DEFAULT           (4)   // Increase computing accuracy.
#define TOUCH_PAD_SHIFT_ROUND_DEFAULT     (8)   // ROUND = 2^(n-1); rounding off for fractional.
#define DAC_ERR_STR_CHANNEL_ERROR   "DAC channel error"

static const char *RTC_MODULE_TAG = "RTC_MODULE";

#define RTC_MODULE_CHECK(a, str, ret_val) if (!(a)) {                                \
    ESP_LOGE(RTC_MODULE_TAG,"%s:%d (%s):%s", __FILE__, __LINE__, __FUNCTION__, str); \
    return (ret_val);                                                              \
}

#define RTC_RES_CHECK(res, ret_val) if ( (a) != ESP_OK) {                           \
        ESP_LOGE(RTC_MODULE_TAG,"%s:%d (%s)", __FILE__, __LINE__, __FUNCTION__); \
        return (ret_val);                                                              \
}


static portMUX_TYPE rtc_spinlock = portMUX_INITIALIZER_UNLOCKED;
static SemaphoreHandle_t rtc_touch_mux = NULL;

typedef struct {
    TimerHandle_t timer;
    uint16_t filtered_val[TOUCH_PAD_MAX];
    uint16_t raw_val[TOUCH_PAD_MAX];
    uint32_t filter_period;
    uint32_t period;
    bool enable;
} touch_pad_filter_t;
static touch_pad_filter_t *s_touch_pad_filter = NULL;
// check if touch pad be inited.
static uint16_t s_touch_pad_init_bit = 0x0000;
static filter_cb_t s_filter_cb = NULL;

static const char TAG[] = "adc";

/*---------------------------------------------------------------
                    Touch Pad
---------------------------------------------------------------*/
//Some register bits of touch sensor 8 and 9 are mismatched, we need to swap the bits.
#define BITSWAP(data, n, m)   (((data >> n) &  0x1)  == ((data >> m) & 0x1) ? (data) : ((data) ^ ((0x1 <<n) | (0x1 << m))))
#define TOUCH_BITS_SWAP(v)  BITSWAP(v, TOUCH_PAD_NUM8, TOUCH_PAD_NUM9)
static esp_err_t _touch_pad_read(touch_pad_t touch_num, uint16_t *touch_value, touch_fsm_mode_t mode);

//Some registers of touch sensor 8 and 9 are mismatched, we need to swap register index
inline static touch_pad_t touch_pad_num_wrap(touch_pad_t touch_num)
{
    if (touch_num == TOUCH_PAD_NUM8) {
        return TOUCH_PAD_NUM9;
    } else if (touch_num == TOUCH_PAD_NUM9) {
        return TOUCH_PAD_NUM8;
    }
    return touch_num;
}


static uint32_t _touch_filter_iir(uint32_t in_now, uint32_t out_last, uint32_t k)
{
    if (k == 0) {
        return in_now;
    } else {
        uint32_t out_now = (in_now + (k - 1) * out_last) / k;
        return out_now;
    }
}

esp_err_t touch_pad_set_filter_read_cb(filter_cb_t read_cb)
{
    s_filter_cb = read_cb;
    return ESP_OK;
}

static void touch_pad_filter_cb(void *arg)
{
    static uint32_t s_filtered_temp[TOUCH_PAD_MAX] = {0};

    if (s_touch_pad_filter == NULL) {
        return;
    }
    uint16_t val = 0;
    touch_fsm_mode_t mode;
    xSemaphoreTake(rtc_touch_mux, portMAX_DELAY);
    touch_pad_get_fsm_mode(&mode);
    for (int i = 0; i < TOUCH_PAD_MAX; i++) {
        if ((s_touch_pad_init_bit >> i) & 0x1) {
            _touch_pad_read(i, &val, mode);
            s_touch_pad_filter->raw_val[i] = val;
            s_filtered_temp[i] = s_filtered_temp[i] == 0 ? ((uint32_t)val << TOUCH_PAD_SHIFT_DEFAULT) : s_filtered_temp[i];
            s_filtered_temp[i] = _touch_filter_iir((val << TOUCH_PAD_SHIFT_DEFAULT),
                    s_filtered_temp[i], TOUCH_PAD_FILTER_FACTOR_DEFAULT);
            s_touch_pad_filter->filtered_val[i] = (s_filtered_temp[i] + TOUCH_PAD_SHIFT_ROUND_DEFAULT) >> TOUCH_PAD_SHIFT_DEFAULT;
        }
    }
    xTimerReset(s_touch_pad_filter->timer, portMAX_DELAY);
    xSemaphoreGive(rtc_touch_mux);
    if(s_filter_cb != NULL) {
        //return the raw data and filtered data.
        s_filter_cb(s_touch_pad_filter->raw_val, s_touch_pad_filter->filtered_val);
    }
}

esp_err_t touch_pad_set_meas_time_refactor(uint16_t sleep_cycle, uint16_t meas_cycle)
{
    xSemaphoreTake(rtc_touch_mux, portMAX_DELAY);
    portENTER_CRITICAL(&rtc_spinlock);
    //touch sensor sleep cycle Time = sleep_cycle / RTC_SLOW_CLK( can be 150k or 32k depending on the options)
    SENS.sar_touch_ctrl2.touch_sleep_cycles = sleep_cycle;
    //touch sensor measure time= meas_cycle / 8Mhz
    SENS.sar_touch_ctrl1.touch_meas_delay = meas_cycle;
    //the waiting cycles (in 8MHz) between TOUCH_START and TOUCH_XPD
    SENS.sar_touch_ctrl1.touch_xpd_wait = TOUCH_PAD_MEASURE_WAIT_DEFAULT_REFACTOR;
    portEXIT_CRITICAL(&rtc_spinlock);
    xSemaphoreGive(rtc_touch_mux);
    return ESP_OK;
}


esp_err_t touch_pad_config_refactor(touch_pad_t touch_num, uint16_t threshold)
{
    RTC_MODULE_CHECK(rtc_touch_mux != NULL, "Touch pad not initialized", ESP_FAIL);
    RTC_MODULE_CHECK(touch_num < TOUCH_PAD_MAX, "Touch_Pad Num Err", ESP_ERR_INVALID_ARG);
    s_touch_pad_init_bit |= (1 << touch_num);
    touch_pad_set_thresh(touch_num, threshold);
    touch_pad_io_init(touch_num);
    touch_pad_set_cnt_mode(touch_num, TOUCH_PAD_SLOPE_7, TOUCH_PAD_TIE_OPT_LOW);
    touch_fsm_mode_t mode;
    touch_pad_get_fsm_mode(&mode);
    if (TOUCH_FSM_MODE_SW == mode) {
        touch_pad_clear_group_mask((1 << touch_num), (1 << touch_num), (1 << touch_num));
    } else if (TOUCH_FSM_MODE_TIMER == mode){
        touch_pad_set_group_mask((1 << touch_num), (1 << touch_num), (1 << touch_num));
    } else {
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t touch_pad_init_refactor()
{
    if (rtc_touch_mux == NULL) {
        rtc_touch_mux = xSemaphoreCreateMutex();
    }
    if (rtc_touch_mux == NULL) {
        return ESP_FAIL;
    }
    touch_pad_intr_disable();
    touch_pad_clear_group_mask(TOUCH_PAD_BIT_MASK_MAX, TOUCH_PAD_BIT_MASK_MAX, TOUCH_PAD_BIT_MASK_MAX);
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_DEFAULT);
    touch_pad_set_trigger_mode(TOUCH_FSM_MODE_DEFAULT_REFACTOR);
    touch_pad_set_trigger_source(TOUCH_TRIGGER_SOURCE_DEFAULT);
    touch_pad_clear_status();
    touch_pad_set_meas_time_refactor(TOUCH_PAD_SLEEP_CYCLE_DEFAULT, TOUCH_PAD_MEASURE_CYCLE_DEFAULT_REFACTOR);
    return ESP_OK;
}

esp_err_t touch_pad_deinit_refactor()
{
    if (rtc_touch_mux == NULL) {
        return ESP_FAIL;
    }
    s_touch_pad_init_bit = 0x0000;
    touch_pad_filter_delete();
    touch_pad_set_fsm_mode(TOUCH_FSM_MODE_SW);
    touch_pad_clear_status();
    touch_pad_intr_disable();
    vSemaphoreDelete(rtc_touch_mux);
    rtc_touch_mux = NULL;
    return ESP_OK;
}

static esp_err_t _touch_pad_read(touch_pad_t touch_num, uint16_t *touch_value, touch_fsm_mode_t mode)
{
    esp_err_t res = ESP_OK;
    touch_pad_t tp_wrap = touch_pad_num_wrap(touch_num);
    if (TOUCH_FSM_MODE_SW == mode) {
        touch_pad_set_group_mask((1 << touch_num), (1 << touch_num), (1 << touch_num));
        touch_pad_sw_start();
        while (SENS.sar_touch_ctrl2.touch_meas_done == 0) {};
        *touch_value = (tp_wrap & 0x1) ? \
                            SENS.touch_meas[tp_wrap / 2].l_val: \
                            SENS.touch_meas[tp_wrap / 2].h_val;

        touch_pad_clear_group_mask((1 << touch_num), (1 << touch_num), (1 << touch_num));
    } else if (TOUCH_FSM_MODE_TIMER == mode) {
        while (SENS.sar_touch_ctrl2.touch_meas_done == 0) {};
        *touch_value = (tp_wrap & 0x1) ? \
                            SENS.touch_meas[tp_wrap / 2].l_val: \
                            SENS.touch_meas[tp_wrap / 2].h_val;
    } else {
        res = ESP_FAIL;
    }
    return res;
}

esp_err_t touch_pad_read_refactor(touch_pad_t touch_num, uint16_t *touch_value)
{
    RTC_MODULE_CHECK(touch_num < TOUCH_PAD_MAX, "Touch_Pad Num Err", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(touch_value != NULL, "touch_value", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(rtc_touch_mux != NULL, "Touch pad not initialized", ESP_FAIL);

    esp_err_t res = ESP_OK;
    touch_fsm_mode_t mode;
    touch_pad_get_fsm_mode(&mode);
    xSemaphoreTake(rtc_touch_mux, portMAX_DELAY);
    res = _touch_pad_read(touch_num, touch_value, mode);
    xSemaphoreGive(rtc_touch_mux);
    return res;
}

IRAM_ATTR esp_err_t touch_pad_read_raw_data(touch_pad_t touch_num, uint16_t *touch_value)
{
    RTC_MODULE_CHECK(rtc_touch_mux != NULL, "Touch pad not initialized", ESP_FAIL);
    RTC_MODULE_CHECK(touch_num < TOUCH_PAD_MAX, "Touch_Pad Num Err", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(touch_value != NULL, "touch_value", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(s_touch_pad_filter != NULL, "Touch pad filter not initialized", ESP_ERR_INVALID_STATE);
    *touch_value = s_touch_pad_filter->raw_val[touch_num];
    return ESP_OK;
}

esp_err_t touch_pad_set_filter_period_refactor(uint32_t new_period_ms)
{
    RTC_MODULE_CHECK(s_touch_pad_filter != NULL, "Touch pad filter not initialized", ESP_ERR_INVALID_STATE);
    RTC_MODULE_CHECK(new_period_ms > 0, "Touch pad filter period error", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(rtc_touch_mux != NULL, "Touch pad not initialized", ESP_ERR_INVALID_STATE);

    esp_err_t ret = ESP_OK;
    xSemaphoreTake(rtc_touch_mux, portMAX_DELAY);
    if (s_touch_pad_filter != NULL) {
        xTimerChangePeriod(s_touch_pad_filter->timer, new_period_ms / portTICK_PERIOD_MS, portMAX_DELAY);
        s_touch_pad_filter->period = new_period_ms;
    } else {
        ESP_LOGE(RTC_MODULE_TAG, "Touch pad filter deleted");
        ret = ESP_ERR_INVALID_STATE;
    }
    xSemaphoreGive(rtc_touch_mux);
    return ret;
}

esp_err_t touch_pad_get_filter_period_refactor(uint32_t* p_period_ms)
{
    RTC_MODULE_CHECK(s_touch_pad_filter != NULL, "Touch pad filter not initialized", ESP_ERR_INVALID_STATE);
    RTC_MODULE_CHECK(p_period_ms != NULL, "Touch pad period pointer error", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(rtc_touch_mux != NULL, "Touch pad not initialized", ESP_ERR_INVALID_STATE);

    esp_err_t ret = ESP_OK;
    xSemaphoreTake(rtc_touch_mux, portMAX_DELAY);
    if (s_touch_pad_filter != NULL) {
        *p_period_ms = s_touch_pad_filter->period;
    } else {
        ESP_LOGE(RTC_MODULE_TAG, "Touch pad filter deleted");
        ret = ESP_ERR_INVALID_STATE;
    }
    xSemaphoreGive(rtc_touch_mux);
    return ret;
}

IRAM_ATTR esp_err_t touch_pad_read_filtered_refactor(touch_pad_t touch_num, uint16_t *touch_value)
{
    RTC_MODULE_CHECK(rtc_touch_mux != NULL, "Touch pad not initialized", ESP_FAIL);
    RTC_MODULE_CHECK(touch_num < TOUCH_PAD_MAX, "Touch_Pad Num Err", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(touch_value != NULL, "touch_value", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(s_touch_pad_filter != NULL, "Touch pad filter not initialized", ESP_ERR_INVALID_STATE);
    *touch_value = (s_touch_pad_filter->filtered_val[touch_num]);
    return ESP_OK;
}


esp_err_t touch_pad_filter_start_refactor(uint32_t filter_period_ms)
{
    RTC_MODULE_CHECK(filter_period_ms >= portTICK_PERIOD_MS, "Touch pad filter period error", ESP_ERR_INVALID_ARG);
    RTC_MODULE_CHECK(rtc_touch_mux != NULL, "Touch pad not initialized", ESP_ERR_INVALID_STATE);

    esp_err_t ret = ESP_OK;
    xSemaphoreTake(rtc_touch_mux, portMAX_DELAY);
    if (s_touch_pad_filter == NULL) {
        s_touch_pad_filter = (touch_pad_filter_t *) calloc(1, sizeof(touch_pad_filter_t));
        if (s_touch_pad_filter == NULL) {
            ret = ESP_ERR_NO_MEM;
        }
    }
    if (s_touch_pad_filter->timer == NULL) {
        s_touch_pad_filter->timer = xTimerCreate("filter_tmr", filter_period_ms / portTICK_PERIOD_MS, pdFALSE,
        NULL, touch_pad_filter_cb);
        if (s_touch_pad_filter->timer == NULL) {
            ret = ESP_ERR_NO_MEM;
        }
        xTimerStart(s_touch_pad_filter->timer, portMAX_DELAY);
    } else {
        xTimerChangePeriod(s_touch_pad_filter->timer, filter_period_ms / portTICK_PERIOD_MS, portMAX_DELAY);
        s_touch_pad_filter->period = filter_period_ms;
        xTimerStart(s_touch_pad_filter->timer, portMAX_DELAY);
    }
    xSemaphoreGive(rtc_touch_mux);
    return ret;
}

esp_err_t touch_pad_filter_stop_refactor()
{
    RTC_MODULE_CHECK(s_touch_pad_filter != NULL, "Touch pad filter not initialized", ESP_ERR_INVALID_STATE);

    esp_err_t ret = ESP_OK;
    xSemaphoreTake(rtc_touch_mux, portMAX_DELAY);
    if (s_touch_pad_filter != NULL) {
        xTimerStop(s_touch_pad_filter->timer, portMAX_DELAY);
    } else {
        ESP_LOGE(RTC_MODULE_TAG, "Touch pad filter deleted");
        ret = ESP_ERR_INVALID_STATE;
    }
    xSemaphoreGive(rtc_touch_mux);
    return ret;
}
