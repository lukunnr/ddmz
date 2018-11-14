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

#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_audio_log.h"

#include "DuerAppService.h"
#include "DeviceCommon.h"

#include "esp_audio_device_info.h"
#include "lightduer_dev_info.h"
#include "lightduer_ota_notifier.h"
#include "duer_audio_wrapper.h"
#include "lightduer_voice.h"
#include "lightduer_net_ntp.h"
#include "lightduer_connagent.h"
#include "lightduer_dcs.h"
#include "duerapp_ota.h"
#include "duer_profile_config.h"

#include "EspAudioAlloc.h"
#include "Recorder.h"
#include "toneuri.h"
#include "recorder_engine.h"
#include "led.h"
#include "sdkconfig.h"
#include "duer_control_point.h"
#include "apps/sntp/sntp.h"

static const char *TAG = "DUEROS_SERVICE";

#define RECOG_SERVICE_TASK_PRIORITY     5
#define RECOG_SERVICE_TASK_SIZE         4*1024
#define RECORD_SAMPLE_RATE              (16000)
#define RECOGNITION_TRIGGER_URI         "i2s://16000:1@record.pcm#raw"

#define RECORD_DEBUG                    0
#define ENABLE_WAKEUP_BY_DSP            0
#define WAKEUP_NOTIFICATION_TYPE        0 // After wakeup  0: Playback Tone, 1: LED Bar;
// #define WAKEUP_TEST
#if WAKEUP_NOTIFICATION_TYPE
#include "led_bar.h"
#include "led_light.h"
#endif

static EventGroupHandle_t       regc_evt_group;
static DuerAppService          *localService;
static int                      set_wifi_flag;
static TimerHandle_t            retry_login_timer;
static TaskHandle_t             report_info_tsk_handle;
const static int CONNECT_BIT = BIT0;
static bool duer_sent_stop_flag;
static bool wifi_is_connected;
static bool mic_mute_flag;

typedef enum {
    DUER_CMD_UNKNOWN,
    DUER_CMD_LOGIN,
    DUER_CMD_CONNECTED,
    DUER_CMD_START,
    DUER_CMD_STOP,
    DUER_CMD_QUIT,
} duer_task_cmd_t;

typedef enum {
    DUER_STATE_IDLE,
    DUER_STATE_CONNECTING,
    DUER_STATE_CONNECTED,
    DUER_STATE_START,
    DUER_STATE_STOP,
} duer_task_state_t;

typedef struct {
    duer_task_cmd_t     type;
    uint32_t            *pdata;
    int                 index;
    int                 len;
} duer_task_msg_t;

static duer_task_state_t duer_state = DUER_STATE_IDLE;

static void duerOSEvtSend(void *que, duer_task_cmd_t type, void *data, int index, int len, int dir)
{
    duer_task_msg_t evt = {0};
    evt.type = type;
    evt.pdata = data;
    evt.index = index;
    evt.len = len;
    if (dir) {
        xQueueSendToFront(que, &evt, 0) ;
    } else {
        xQueueSend(que, &evt, 0);
    }
}

