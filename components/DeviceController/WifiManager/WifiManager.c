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
#include "freertos/event_groups.h"
#include "esp_audio_log.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_err.h"
#include "esp_system.h"
#include "sdkconfig.h"

#include "WifiManager.h"
#include "DeviceCommon.h"
#include "NotifyHelper.h"
#include "wifismartconfig.h"
#include "airkiss.h"
#include "wifibleconfig.h"
#include "led.h"
#include "userconfig.h"
#include "esp_audio_log.h"


#ifndef CONFIG_CLASSIC_BT_ENABLED

#define WIFICONFIG_TAG                              "WIFI_CONFIG"
#define WIFI_EVT_QUEUE_LEN                          4
#define WIFI_SMARTCONFIG_TASK_PRIORITY              1
#define WIFI_SMARTCONFIG_TASK_STACK_SIZE            (2048 + 512)

#define WIFI_BLECONFIG_TASK_PRIORITY              1
#define WIFI_BLECONFIG_TASK_STACK_SIZE            (2048)

static int HighWaterLine = 0;
TaskHandle_t WifiManageHandle;


#define WIFI_MANAGER_TASK_PRIORITY                  3
#define WIFI_MANAGER_TASK_STACK_SIZE                (2048 + 512)
#define SMARTCONFIG_TIMEOUT_TICK                    (60000 / portTICK_RATE_MS)
#define BLECONFIG_TIMEOUT_TICK                      (60000 / portTICK_RATE_MS)
#define WIFI_SSID CONFIG_ESP_AUDIO_WIFI_SSID
#define  WIFI_PASS CONFIG_ESP_AUDIO_WIFI_PWD

QueueHandle_t xQueueWifi = NULL;
extern EventGroupHandle_t g_sc_event_group;
TaskHandle_t SmartConfigHandle = NULL;
TaskHandle_t BleConfigHandle = NULL;
TaskHandle_t airkiss_notify_handle = NULL;
#if CONFIG_ENABLE_BLE_CONFIG
extern uint8_t gl_sta_bssid[6];
extern uint8_t gl_sta_ssid[32];
extern int gl_sta_ssid_len;
extern EventGroupHandle_t g_ble_event_group;
#endif
static int retry_wifi_times = 5;

static wifi_config_t sta_config = {
    .sta = {
        .ssid = WIFI_SSID,
        .password = WIFI_PASS,
        .bssid_set = false
    }
};

