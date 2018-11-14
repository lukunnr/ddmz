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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
#include "soc/sens_reg.h"

#include "esp_audio_log.h"
#include "driver/touch_pad.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "touchpad.h"
#include "TouchPadRefactor.h"
#include "TouchManager.h"
#include "NotifyHelper.h"
#include "DeviceCommon.h"
#include "userconfig.h"
#include "button.h"
#include "AdcButton.h"

#define TOUCH_TAG "TOUCH_MANAGER"

/*
 * Touch button threshold settings.
 * stores the max change rate of the reading value when a touch occurs.
 * Decreasing this threshold appropriately gives higher sensitivity.
 * If the value is less than 0.1 (10%), leave at least 4 decimal places.
 * Calculation formula: (non-trigger value - trigger value) / non-trigger value.
 * */
#define TOUCHPAD_THRES_PERCENT_PAD0             0.22    // 22.0%
#define TOUCHPAD_THRES_PERCENT_PAD1             0.22    // 22.0%
#define TOUCHPAD_THRES_PERCENT_PAD2             0.22    // 22.0%
#define TOUCHPAD_THRES_PERCENT_PAD3             0.22    // 22.0%
#define TOUCHPAD_LONG_PRESS_SEC_PAD0            3       // 3s
#define TOUCHPAD_LONG_PRESS_SEC_PAD1            3       // 3s
#define TOUCHPAD_LONG_PRESS_SEC_PAD2            3       // 3s
#define TOUCHPAD_LONG_PRESS_SEC_PAD3            3       // 3s

#define TOUCH_EVT_QUEUE_LEN                     8
#define TOUCH_MANAGER_TASK_PRIORITY             7
#define TOUCH_MANAGER_TASK_STACK_SIZE           (3096 + 512)

static int HighWaterLine = 0;
TaskHandle_t TouchManageHandle;
static tp_handle_t tp_dev0, tp_dev1, tp_dev2, tp_dev3;

#define BUTTON_ACTIVE_LEVEL   0

static QueueHandle_t xQueueTouch;
static void ProcessTouchEvent(TouchManager *manager, void *evt)
{
    touchpad_msg_t *recv_value = (touchpad_msg_t *) evt;
    int touch_num = recv_value->num;
    DeviceNotifyMsg msg = DEVICE_NOTIFY_KEY_UNDEFINED;
    switch (touch_num) {

#if IDF_3_0 == 1
        case TOUCH_PAD_NUM9:
#else
        case TOUCH_PAD_NUM8:
#endif
            {
                if (recv_value->event == TOUCHPAD_EVENT_TAP) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Switch Mode");
                    //TODO
                } else if (recv_value->event == TOUCHPAD_EVENT_LONG_PRESS) {
                    //FIXME: does not work now
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Setting");
                    msg = DEVICE_NOTIFY_KEY_WIFI_SET;
                }
            }
            break;
        case TOUCH_PAD_NUM7: {
                if (recv_value->event == TOUCHPAD_EVENT_TAP) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Volume Up Event");
                    msg = DEVICE_NOTIFY_KEY_VOL_UP;
                } else if (recv_value->event == TOUCHPAD_EVENT_LONG_PRESS) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Prev Event");
                    msg = DEVICE_NOTIFY_KEY_PREV;
                }
            }
            break;
#if IDF_3_0 == 1
        case TOUCH_PAD_NUM8:
#else
        case TOUCH_PAD_NUM9:
#endif
            {
                if (recv_value->event == TOUCHPAD_EVENT_TAP) {
                    msg = DEVICE_NOTIFY_KEY_PLAY;
                    ESP_AUDIO_LOGI(TOUCH_TAG, "Play Event");
                } else {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "long press, undefined act");
                }
            }
            break;
        case TOUCH_PAD_NUM4: {
                if (recv_value->event == TOUCHPAD_EVENT_TAP) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Volume Down Event");
                    msg = DEVICE_NOTIFY_KEY_VOL_DOWN;
                } else if (recv_value->event == TOUCHPAD_EVENT_LONG_PRESS) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Next Event");
                    msg = DEVICE_NOTIFY_KEY_NEXT;
                }
            }
            break;
        case GPIO_REC: {
                if (recv_value->event == TOUCHPAD_EVENT_TAP) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Rec Event");
                    msg = DEVICE_NOTIFY_KEY_REC;
                } else if (recv_value->event == TOUCHPAD_EVENT_LONG_PRESS) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch Rec playback Event");
                    msg = DEVICE_NOTIFY_KEY_REC_QUIT;
                }
            }
            break;
        case GPIO_MODE: {
                if (recv_value->event == TOUCHPAD_EVENT_TAP) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch change mode Event");
                    msg = DEVICE_NOTIFY_KEY_MODE;
                }