void rec_engine_cb(rec_event_type_t type)
{
    if (wifi_is_connected == false) {
        ESP_AUDIO_LOGW(TAG, "rec_engine_cb, Wi-Fi has not connected");
        duer_dcs_tone_handler(tone_uri[TONE_TYPE_PLEASE_SETTING_WIFI]);
        return;
    }

    if (REC_EVENT_WAKEUP_START == type) {
        ESP_AUDIO_LOGI(TAG, "--- rec_engine_cb --- REC_EVENT_WAKEUP_START, state:%d", duer_state);
#if WAKEUP_NOTIFICATION_TYPE
        LedIndicatorWakeup(1);
#else
        duer_dcs_tone_handler(tone_uri[TONE_TYPE_HELLO]);
#endif
        if (DUER_AUDIO_TYPE_SPEECH == duer_audio_wrapper_get_type()) { // speak
            localService->Based.stopTone((MediaService *)localService);
            duer_audio_wrapper_set_type(DUER_AUDIO_TYPE_UNKOWN);
        }
        if (duer_state == DUER_STATE_START) {
            return;
        }
    } else if (REC_EVENT_VAD_START == type) {
        ESP_AUDIO_LOGI(TAG, "--- rec_engine_cb --- REC_EVENT_VAD_START")
        duerOSEvtSend(localService->_duerOsQue, DUER_CMD_START, NULL, 0, 0, 0);
        duer_sent_stop_flag = false;
    } else if (REC_EVENT_VAD_STOP == type) {
        if ((duer_state == DUER_STATE_START) && (duer_sent_stop_flag == false)) {
#if WAKEUP_NOTIFICATION_TYPE
            LedIndicatorWakeup(0);
#endif
            duerOSEvtSend(localService->_duerOsQue, DUER_CMD_STOP, NULL, 0, 0, 0);
            duer_sent_stop_flag =  true;
            ESP_AUDIO_LOGE(TAG, "--- SEND --- REC_EVENT_VAD_STOP");
        }
        ESP_AUDIO_LOGI(TAG, "--- rec_engine_cb --- REC_EVENT_VAD_STOP");
    } else if (REC_EVENT_WAKEUP_END == type) {
        if ((duer_state == DUER_STATE_START) && (duer_sent_stop_flag == false)) {
            duerOSEvtSend(localService->_duerOsQue, DUER_CMD_STOP, NULL, 0, 0, 0);
            duer_sent_stop_flag =  true;
            ESP_AUDIO_LOGE(TAG, "--- SEND --- REC_EVENT_VAD_STOP");
        }
#if WAKEUP_NOTIFICATION_TYPE
        LedIndicatorWakeup(0);
#endif
        ESP_AUDIO_LOGI(TAG, "--- rec_engine_cb --- REC_EVENT_WAKEUP_END");
    } else {

    }
}

void retry_login_timer_cb(xTimerHandle tmr)
{
    ESP_AUDIO_LOGI(TAG, "Func:%s", __func__);
    duerOSEvtSend(localService->_duerOsQue, DUER_CMD_LOGIN, NULL, 0, 0, 0);
    xTimerStop(tmr, 0);
}

static void initialize_sntp(void)
{
    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();
}

static void obtain_time(void)
{
    initialize_sntp();
    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

static void setup_local_time()
{
    time_t now;
    struct tm timeinfo;
    char strftime_buf[64];
    time(&now);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
        obtain_time();
        // update 'now' variable with current time
        time(&now);
    }
    // Set timezone to China Standard Time
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time is: %s", strftime_buf);
}

static void report_info_task(void *pvParameters)
{
    int ret;
    static bool setup_local_time_flag;
    if (setup_local_time_flag == false) {
        setup_local_time();
        setup_local_time_flag = true;
    }

    ret = duer_report_device_info();
    if (ret != DUER_OK) {
        ESP_AUDIO_LOGE(TAG, "Report device info failed ret:%d", ret);
    }
    ret = duer_ota_notify_package_info();
    if (ret != DUER_OK) {
        ESP_AUDIO_LOGE(TAG, "Report package info failed ret:%d", ret);
    }
    report_info_tsk_handle = NULL;
    vTaskDelete(NULL);
}