static esp_err_t wifiEvtHandlerCb(void* ctx, system_event_t* evt)
{
    WifiState g_WifiState;
    static char lastState = -1;

    if (evt == NULL) {
        return ESP_FAIL;
    }
    switch (evt->event_id) {
    case SYSTEM_EVENT_WIFI_READY:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WIFI:READY");
        break;

    case SYSTEM_EVENT_SCAN_DONE:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+SCANDONE");
        break;

    case SYSTEM_EVENT_STA_START:
        g_WifiState = WifiState_Connecting;
        xQueueSend(xQueueWifi, &g_WifiState, 0);
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WIFI:STA_START");

        break;

    case SYSTEM_EVENT_STA_STOP:

        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WIFI:STA_STOP");
        break;

    case SYSTEM_EVENT_STA_CONNECTED:
        g_WifiState = WifiState_Connected;
        xQueueSend(xQueueWifi, &g_WifiState, 0);
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+JAP:WIFICONNECTED");
        break;

    case SYSTEM_EVENT_STA_DISCONNECTED:
        if (lastState != SYSTEM_EVENT_STA_DISCONNECTED) {
            g_WifiState = WifiState_Disconnected;
            LedIndicatorSet(LedIndexNet, LedWorkState_NetDisconnect);
        }

        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+JAP:DISCONNECTED,%u,%u", 0, evt->event_info.disconnected.reason);
        EventBits_t uxBits = xEventGroupWaitBits(g_sc_event_group, SC_PAIRING, true, false, 0);
#if CONFIG_ENABLE_BLE_CONFIG
        EventBits_t uxBleBits = xEventGroupWaitBits(g_ble_event_group, BLE_CONFIGING, true, false, 0);
        uxBleBits |= xEventGroupWaitBits(g_ble_event_group, BLE_PAIRING, false, false, 0);
#endif
        // WiFi connected event
#if CONFIG_ENABLE_BLE_CONFIG

        if (!(uxBleBits & BLE_PAIRING) && !(uxBleBits & BLE_CONFIGING)) {
#else

        if (!(uxBits & SC_PAIRING)) {
#endif

            if (lastState != SYSTEM_EVENT_STA_DISCONNECTED) {
                xQueueSend(xQueueWifi, &g_WifiState, 0);
            }
            ESP_AUDIO_LOGI(WIFICONFIG_TAG, "Autoconnect");
            // ESP_AUDIO_LOGI(WIFICONFIG_TAG, "Autoconnect to Wi-Fi ssid:%s,pwd:%s.", WIFI_SSID, WIFI_PASS);
            esp_wifi_set_config(WIFI_IF_STA, &sta_config);
            retry_wifi_times--;
            if (retry_wifi_times){
                if(ESP_FAIL == esp_wifi_connect()) {
                    ESP_AUDIO_LOGE(WIFICONFIG_TAG, "esp_wifi_connect failed");
                }
            } else{
                ESP_AUDIO_LOGW(WIFICONFIG_TAG, "Autoconnect Wi-Fi failed");
            }
        } else {
            if (lastState != SYSTEM_EVENT_STA_DISCONNECTED) {
#ifdef CONFIG_ENABLE_SMART_CONFIG
                g_WifiState = WifiState_SC_Disconnected;
#elif (defined CONFIG_ENABLE_BLE_CONFIG)
                g_WifiState = WifiState_BLE_Disconnected;
#endif
                xQueueSend(xQueueWifi, &g_WifiState, 0);
            }

            ESP_AUDIO_LOGI(WIFICONFIG_TAG, "don't Autoconnect due to smart_config or ble config!");
        }

        lastState = SYSTEM_EVENT_STA_DISCONNECTED;
#if CONFIG_ENABLE_BLE_CONFIG
        memset(gl_sta_ssid, 0, 32);
        memset(gl_sta_bssid, 0, 6);
        gl_sta_ssid_len = 0;
#endif
        break;

    case SYSTEM_EVENT_STA_AUTHMODE_CHANGE:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+JAP:AUTHCHANGED,%d,%d",
                 evt->event_info.auth_change.new_mode,
                 evt->event_info.auth_change.old_mode);
        break;

    case SYSTEM_EVENT_STA_GOT_IP: {
            if (lastState != SYSTEM_EVENT_STA_GOT_IP) {
                g_WifiState = WifiState_GotIp;
                xQueueSend(xQueueWifi, &g_WifiState, 0);
                LedIndicatorSet(LedIndexNet, LedWorkState_NetConnectOk);
            }

            wifi_config_t w_config;
            memset(&w_config, 0x00, sizeof(wifi_config_t));

            if (ESP_OK == esp_wifi_get_config(WIFI_IF_STA, &w_config)) {
                ESP_AUDIO_LOGI(WIFICONFIG_TAG, ">>>>>> Connected Wifi SSID:%s <<<<<<< \r\n", w_config.sta.ssid);
            } else {
                ESP_AUDIO_LOGE(WIFICONFIG_TAG, "Got wifi config failed");
            }

            ESP_AUDIO_LOGI(WIFICONFIG_TAG, "SYSTEM_EVENT_STA_GOTIP");
            lastState = SYSTEM_EVENT_STA_GOT_IP;
            retry_wifi_times = 5;
#if CONFIG_ENABLE_BLE_CONFIG
            esp_blufi_extra_info_t info;
            wifi_mode_t mode;
            esp_wifi_get_mode(&mode);
            memset(&info, 0, sizeof(esp_blufi_extra_info_t));
            memcpy(info.sta_bssid, gl_sta_bssid, 6);
            info.sta_bssid_set = true;
            info.sta_ssid = gl_sta_ssid;
            info.sta_ssid_len = gl_sta_ssid_len;
            esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
#endif
            break;
        }

    case SYSTEM_EVENT_STA_WPS_ER_SUCCESS:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WPS:SUCCEED");
        // esp_wifi_wps_disable();
        //esp_wifi_connect();
        break;

    case SYSTEM_EVENT_STA_WPS_ER_FAILED:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WPS:FAILED");
        break;

    case SYSTEM_EVENT_STA_WPS_ER_TIMEOUT:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WPS:TIMEOUT");
        // esp_wifi_wps_disable();
        //esp_wifi_disconnect();
        break;

    case SYSTEM_EVENT_STA_WPS_ER_PIN: {
//                char pin[9] = {0};
//                memcpy(pin, event->event_info.sta_er_pin.pin_code, 8);
//                ESP_AUDIO_LOGI(APP_TAG, "+WPS:PIN: [%s]", pin);
            break;
        }

    case SYSTEM_EVENT_AP_START:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WIFI:AP_START");
        break;

    case SYSTEM_EVENT_AP_STOP:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+WIFI:AP_STOP");
        break;

    case SYSTEM_EVENT_AP_STACONNECTED:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+SOFTAP:STACONNECTED,"MACSTR,
                 MAC2STR(evt->event_info.sta_connected.mac));
        break;

    case SYSTEM_EVENT_AP_STADISCONNECTED:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+SOFTAP:STADISCONNECTED,"MACSTR,
                 MAC2STR(evt->event_info.sta_disconnected.mac));
        break;

    case SYSTEM_EVENT_AP_PROBEREQRECVED:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "+PROBEREQ:"MACSTR",%d",MAC2STR(evt->event_info.ap_probereqrecved.mac),
                evt->event_info.ap_probereqrecved.rssi);
        break;
    default:
        break;
    }
    return ESP_OK;
}