#if (!defined CONFIG_ENABLE_TURINGAIWIFI_SERVICE) && \
    (!defined CONFIG_ENABLE_TURINGWECHAT_SERVICE)
                else if (recv_value->event == TOUCHPAD_EVENT_LONG_PRESS) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch to switch App(Wi-Fi or Bt)");
                    msg = DEVICE_NOTIFY_KEY_BT_WIFI_SWITCH;
                }
#else
                else if (recv_value->event == TOUCHPAD_EVENT_LONG_PRESS) {
                    ESP_AUDIO_LOGI(TOUCH_TAG, "touch to switch aiwifi or wechat)");
                    msg = DEVICE_NOTIFY_KEY_AIWIFI_WECHAT_SWITCH;
                }

#endif
            }
            break;
        default:
            ESP_AUDIO_LOGI(TOUCH_TAG, "Not supported line=%d", __LINE__);
            break;

    }

    if (msg != DEVICE_NOTIFY_KEY_UNDEFINED) {
        manager->Based.notify((struct DeviceController *) manager->Based.controller, DEVICE_NOTIFY_TYPE_TOUCH, &msg, sizeof(DeviceNotifyMsg));
    }
}

static void TouchManagerEvtTask(void *pvParameters)
{
    TouchManager *manager = (TouchManager *) pvParameters;
    touchpad_msg_t recv_value = {0};
    while (1) {
        if (xQueueReceive(xQueueTouch, &recv_value, portMAX_DELAY)) {
            ProcessTouchEvent(manager, &recv_value);
#if EN_STACK_TRACKER
            if (uxTaskGetStackHighWaterMark(TouchManageHandle) > HighWaterLine) {
                HighWaterLine = uxTaskGetStackHighWaterMark(TouchManageHandle);
                ESP_AUDIO_LOGI("STACK", "%s %d\n\n\n", __func__, HighWaterLine);
            }
#endif
        }
    }
    vTaskDelete(NULL);
}

static touchpad_msg_t gpioPadMsg;
static char AdcBugFixRec = 0, AdcBugFixMode = 0;
static void GpioRecPushCb(void *arg)
{
    gpioPadMsg.num = GPIO_REC;
    gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
    xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
    AdcBugFixRec = 1;

}

static void GpioRecReleaseCb(void *arg)
{
    gpioPadMsg.num = GPIO_REC;
    gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
    if (AdcBugFixRec) {
        AdcBugFixRec = 0;
        xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
    }
}

static void GpioModeTapCb(void *arg)
{
    gpioPadMsg.num = GPIO_MODE;
    gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
    xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
}

static void GpioModePushCb(void *arg)
{
    AdcBugFixMode = 1;
}

static void GpioModeReleaseCb(void *arg)
{

}

static void GpioModePressCb(void *arg)
{
    gpioPadMsg.num = GPIO_MODE;
    gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
    if (AdcBugFixMode) {
        AdcBugFixMode = 0;
        xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
    }
}
static void gpioSetWifiPushCb(void *arg)
{
}

static void gpioSetWifiTapCb(void *arg)
{
}

static void gpioSetWifiReleaseCb(void *arg)
{
}

static void gpiosSetWifiCb(void *arg)
{
    gpioPadMsg.num = 9;
    gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
    xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
}

static void btnCallback(int id, BtnState state)
{
    switch (id) {
        case USER_KEY_SET:
            if (BTN_STATE_CLICK == state) {

            } else if (BTN_STATE_PRESS == state) {
                gpioPadMsg.num = 9;
                gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            }
            break;
        case USER_KEY_PLAY:
            if (BTN_STATE_CLICK == state) {
                gpioPadMsg.num = 8;
                gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            } else if (BTN_STATE_PRESS == state) {

            }
            break;
        case USER_KEY_REC:
            if (BTN_STATE_CLICK == state) {
                gpioPadMsg.num = GPIO_REC;
                gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            } else if (BTN_STATE_RELEASE == state) {
                gpioPadMsg.num = GPIO_REC;
                gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            }
            break;
        case USER_KEY_MODE:
#ifndef CONFIG_ESP_LYRATD_FT_DOSS_V1_0_BOARD
            if (BTN_STATE_CLICK == state) {
                gpioPadMsg.num = GPIO_MODE;
                gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            } else if (BTN_STATE_PRESS == state) {
                gpioPadMsg.num = GPIO_MODE;
                gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            }
#else
            // For play/pause
            if (BTN_STATE_CLICK == state) {
                gpioPadMsg.num = 8;
                gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            } else if (BTN_STATE_PRESS == state) {

            }
#endif
            break;
        case USER_KEY_VOL_DOWN:
            if (BTN_STATE_CLICK == state) {
                gpioPadMsg.num = 4;
                gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            } else if (BTN_STATE_PRESS == state) {
                gpioPadMsg.num = 4;
                gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            }
            break;
        case USER_KEY_VOL_UP:
            if (BTN_STATE_CLICK == state) {
                gpioPadMsg.num = 7;
                gpioPadMsg.event = TOUCHPAD_EVENT_TAP;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            } else if (BTN_STATE_PRESS == state) {
                gpioPadMsg.num = 7;
                gpioPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
                xQueueSend( xQueueTouch, ( void * ) &gpioPadMsg, ( TickType_t ) 0 );
            }
            break;
    }
}