static void duer_event_hook(duer_event_t *event)
{
    if (!event) {
        ESP_AUDIO_LOGE(TAG, "NULL event!!!");
    }
    ESP_AUDIO_LOGE(TAG, "event: %d", event->_event);
    switch (event->_event) {
        case DUER_EVENT_STARTED:
            // Initialize the DCS API
            duer_dcs_init();
            duer_control_point_init();
            duer_initialize_ota();

            duerOSEvtSend(localService->_duerOsQue, DUER_CMD_CONNECTED, NULL, 0, 0, 0);
            ESP_AUDIO_LOGE(TAG, "event: DUER_EVENT_STARTED");
            if (report_info_tsk_handle == NULL) {
                xTaskCreate(&report_info_task, "report_info_task", 1024 * 4, NULL, 5, &report_info_tsk_handle);
            }
            duer_dcs_tone_handler(tone_uri[TONE_TYPE_SERVER_CONNECT]);
            break;
        case DUER_EVENT_STOPPED:
            ESP_AUDIO_LOGE(TAG, "event: DUER_EVENT_STOPPED");
            if (duer_audio_wrapper_get_type() == DUER_AUDIO_TYPE_MUSIC) {
                duer_dcs_audio_on_stopped();
            }
            duer_control_point_deinit();
            rec_engine_trigger_stop();
            duerOSEvtSend(localService->_duerOsQue, DUER_CMD_QUIT, NULL, 0, 0, 0);
            if (set_wifi_flag == 0) {
                duer_dcs_tone_handler(tone_uri[TONE_TYPE_SERVER_DISCONNECT]);
            }
            break;
    }
}

static void duer_login(void)
{
    char *data = duer_load_profile();
    if (NULL == data) {
        ESP_AUDIO_LOGE(TAG, "Get duerOS profle failed");
        return;
    }
    int sz = strlen(data);
    ESP_AUDIO_LOGI(TAG, "duer_start, len:%d\n%s", sz, data);
    duer_start(data, sz);
    free((void *)data);
}

static esp_err_t recorder_open(void **handle)
{
#if (defined CONFIG_ESP_LYRATD_FT_V1_0_BOARD || defined CONFIG_ESP_LYRATD_FT_DOSS_V1_0_BOARD)
    int ret = EspAudioRecorderStart(RECOGNITION_TRIGGER_URI, 10 * 1024, 1);
#else
    int ret = EspAudioRecorderStart(RECOGNITION_TRIGGER_URI, 10 * 1024, 0);
#endif
    return ret;
}

static esp_err_t recorder_read(void *handle, char *data, int data_size)
{
    int len = 0;
    int ret = 0;
    if (mic_mute_flag == true) {
        memset(data, 0, data_size);
        len = data_size;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    } else {
        ret = EspAudioRecorderRead((uint8_t *)data, data_size, &len);
    }
    if (ret == 0) {
        return len;
    } else {
        return -1;
    }
}

static esp_err_t recorder_close(void *handle)
{
    int ret = EspAudioRecorderStop(TERMINATION_TYPE_NOW);
    return ret;
}