//@warning: must be called by app_main task
static void WifiStartUp(void)
{
    wifi_config_t w_config;
    memset(&w_config, 0x00, sizeof(wifi_config_t));
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    //TODO: used memorized SSID and PASSWORD
    ESP_ERROR_CHECK(esp_event_loop_init(wifiEvtHandlerCb, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    if (ESP_OK == esp_wifi_get_config(WIFI_IF_STA, &w_config)) {
        if(w_config.sta.ssid[0] != 0){
            //if set before
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &w_config));
        }else{
            ESP_AUDIO_LOGE(WIFICONFIG_TAG, "Connect to default Wi-Fi SSID:%s", sta_config.sta.ssid);
            ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &sta_config));
        }
    } else {
        ESP_AUDIO_LOGE(WIFICONFIG_TAG, "get wifi config failed!");
        return;
    }
    ESP_ERROR_CHECK(esp_wifi_start());
    ESP_ERROR_CHECK(esp_wifi_connect());
    SmartconfigSetup(SC_TYPE_ESPTOUCH_AIRKISS, true);
#if CONFIG_ENABLE_BLE_CONFIG
    BleconfigSetup();
#endif

}

/******************************************************************************
 * FunctionName : smartconfig_task
 * Description  : start the samrtconfig proces and call back
 * Parameters   : pvParameters
 * Returns      : none
*******************************************************************************/
static void WifiConfigTask(void *pvParameters)
{
    ESP_AUDIO_LOGI(WIFICONFIG_TAG, "WifiConfig task running");
    xEventGroupSetBits(g_sc_event_group, SC_PAIRING);
    esp_wifi_disconnect();
    vTaskDelay(1000);
    wifi_mode_t curMode = WIFI_MODE_NULL;
    WifiState g_WifiState;
    if ((ESP_OK == esp_wifi_get_mode(&curMode)) && (curMode == WIFI_MODE_STA)) {
        ESP_AUDIO_LOGD(WIFICONFIG_TAG, "Smartconfig start,heap=%d\n", esp_get_free_heap_size());
        esp_err_t res = WifiSmartConnect((TickType_t) SMARTCONFIG_TIMEOUT_TICK);
        //LedIndicatorSet(LedIndexNet, LedWorkState_NetSetting);
        switch (res) {
            case ESP_OK:
                // xEventGroupWaitBits(g_sc_event_group, GOT_IP_BIT,
                    // false, true, portMAX_DELAY);
#ifdef CONFIG_ENABLE_TURINGWECHAT_SERVICE
                if(airkiss_notify_handle != NULL){
                    vTaskDelete(airkiss_notify_handle);
                    airkiss_notify_handle = NULL;
                }
                xTaskCreate(airkiss_notify_wechat, "airkiss_notify_wechat", 3*1024, NULL,
                            6, &airkiss_notify_handle);
#endif
        case ESP_ERR_INVALID_STATE:
            ESP_AUDIO_LOGE(WIFICONFIG_TAG, "Already in smartconfig, please wait");
            break;
        case ESP_ERR_TIMEOUT:
            g_WifiState = WifiState_Config_Timeout;
            xQueueSend(xQueueWifi, &g_WifiState, 0);
            ESP_AUDIO_LOGI(WIFICONFIG_TAG, "Smartconfig timeout");
            break;
        case ESP_FAIL:
            ESP_AUDIO_LOGI(WIFICONFIG_TAG, "Smartconfig failed, please try again");
            break;
        default:
            break;
        }
    } else {
        ESP_AUDIO_LOGE(WIFICONFIG_TAG, "Invalid wifi mode");
    }
#if EN_STACK_TRACKER
    ESP_AUDIO_LOGI("STACK", "WifiConfigTask %d\n\n\n", uxTaskGetStackHighWaterMark(SmartConfigHandle));
#endif
    SmartConfigHandle = NULL;
    vTaskDelete(NULL);
}

