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
#include <assert.h>
#include <math.h>
#include "sdkconfig.h"

#ifdef CONFIG_BLUEDROID_ENABLED
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "esp_event.h"

#include "esp_audio_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_bt.h"

#include "esp_blufi_api.h"
#include "esp_bt_defs.h"
#include "esp_gap_ble_api.h"
#include "esp_bt_device.h"
#include "esp_bt_main.h"
#include "esp_gap_bt_api.h"
#include "esp_a2dp_api.h"
#include "esp_avrc_api.h"

#include "key_control.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"

#include "../DeviceController/DeviceCommon.h"
#include "MediaControl.h"
#include "AudioCodec.h"
#include "toneuri.h"
#include "AudioDef.h"
#include "EspAudioAlloc.h"
#include "wifibleconfig.h"
#include "userconfig.h"
#include "esp_heap_alloc_caps.h"
#include "WifiManager.h"
#include "blufi_security.h"

#include "mbedtls/aes.h"
#include "mbedtls/dhm.h"
#include "mbedtls/md5.h"
#include "rom/crc.h"

#define WIFI_BLE_TAG "WIFI_BLE_CONFIG"


EventGroupHandle_t g_ble_event_group = NULL;
static xSemaphoreHandle g_ble_mux = NULL;

static void audio_ble_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param);

#define BLUFI_DEVICE_NAME            "BLUFI_DEVICE"
static uint8_t audio_ble_service_uuid128[32] = {
    /* LSB <--------------------------------------------------------------------------------> MSB */
    //first uuid, 16bit, [12],[13] is the value
    0xfb, 0x34, 0x9b, 0x5f, 0x80, 0x00, 0x00, 0x80, 0x00, 0x10, 0x00, 0x00, 0xFF, 0xFF, 0x00, 0x00,
};

//static uint8_t test_manufacturer[TEST_MANUFACTURER_DATA_LEN] =  {0x12, 0x23, 0x45, 0x56};
static esp_ble_adv_data_t audio_ble_adv_data = {
    .set_scan_rsp = false,
    .include_name = true,
    .include_txpower = true,
    .min_interval = 0x100,
    .max_interval = 0x100,
    .appearance = 0x00,
    .manufacturer_len = 0,
    .p_manufacturer_data =  NULL,
    .service_data_len = 0,
    .p_service_data = NULL,
    .service_uuid_len = 16,
    .p_service_uuid = audio_ble_service_uuid128,
    .flag = 0x6,
};

static esp_ble_adv_params_t audio_ble_adv_params = {
    .adv_int_min        = 0x100,
    .adv_int_max        = 0x100,
    .adv_type           = ADV_TYPE_IND,
    .own_addr_type      = BLE_ADDR_TYPE_PUBLIC,
    //.peer_addr            =
    //.peer_addr_type       =
    .channel_map        = ADV_CHNL_ALL,
    .adv_filter_policy = ADV_FILTER_ALLOW_SCAN_ANY_CON_ANY,
};

#define WIFI_LIST_NUM   10

static wifi_config_t sta_config;
static wifi_config_t ap_config;

/* store the station info for send back to phone */
uint8_t gl_sta_bssid[6];
uint8_t gl_sta_ssid[32];
int gl_sta_ssid_len;

/* connect infor*/
static uint8_t server_if;
static uint16_t conn_id;


static esp_blufi_callbacks_t audio_ble_callbacks = {
    .event_cb = audio_ble_event_callback,
    .negotiate_data_handler = blufi_dh_negotiate_data_handler,
    .encrypt_func = blufi_aes_encrypt,
    .decrypt_func = blufi_aes_decrypt,
    .checksum_func = blufi_crc_checksum,
};
extern QueueHandle_t xQueueWifi;