static touchpad_msg_t TouchPadMsg;

static void touchpad_push_cb(void *arg)
{
    tp_handle_t *tp_dev = (tp_handle_t *) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_AUDIO_LOGD("touch", "push callback of touch pad num %d", tp_num);
    TouchPadMsg.num = tp_num;
    TouchPadMsg.event = TOUCHPAD_EVENT_PUSH;
    xQueueSend( xQueueTouch, ( void * ) &TouchPadMsg, ( TickType_t ) 0 );
}

static void touchpad_release_cb(void *arg)
{
    tp_handle_t *tp_dev = (tp_handle_t *) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_AUDIO_LOGD("touch", "push callback of touch pad num %d", tp_num);
    TouchPadMsg.num = tp_num;
    TouchPadMsg.event = TOUCHPAD_EVENT_RELEASE;
    xQueueSend( xQueueTouch, ( void * ) &TouchPadMsg, ( TickType_t ) 0 );
}

static void touchpad_tap_cb(void *arg)
{
    tp_handle_t *tp_dev = (tp_handle_t *) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_AUDIO_LOGD("touch", "push callback of touch pad num %d", tp_num);
    TouchPadMsg.num = tp_num;
    TouchPadMsg.event = TOUCHPAD_EVENT_TAP;
    xQueueSend( xQueueTouch, ( void * ) &TouchPadMsg, ( TickType_t ) 0 );
}

static void touchpad_long_press_cb(void *arg)
{
    tp_handle_t *tp_dev = (tp_handle_t *) arg;
    touch_pad_t tp_num = iot_tp_num_get(tp_dev);
    ESP_AUDIO_LOGD("touch", "push callback of touch pad num %d", tp_num);
    TouchPadMsg.num = tp_num;
    TouchPadMsg.event = TOUCHPAD_EVENT_LONG_PRESS;
    xQueueSend( xQueueTouch, ( void * ) &TouchPadMsg, ( TickType_t ) 0 );
}