static void dueros_task(void *pvParameters)
{
    DuerAppService *service =  (DuerAppService *)pvParameters;
    duer_audio_wrapper_init();
    duer_initialize();
    duer_set_event_callback(duer_event_hook);
    duer_init_device_info();

    uint8_t *voiceData = EspAudioAlloc(1, REC_ONE_BLOCK_SIZE);
    if (NULL == voiceData) {
        ESP_AUDIO_LOGE(TAG, "Func:%s, Line:%d, Malloc failed", __func__, __LINE__);
        return;
    }
    static duer_task_msg_t duer_msg;
    xEventGroupWaitBits(regc_evt_group, CONNECT_BIT, false, true, 5000 / portTICK_PERIOD_MS);

    rec_config_t eng = DEFAULT_REC_ENGINE_CONFIG();
    eng.vad_off_delay_ms = 600;
    eng.wakeup_time_ms = 2000;
    eng.evt_cb = rec_engine_cb;
    eng.open = recorder_open;
    eng.close = recorder_close;
    eng.fetch = recorder_read;
    rec_engine_create(&eng);

    int retry_time = 1000 / portTICK_PERIOD_MS;
    int retry_num = 1;
    retry_login_timer = xTimerCreate("release", retry_time,
                                     pdFALSE, NULL, retry_login_timer_cb);
    FILE *file = NULL;
#if RECORD_DEBUG
    file = fopen("/sdcard/rec_debug_1.wav", "w+");
    if (NULL == file) {
        ESP_AUDIO_LOGW(TAG, "open rec_debug_1.wav failed,[%d]", __LINE__);
    }
#endif
    int task_run = 1;
    while (task_run) {
        if (xQueueReceive(service->_duerOsQue, &duer_msg, portMAX_DELAY)) {
            if (duer_msg.type == DUER_CMD_LOGIN) {
                ESP_AUDIO_LOGI(TAG, "Recv Que DUER_CMD_LOGIN");
                if (duer_state == DUER_STATE_IDLE) {
                    duer_login();
                    duer_state = DUER_STATE_CONNECTING;
                } else {
                    ESP_AUDIO_LOGW(TAG, "DUER_CMD_LOGIN connecting,duer_state = %d", duer_state);
                }
            } else if (duer_msg.type == DUER_CMD_CONNECTED) {
                ESP_AUDIO_LOGI(TAG, "Dueros DUER_CMD_CONNECTED, duer_state:%d", duer_state);
                duer_state = DUER_STATE_CONNECTED;
                retry_num = 1;
            } else if (duer_msg.type == DUER_CMD_START) {
                if (duer_state < DUER_STATE_CONNECTED) {
                    ESP_AUDIO_LOGW(TAG, "Dueros has not connected, state:%d", duer_state);
                    continue;
                }
                ESP_AUDIO_LOGI(TAG, "Recv Que DUER_CMD_START");
                duer_voice_start(RECORD_SAMPLE_RATE);
                duer_dcs_on_listen_started();
                duer_state = DUER_STATE_START;
                while (1) {
                    int ret = rec_engine_data_read(voiceData, REC_ONE_BLOCK_SIZE, 110 / portTICK_PERIOD_MS);
                    // ESP_AUDIO_LOGE(TAG, "index = %d", ret);
                    if ((ret == 0) || (ret == -1)) {
                        break;
                    }
                    if (file) {
                        fwrite(voiceData, 1, REC_ONE_BLOCK_SIZE, file);
                        // fsync(fileno(file));
                    }
                    ret  = duer_voice_send(voiceData, REC_ONE_BLOCK_SIZE);
                    if (ret < 0) {
                        ESP_AUDIO_LOGE(TAG, "duer_voice_send failed ret:%d", ret);
                        break;
                    }
                }
            } else if (duer_msg.type == DUER_CMD_STOP && (duer_state == DUER_STATE_START))  {
                ESP_AUDIO_LOGI(TAG, "Dueros DUER_CMD_STOP");
                duer_voice_stop();
                if (file) {
                    fclose(file);
                }
                duer_state = DUER_STATE_STOP;
            } else if (duer_msg.type == DUER_CMD_QUIT)  {
                duer_state = DUER_STATE_IDLE;
                if (set_wifi_flag) {
                    continue;
                }
                if (retry_num < 128) {
                    retry_num *= 2;
                    ESP_AUDIO_LOGI(TAG, "Dueros DUER_CMD_QUIT reconnect, retry_num:%d", retry_num);
                } else {
                    ESP_AUDIO_LOGE(TAG, "Dueros reconnect failed,time num:%d ", retry_num);
                    xTimerStop(retry_login_timer, portMAX_DELAY);
                    break;
                }
                xTimerStop(retry_login_timer, portMAX_DELAY);
                xTimerChangePeriod(retry_login_timer, retry_time * retry_num, portMAX_DELAY);
                xTimerStart(retry_login_timer, portMAX_DELAY);
            }
        }
    }
    free(voiceData);
    vTaskDelete(NULL);
}