static void audio_ble_event_callback(esp_blufi_cb_event_t event, esp_blufi_cb_param_t *param)
{
    /* actually, should post to blufi_task handle the procedure,
     * now, as a audio_ble, we do it more simply */
    esp_err_t ret ;
    switch (event) {
    case ESP_BLUFI_EVENT_INIT_FINISH:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI init finish\n");

        esp_ble_gap_set_device_name(BLUFI_DEVICE_NAME);
        esp_ble_gap_config_adv_data(&audio_ble_adv_data);
        break;
    case ESP_BLUFI_EVENT_DEINIT_FINISH:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI deinit finish\n");
        break;
    case ESP_BLUFI_EVENT_BLE_CONNECT:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI ble connect\n");
        server_if=param->connect.server_if;
        conn_id=param->connect.conn_id;

        break;
    case ESP_BLUFI_EVENT_BLE_DISCONNECT:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI ble disconnect\n");
        int state = WifiState_BLE_Stop;
        xQueueSend(xQueueWifi, &state, 0);
        break;
    case ESP_BLUFI_EVENT_SET_WIFI_OPMODE:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI Set WIFI opmode %d\n", param->wifi_mode.op_mode);
        ESP_ERROR_CHECK( esp_wifi_set_mode(param->wifi_mode.op_mode) );
        break;
    case ESP_BLUFI_EVENT_REQ_CONNECT_TO_AP:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI requset wifi connect to AP\n");
        xEventGroupSetBits(g_ble_event_group, BLE_CONFIGING);
        esp_wifi_disconnect();
        if(ESP_OK != esp_wifi_connect()){
            xEventGroupSetBits(g_ble_event_group, BLE_STOP_REQ_EVT);
        }
        break;
    case ESP_BLUFI_EVENT_REQ_DISCONNECT_FROM_AP:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI requset wifi disconnect from AP\n");
        esp_wifi_disconnect();
        break;
    case ESP_BLUFI_EVENT_GET_WIFI_STATUS: {
        wifi_mode_t mode;
        esp_blufi_extra_info_t info;

        esp_wifi_get_mode(&mode);

        memset(&info, 0, sizeof(esp_blufi_extra_info_t));
        memcpy(info.sta_bssid, gl_sta_bssid, 6);
        info.sta_bssid_set = true;
        info.sta_ssid = gl_sta_ssid;
        info.sta_ssid_len = gl_sta_ssid_len;
        esp_blufi_send_wifi_conn_report(mode, ESP_BLUFI_STA_CONN_SUCCESS, 0, &info);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"BLUFI get wifi status from AP\n");

        break;
    }
    case ESP_BLUFI_EVENT_RECV_SLAVE_DISCONNECT_BLE:
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"blufi close a gatt connection");
        esp_blufi_close(server_if,conn_id);
        break;
    case ESP_BLUFI_EVENT_DEAUTHENTICATE_STA:
        /* TODO */
        break;
    case ESP_BLUFI_EVENT_RECV_STA_BSSID:
        memcpy(sta_config.sta.bssid, param->sta_bssid.bssid, 6);
        sta_config.sta.bssid_set = 1;
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv STA BSSID %s\n", sta_config.sta.bssid);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_SSID:
        strncpy((char *)sta_config.sta.ssid, (char *)param->sta_ssid.ssid, param->sta_ssid.ssid_len);
        sta_config.sta.ssid[param->sta_ssid.ssid_len] = '\0';
        ret = esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv STA SSID ret %d %s\n", ret,sta_config.sta.ssid);
        break;
    case ESP_BLUFI_EVENT_RECV_STA_PASSWD:
        strncpy((char *)sta_config.sta.password, (char *)param->sta_passwd.passwd, param->sta_passwd.passwd_len);
        sta_config.sta.password[param->sta_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_STA, &sta_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv STA PASSWORD %s\n", sta_config.sta.password);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_SSID:
        strncpy((char *)ap_config.ap.ssid, (char *)param->softap_ssid.ssid, param->softap_ssid.ssid_len);
        ap_config.ap.ssid[param->softap_ssid.ssid_len] = '\0';
        ap_config.ap.ssid_len = param->softap_ssid.ssid_len;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv SOFTAP SSID %s, ssid len %d\n", ap_config.ap.ssid, ap_config.ap.ssid_len);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_PASSWD:
        strncpy((char *)ap_config.ap.password, (char *)param->softap_passwd.passwd, param->softap_passwd.passwd_len);
        ap_config.ap.password[param->softap_passwd.passwd_len] = '\0';
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv SOFTAP PASSWORD %s\n", ap_config.ap.password);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_MAX_CONN_NUM:
        if (param->softap_max_conn_num.max_conn_num > 4) {
            return;
        }
        ap_config.ap.max_connection = param->softap_max_conn_num.max_conn_num;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv SOFTAP MAX CONN NUM %d\n", ap_config.ap.max_connection);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_AUTH_MODE:
        if (param->softap_auth_mode.auth_mode >= WIFI_AUTH_MAX) {
            return;
        }
        ap_config.ap.authmode = param->softap_auth_mode.auth_mode;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv SOFTAP AUTH MODE %d\n", ap_config.ap.authmode);
        break;
    case ESP_BLUFI_EVENT_RECV_SOFTAP_CHANNEL:
        if (param->softap_channel.channel > 13) {
            return;
        }
        ap_config.ap.channel = param->softap_channel.channel;
        esp_wifi_set_config(WIFI_IF_AP, &ap_config);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG ,"Recv SOFTAP CHANNEL %d\n", ap_config.ap.channel);
        break;
    case ESP_BLUFI_EVENT_RECV_USERNAME:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_CA_CERT:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_CLIENT_CERT:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_SERVER_CERT:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_CLIENT_PRIV_KEY:
        /* Not handle currently */
        break;
    case ESP_BLUFI_EVENT_RECV_SERVER_PRIV_KEY:
        /* Not handle currently */
        break;
    default:
        break;
    }
}

static void audio_ble_gap_event_handler(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    switch (event) {
    case ESP_GAP_BLE_ADV_DATA_SET_COMPLETE_EVT:
        esp_ble_gap_start_advertising(&audio_ble_adv_params);
        break;
    default:
        break;
    }
}