//public interface
static void WifiSmartConfig(WifiManager *manager)
{
    //configASSERT(manager);
    if(SmartConfigHandle != NULL) {
        vTaskDelete(SmartConfigHandle);
        SmartConfigHandle = NULL;
    }

    if(xTaskCreate(WifiConfigTask,
                   "WifiConfigTask",
                   WIFI_SMARTCONFIG_TASK_STACK_SIZE,
                   NULL,
                   WIFI_SMARTCONFIG_TASK_PRIORITY,
                   &SmartConfigHandle) != pdPASS) {

        ESP_AUDIO_LOGE(WIFICONFIG_TAG, "ERROR creating WifiConfigTask task! Out of memory?");
    }
}

#if CONFIG_ENABLE_BLE_CONFIG
/******************************************************************************
 * FunctionName : BleConfigTask
 * Description  : start the Bleconfig proces and call back
 * Parameters   : pvParameters
 * Returns      : none
*******************************************************************************/
static void BleConfigTask(void *pvParameters)
{
    BleConfigStop();
    xEventGroupClearBits(g_ble_event_group, BLE_STOP_REQ_EVT);
    xEventGroupClearBits(g_ble_event_group, BLE_CONFIGING);

    xEventGroupSetBits(g_ble_event_group, BLE_PAIRING);
    ESP_AUDIO_LOGI(WIFICONFIG_TAG, "BleConfig task running");
    wifi_mode_t curMode = WIFI_MODE_NULL;
    ESP_AUDIO_LOGD(WIFICONFIG_TAG, "BleConfig start,heap=%d\n", esp_get_free_heap_size());
    esp_wifi_disconnect();

    WifiState g_WifiState;
    esp_err_t res = BleConfigStart((TickType_t) BLECONFIG_TIMEOUT_TICK);
    //LedIndicatorSet(LedIndexNet, LedWorkState_NetSetting);
    switch (res) {
    case ESP_ERR_INVALID_STATE:
        ESP_AUDIO_LOGE(WIFICONFIG_TAG, "Already in BleConfig, please wait");
        break;
    case ESP_ERR_TIMEOUT:
        g_WifiState = WifiState_Config_Timeout;
        xQueueSend(xQueueWifi, &g_WifiState, 0);
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "BleConfig timeout");
        break;
    case ESP_FAIL:
        ESP_AUDIO_LOGI(WIFICONFIG_TAG, "BleConfig failed, please try again");
        break;
    default:
        break;
    }
    BleConfigHandle = NULL;
    vTaskDelete(NULL);
}

//public interface
static void WifiBleConfig(WifiManager *manager)
{
    //configASSERT(manager);
    if (BleConfigHandle != NULL) {
        vTaskDelete(BleConfigHandle);
        BleConfigHandle = NULL;
    }

    if(xTaskCreate(BleConfigTask,
                   "BleConfigTask",
                   WIFI_BLECONFIG_TASK_STACK_SIZE,
                   NULL,
                   WIFI_BLECONFIG_TASK_PRIORITY,
                   &BleConfigHandle) != pdPASS) {

        ESP_AUDIO_LOGE(WIFICONFIG_TAG, "ERROR creating BleConfigTask task! Out of memory?");
    }
}