static int TouchManagerActive(DeviceManager *self)
{
    TouchManager *manager = (TouchManager *) self;
    xQueueTouch = xQueueCreate(TOUCH_EVT_QUEUE_LEN, sizeof(touchpad_msg_t));
    configASSERT(xQueueTouch);

#ifndef ENABLE_ADC_BUTTON
    //touchpad

    /***** Touchpad New version *****/
    tp_dev0 = iot_tp_create(TOUCH_PAD_NUM4, TOUCHPAD_THRES_PERCENT_PAD0);
    iot_tp_add_cb(tp_dev0, TOUCHPAD_CB_TAP, touchpad_tap_cb, tp_dev0);
    // iot_tp_add_cb(tp_dev0, TOUCHPAD_CB_PUSH, touchpad_push_cb, tp_dev0);
    iot_tp_add_cb(tp_dev0, TOUCHPAD_CB_RELEASE, touchpad_release_cb, tp_dev0);
    iot_tp_add_custom_cb(tp_dev0, TOUCHPAD_LONG_PRESS_SEC_PAD0, touchpad_long_press_cb, tp_dev0);

    tp_dev1 = iot_tp_create(TOUCH_PAD_NUM8, TOUCHPAD_THRES_PERCENT_PAD1);
    iot_tp_add_cb(tp_dev1, TOUCHPAD_CB_TAP, touchpad_tap_cb, tp_dev1);
    // iot_tp_add_cb(tp_dev1, TOUCHPAD_CB_PUSH, touchpad_push_cb, tp_dev1);
    iot_tp_add_cb(tp_dev1, TOUCHPAD_CB_RELEASE, touchpad_release_cb, tp_dev1);
    iot_tp_add_custom_cb(tp_dev1, TOUCHPAD_LONG_PRESS_SEC_PAD1, touchpad_long_press_cb, tp_dev1);


    tp_dev2 = iot_tp_create(TOUCH_PAD_NUM7, TOUCHPAD_THRES_PERCENT_PAD2);
    iot_tp_add_cb(tp_dev2, TOUCHPAD_CB_TAP, touchpad_tap_cb, tp_dev2);
    // iot_tp_add_cb(tp_dev2, TOUCHPAD_CB_PUSH, touchpad_push_cb, tp_dev2);
    iot_tp_add_cb(tp_dev2, TOUCHPAD_CB_RELEASE, touchpad_release_cb, tp_dev2);
    iot_tp_add_custom_cb(tp_dev2, TOUCHPAD_LONG_PRESS_SEC_PAD2, touchpad_long_press_cb, tp_dev2);

    tp_dev3 = iot_tp_create(TOUCH_PAD_NUM9, TOUCHPAD_THRES_PERCENT_PAD3);
    iot_tp_add_cb(tp_dev3, TOUCHPAD_CB_TAP, touchpad_tap_cb, tp_dev3);
    // iot_tp_add_cb(tp_dev3, TOUCHPAD_CB_PUSH, touchpad_push_cb, tp_dev3);
    iot_tp_add_cb(tp_dev3, TOUCHPAD_CB_RELEASE, touchpad_release_cb, tp_dev3);
    iot_tp_add_custom_cb(tp_dev3, TOUCHPAD_LONG_PRESS_SEC_PAD3, touchpad_long_press_cb, tp_dev3);

    //button
    button_handle_t btnRec = button_dev_init(GPIO_REC, 0, BUTTON_ACTIVE_LEVEL);
    button_dev_add_tap_cb(BUTTON_PUSH_CB, GpioRecPushCb, "PUSH", 100 / portTICK_PERIOD_MS, btnRec);
    button_dev_add_tap_cb(BUTTON_RELEASE_CB, GpioRecReleaseCb, "RELEASE", 100 / portTICK_PERIOD_MS, btnRec);

    button_handle_t btnMode = button_dev_init(GPIO_MODE, 1, BUTTON_ACTIVE_LEVEL);
    //in order to support BUTTON_TAP_CB, then push and release must be enabled
    button_dev_add_tap_cb(BUTTON_TAP_CB, GpioModeTapCb, "TAP", 100 / portTICK_PERIOD_MS, btnMode);
    button_dev_add_tap_cb(BUTTON_PUSH_CB, GpioModePushCb, "PUSH", 100 / portTICK_PERIOD_MS, btnMode);
    button_dev_add_tap_cb(BUTTON_RELEASE_CB, GpioModeReleaseCb, "RELEASE", 100 / portTICK_PERIOD_MS, btnMode);
    button_dev_add_press_cb(0, GpioModePressCb, NULL, 2000 / portTICK_PERIOD_MS, btnMode);
    ESP_AUDIO_LOGI(TOUCH_TAG, "Button is touch");

#else
    ESP_AUDIO_LOGI(TOUCH_TAG, "Button is adc");
    adc_btn_init(btnCallback);

#if defined CONFIG_ESP_LYRATD_FT_DOSS_V1_0_BOARD
    button_handle_t wifi_set = button_dev_init(WIFI_SET_PIN, 1, BUTTON_ACTIVE_LEVEL);
    button_dev_add_press_cb(0, gpiosSetWifiCb, NULL, 2000 / portTICK_PERIOD_MS, wifi_set);

#endif // CONFIG_ESP_LYRATD_FT_DOSS_V1_0_BOARD
#endif // ENABLE_ADC_BUTTON
    if (xTaskCreatePinnedToCore(TouchManagerEvtTask,
                                "TouchManagerEvtTask",
                                TOUCH_MANAGER_TASK_STACK_SIZE,
                                manager,
                                TOUCH_MANAGER_TASK_PRIORITY,
                                &TouchManageHandle, xPortGetCoreID()) != pdPASS) {
        ESP_AUDIO_LOGE(TOUCH_TAG, "Error create TouchManagerTask");
        return -1;
    }
    return 0;
}

static void TouchManagerDeactive(DeviceManager *self)
{
    //TODO
    ESP_AUDIO_LOGI(TOUCH_TAG, "TouchManagerStop\r\n");
    vQueueDelete(xQueueTouch);
    xQueueTouch = NULL;
}

TouchManager *TouchManagerCreate(struct DeviceController *controller)
{
    if (!controller)
        return NULL;
    ESP_AUDIO_LOGI(TOUCH_TAG, "TouchManagerCreate\r\n");
    TouchManager *touch = (TouchManager *) calloc(1, sizeof(TouchManager));
    ESP_ERROR_CHECK(!touch);
    InitManager((DeviceManager *) touch, controller);
    touch->Based.active = TouchManagerActive;
    touch->Based.deactive = TouchManagerDeactive;
    touch->Based.notify = TouchpadEvtNotify;  //TODO
    return touch;
}