esp_err_t BleconfigSetup(void)
{
    if (g_ble_event_group == NULL) {
        g_ble_event_group = xEventGroupCreate();
        if(g_ble_event_group == NULL){
           ESP_AUDIO_LOGE(WIFI_BLE_TAG, "g_ble_event_group creat failed!");
           return ESP_FAIL;
        }
    }
    if (g_ble_mux == NULL) {
        g_ble_mux = xSemaphoreCreateMutex();
        if (g_ble_mux == NULL) {
            ESP_AUDIO_LOGE(WIFI_BLE_TAG, "g_ble_mux creat failed!");
            vEventGroupDelete(g_ble_event_group);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

void BleConfigStop(void)
{
    EventBits_t uxBleBits = xEventGroupWaitBits(g_ble_event_group, BLE_PAIRING, false, false, 0);
    ESP_AUDIO_LOGI(WIFI_BLE_TAG, "BleConfigStop: 0x%x", uxBleBits);
    if ((uxBleBits & BLE_PAIRING)) {
        xEventGroupClearBits(g_ble_event_group, BLE_PAIRING);
        ESP_AUDIO_LOGI(WIFI_BLE_TAG, "BleConfigStop start");
        blufi_security_deinit();
        esp_blufi_profile_deinit();
        esp_bluedroid_disable();
        esp_bluedroid_deinit();
        ESP_AUDIO_LOGI(WIFI_BLE_TAG, "BleConfigStop done");
        // esp_bt_controller_disable();
        // esp_bt_controller_deinit();
    }
}

esp_err_t BleConfigStart(uint32_t ticks_to_wait)
{
    ESP_AUDIO_LOGI(WIFI_BLE_TAG, "BleConfigStart\r\n");
    esp_err_t ret;

    portBASE_TYPE res = xSemaphoreTake(g_ble_mux, ticks_to_wait);
    if (res != pdPASS) {
        return ESP_ERR_TIMEOUT;
    }
    ESP_AUDIO_LOGI(WIFI_BLE_TAG, "xSemaphoreTake(g_ble_mux------\r\n");
    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    if(esp_bt_controller_get_status() == ESP_BT_CONTROLLER_STATUS_IDLE){
        if (esp_bt_controller_init(&bt_cfg) != ESP_OK) {
            ESP_AUDIO_LOGE(WIFI_BLE_TAG, "%s initialize controller failed\n", __func__);
            xSemaphoreGive(g_ble_mux);
            return ESP_FAIL;
        }
    
        if (esp_bt_controller_enable(ESP_BT_MODE_BLE) != ESP_OK) {
            ESP_AUDIO_LOGE(WIFI_BLE_TAG, "%s enable controller failed\n", __func__);
            xSemaphoreGive(g_ble_mux);
            return ESP_FAIL;
        }
    }
    if(esp_bluedroid_get_status() == ESP_BLUEDROID_STATUS_UNINITIALIZED){
        ret = esp_bluedroid_init();
        if (ret) {
            ESP_AUDIO_LOGE(WIFI_BLE_TAG, "%s esp_bluedroid_init failed", __func__);
            xSemaphoreGive(g_ble_mux);
            return ret;
        }
        ret = esp_bluedroid_enable();
        if (ret) {
            ESP_AUDIO_LOGE(WIFI_BLE_TAG, "%s esp_bluedroid_enable failed", __func__);
            xSemaphoreGive(g_ble_mux);
            return ret;
        }
    }
    ESP_AUDIO_LOGI(WIFI_BLE_TAG , "BD ADDR: "ESP_BD_ADDR_STR"\n", ESP_BD_ADDR_HEX(esp_bt_dev_get_address()));

    ESP_AUDIO_LOGI(WIFI_BLE_TAG , "BLUFI VERSION %04x\n", esp_blufi_get_version());

    blufi_security_init();
    esp_ble_gap_register_callback(audio_ble_gap_event_handler);

    esp_blufi_register_callbacks(&audio_ble_callbacks);
    esp_blufi_profile_init();
    EventBits_t uxBits;
    uxBits = xEventGroupWaitBits(g_ble_event_group, BLE_DONE_EVT | BLE_STOP_REQ_EVT, true, false, ticks_to_wait);
    // WiFi connected event
    if (uxBits & BLE_DONE_EVT) {
        ESP_AUDIO_LOGD(WIFI_BLE_TAG, "WiFi connected");
        ret = ESP_OK;
    }
    // WiFi stop connecting event
    else if (uxBits & BLE_STOP_REQ_EVT) {
        ESP_AUDIO_LOGD(WIFI_BLE_TAG, "Bleconfig stop. %d",__LINE__);
        ret = ESP_FAIL;
        BleConfigStop();
    }
    // WiFi connect timeout
    else {
        esp_wifi_stop();
        ESP_AUDIO_LOGE(WIFI_BLE_TAG, "WiFi connect fail");
        ret = ESP_ERR_TIMEOUT;
        BleConfigStop();
    }
    xSemaphoreGive(g_ble_mux);
    return ret;
}
#endif