static void DeviceEvtNotifiedToRecog(DeviceNotification *note)
{
    DuerAppService *service = (DuerAppService *) note->receiver;
    if (DEVICE_NOTIFY_TYPE_TOUCH == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg *) note->data);
        switch (msg) {
            case DEVICE_NOTIFY_KEY_REC:
#ifndef CONFIG_ESP_LYRATD_FT_DOSS_V1_0_BOARD
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_KEY_REC");
                rec_engine_trigger_start();
#else
                mic_mute_flag = !mic_mute_flag;
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_KEY_REC,%d", mic_mute_flag);
#endif
                break;
            case DEVICE_NOTIFY_KEY_REC_QUIT:
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_KEY_REC_QUIT");
                // asr_engine_trigger_stop();
                break;
            case DEVICE_NOTIFY_KEY_WIFI_SET:
                ESP_AUDIO_LOGW(TAG, "DEVICE_NOTIFY_KEY_WIFI_SET");
                duer_stop();
                set_wifi_flag = 1;
                break;
            default:
                break;
        }
        if (DUER_AUDIO_TYPE_ALERT_SPEECH == duer_audio_wrapper_get_type()) {
            duer_audio_wrapper_alert_stop();
        }

    } else if (DEVICE_NOTIFY_TYPE_WIFI == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg *) note->data);
        switch (msg) {
            case DEVICE_NOTIFY_WIFI_GOT_IP:
                duerOSEvtSend(service->_duerOsQue, DUER_CMD_LOGIN, NULL, 0, 0, 0);
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_WIFI_GOT_IP");
                set_wifi_flag = 0;
                wifi_is_connected = true;
                xEventGroupSetBits(regc_evt_group, CONNECT_BIT);
                break;
            case DEVICE_NOTIFY_WIFI_DISCONNECTED:
                duer_stop();
                wifi_is_connected = false;
                ESP_AUDIO_LOGW(TAG, "DEVICE_NOTIFY_WIFI_DISCONNECTED");
                break;
            case DEVICE_NOTIFY_WIFI_SETTING_TIMEOUT:
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_WIFI_SETTING_TIMEOUT");
                duer_dcs_tone_handler(tone_uri[TONE_TYPE_PLEASE_RETRY_WIFI]);
                break;
            default:
                break;
        }
    } else if (DEVICE_NOTIFY_TYPE_DSP == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg *) note->data);
        switch (msg) {
            case DEVICE_NOTIFY_DSP_WAKEUP:
#if ENABLE_WAKEUP_BY_DSP
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_DSP_WAKEUP EVENT");
                rec_engine_trigger_start();
#endif
                break;
            default:
                break;
        }
    }
}

static void duerAppActive(DuerAppService *service)
{
#ifndef WAKEUP_TEST
    if (xTaskCreate(dueros_task, "dueros_task", RECOG_SERVICE_TASK_SIZE, service,
                    RECOG_SERVICE_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_AUDIO_LOGE(TAG, "Error create dueros_task");
        return;
    }
#else
    rec_config_t eng = DEFAULT_REC_ENGINE_CONFIG();
    eng.evt_cb = rec_engine_cb;
    eng.open = recorder_open;
    eng.close = recorder_close;
    eng.fetch = recorder_read;
    rec_engine_create(&eng);
#endif
    ESP_AUDIO_LOGI(TAG, "duerAppActive");
}

static void duerAppDeactive(DuerAppService *service)
{
    vQueueDelete(service->_duerOsQue);
    service->_duerOsQue = NULL;
    ESP_AUDIO_LOGI(TAG, "duerAppDeactive");
}

DuerAppService *DuerAppServiceCreate()
{
    DuerAppService *recog = (DuerAppService *) EspAudioAlloc(1, sizeof(DuerAppService));
    ESP_ERROR_CHECK(!recog);
    recog->Based.deviceEvtNotified = DeviceEvtNotifiedToRecog;
    recog->Based.serviceActive = duerAppActive;
    recog->Based.serviceDeactive = duerAppDeactive;
    regc_evt_group = xEventGroupCreate();
    configASSERT(regc_evt_group);
    recog->_duerOsQue = xQueueCreate(5, sizeof(duer_task_msg_t));
    configASSERT(recog->_duerOsQue);
    ESP_AUDIO_LOGI(TAG, "DuerAppService has create");
    localService = recog;
    return recog;
}