static void WifiBleConfigStop(WifiManager *manager)
{
    BleConfigStop();
}
#endif
static void ProcessWifiEvent(WifiManager *manager, void *evt)
{
    WifiState state = *((WifiState *) evt);
    DeviceNotifyMsg msg = DEVICE_NOTIFY_WIFI_UNDEFINED;
    switch (state) {
    case WifiState_GotIp:
        msg = DEVICE_NOTIFY_WIFI_GOT_IP;
        break;
    case WifiState_Disconnected:
        msg = DEVICE_NOTIFY_WIFI_DISCONNECTED;
        break;
    case WifiState_SC_Disconnected:
        msg = DEVICE_NOTIFY_WIFI_SC_DISCONNECTED;
        break;
    case WifiState_BLE_Disconnected:
        msg = DEVICE_NOTIFY_WIFI_BLE_DISCONNECTED;
        break;
    case WifiState_Config_Timeout:
        msg = DEVICE_NOTIFY_WIFI_SETTING_TIMEOUT;
        break;
    case WifiState_BLE_Stop:
#ifdef CONFIG_ENABLE_BLE_CONFIG
        BleConfigStop();
#endif
        break;

    case WifiState_Connecting:
    case WifiState_Connected:
    case WifiState_ConnectFailed:
    default:
        break;
    }

    if (msg != DEVICE_NOTIFY_WIFI_UNDEFINED) {
        manager->Based.notify((struct DeviceController *) manager->Based.controller, DEVICE_NOTIFY_TYPE_WIFI, &msg, sizeof(DeviceNotifyMsg));
    }
}

static void WifiManagerEvtTask(void *pvParameters)
{
    WifiManager *manager = (WifiManager *) pvParameters;
    WifiState state = WifiState_Unknow;
    while (1) {
        if (xQueueReceive(xQueueWifi, &state, portMAX_DELAY)) {
            ProcessWifiEvent(manager, &state);
#if EN_STACK_TRACKER
            if(uxTaskGetStackHighWaterMark(WifiManageHandle) > HighWaterLine){
                HighWaterLine = uxTaskGetStackHighWaterMark(WifiManageHandle);
                ESP_AUDIO_LOGI("STACK", "%s %d\n\n\n", __func__, HighWaterLine);
            }
#endif
        }
    }
    vTaskDelete(NULL);
}


static int WifiConfigActive(DeviceManager *self)
{
    ESP_AUDIO_LOGI(WIFICONFIG_TAG, "WifiConfigActive, freemem %d", esp_get_free_heap_size());
    WifiManager *manager = (WifiManager *) self;
    xQueueWifi = xQueueCreate(WIFI_EVT_QUEUE_LEN, sizeof(WifiState));
    configASSERT(xQueueWifi);
    WifiStartUp();

    if (xTaskCreatePinnedToCore(WifiManagerEvtTask,
                                "WifiManagerEvtTask",
                                WIFI_MANAGER_TASK_STACK_SIZE,
                                manager,
                                WIFI_MANAGER_TASK_PRIORITY,
                                &WifiManageHandle, xPortGetCoreID()) != pdPASS) {
        ESP_AUDIO_LOGE(WIFICONFIG_TAG, "Error create WifiManagerTask");
        return -1;
    }
    return 0;
}

static void WifiConfigDeactive(DeviceManager *self)
{
    ESP_AUDIO_LOGI(WIFICONFIG_TAG, "WifiConfigStop\r\n");

}

WifiManager *WifiConfigCreate(struct DeviceController *controller)
{
    if (!controller)
        return NULL;
    ESP_AUDIO_LOGI(WIFICONFIG_TAG, "WifiConfigCreate\r\n");
    WifiManager *wifi = (WifiManager *) calloc(1, sizeof(WifiManager));
    ESP_ERROR_CHECK(!wifi);

    InitManager((DeviceManager *) wifi, controller);

    wifi->Based.active = WifiConfigActive;
    wifi->Based.deactive = WifiConfigDeactive;
    wifi->Based.notify = WifiEvtNotify;  //TODO
    wifi->smartConfig = WifiSmartConfig;  //TODO
#if CONFIG_ENABLE_BLE_CONFIG
    wifi->bleConfig = WifiBleConfig;  //TODO
    wifi->bleConfigStop = WifiBleConfigStop;
#endif
    return wifi;
}

#else

WifiManager *WifiConfigCreate(struct DeviceController *controller)
{
    return NULL;
}

#endif
