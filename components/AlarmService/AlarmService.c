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
#include "alarm.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_audio_log.h"
#include "driver/adc.h"

#include "AlarmService.h"

#include "alarmparser.h"
#include "rom/queue.h"

#include "esp_deep_sleep.h"
#include "driver/rtc_io.h"
#include "DeviceCommon.h"
#include "lwip/err.h"
#include "apps/sntp/sntp.h"
#include "EspAudioAlloc.h"
#include "userconfig.h"


#define ALARM_TAG                            "ALARM_SERVICE"

#define SNTP_SERVICE_TASK_SIZE         2*1024
#define SNTP_SERVICE_TASK_PRIORITY     6
static uint32_t g_esp_alarm_last_handle = 0;
TimerHandle_t poll_timer_handle = NULL;

static LIST_HEAD(esp_alarm_info_head, esp_alarm_info_t) g_esp_alarm_info_head =
    LIST_HEAD_INITIALIZER(g_esp_alarm_info_head);

void PollTimerCallback(TimerHandle_t pxTimer)
{
    esp_alarm_info *it;
    int poll_handle;
    int trigger_type;
    for (it = LIST_FIRST(&g_esp_alarm_info_head); it != NULL; it = LIST_NEXT(it, entries)) {
        trigger_type = alarmTrigger(it,&poll_handle);
        if(ABS_SUCC == trigger_type){
            if(it->trigger_func != NULL && it->status == A_running){
                it->trigger_func(it->trigger_arg, it->handle);
                it->status = A_idle;
            }
        } else if(PER_TRIG == trigger_type){
            if(it->trigger_func != NULL && it->status == A_running){
                it->trigger_func(it->trigger_arg,it->handle);
            }
        }
    }

}
Alarm_Code esp_alarm_edit(esp_alarm_info* alarm_info, esp_alarm_handle handle)
{
    esp_alarm_info *it;

    for (it = LIST_FIRST(&g_esp_alarm_info_head); it != NULL; it = LIST_NEXT(it, entries)) {
        if(handle == it->handle){
            memcpy(it,alarm_info,sizeof(esp_alarm_info));
        }
    }
    if(it == NULL){
        return E_ALARM_NOTFOUND;
    }
    if(poll_timer_handle != NULL){
        xTimerReset(poll_timer_handle, 0);
    }
    return E_ALARM_OK;

}

Alarm_Code esp_alarm_add(char* alarm_buf,esp_alarm_handle* handle)
{
    char buff[MAX_FORMAT_LEN];
    Alarm_t alarm;
    int ret;
    time_t now;
    struct tm timeinfo;

    if(alarm_buf == NULL || handle == NULL) {
        ESP_AUDIO_LOGE(ALARM_TAG, "invalid input parameter");
        return E_ALARM_ARG;
    }

    if(strlen(alarm_buf) > MAX_FORMAT_LEN){
        ESP_AUDIO_LOGE(ALARM_TAG, "invalid input parameter alarm_buf len %d",strlen(alarm_buf));
        return E_ALARM_ARG;
    }

    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_AUDIO_LOGE(ALARM_TAG, "alarm not ready, please start sntp first");
        return E_ALARM_NOREADY;
    }
    esp_alarm_info* new_entry = (esp_alarm_info *) calloc(sizeof(esp_alarm_info), 1);
    if (new_entry == NULL) {
        ESP_AUDIO_LOGE(ALARM_TAG, "alloc node failed");
        return E_ALARM_NOMEM;
    }

    bzero(buff,MAX_FORMAT_LEN);
    memcpy(buff,alarm_buf,strlen(alarm_buf));
    ret = parseAlarm(&(new_entry->alarm_info), buff);
    if (E_ALARM_OK != ret){
        ESP_AUDIO_LOGE(ALARM_TAG, "parseAlarm failed,error code %d",ret);
        return ret;
    }

    new_entry->status = A_running;
    new_entry->handle = g_esp_alarm_last_handle++;
    LIST_INSERT_HEAD(&g_esp_alarm_info_head, new_entry, entries);
    *handle = new_entry->handle;

    printAlarmStructure(&(new_entry->alarm_info));
    if(poll_timer_handle == NULL){
        poll_timer_handle = xTimerCreate("PollTimer", 2000 / portTICK_PERIOD_MS , pdTRUE, NULL, PollTimerCallback);
        assert(poll_timer_handle != NULL);
        xTimerStart(poll_timer_handle, 0);
    } else{
        xTimerReset(poll_timer_handle, 0);
    }
    return E_ALARM_OK;
}

Alarm_Code esp_alarm_delete(esp_alarm_handle handle)
{
    esp_alarm_info *it;
    for (it = LIST_FIRST(&g_esp_alarm_info_head); it != NULL; it = LIST_NEXT(it, entries)) {
        if(handle == it->handle){
            LIST_REMOVE(it, entries);
            free(it);
            return E_ALARM_OK;
        }
    }
    if(it == NULL){
        return E_ALARM_NOTFOUND;
    }
    return E_ALARM_OK;
}

Alarm_Code esp_get_alarm_info(esp_alarm_handle handle,esp_alarm_info *alarm_info)
{
    esp_alarm_info *it;
    if(alarm_info == NULL){
        return E_ALARM_ARG;
    }
    for (it = LIST_FIRST(&g_esp_alarm_info_head); it != NULL; it = LIST_NEXT(it, entries)) {
        if(handle == it->handle){
            memcpy(alarm_info, it, sizeof(esp_alarm_info));
            return E_ALARM_OK;
        }
    }
    if(it == NULL){
        return E_ALARM_NOTFOUND;
    }
    return E_ALARM_OK;
}

Alarm_Code esp_alarm_register_trigger_func(void * arg,esp_alarm_handle handle,alarm_trigger_func func)
{
    esp_alarm_info *it;
    for (it = LIST_FIRST(&g_esp_alarm_info_head); it != NULL; it = LIST_NEXT(it, entries)) {
        if(handle == it->handle){
            it->trigger_arg = arg;
            it->trigger_func = func;
            return E_ALARM_OK;
        }
    }
    if(it == NULL){
        return E_ALARM_NOTFOUND;
    }
    return E_ALARM_OK;
}

static void initialize_sntp(void)
{
    ESP_AUDIO_LOGI(ALARM_TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "cn.ntp.org.cn");
    sntp_init();
}

static int obtain_time(void)
{
    initialize_sntp();

    // wait for time to be set
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;
    const int retry_count = 10;
    while(timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_AUDIO_LOGI(ALARM_TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
    if(retry == retry_count){
        return -1;
    } else {
        return 0;
    }
}

void sntp_init_task(void *param)
{
    time_t now;
    struct tm timeinfo;
    time(&now);
    localtime_r(&now, &timeinfo);
    // Is time set? If not, tm_year will be (1970 - 1900).
    if (timeinfo.tm_year < (2016 - 1900)) {
        ESP_AUDIO_LOGI(ALARM_TAG, "Time is not set yet. init sntp and getting time over NTP.");
        if(obtain_time()){
            ESP_AUDIO_LOGE(ALARM_TAG, "obtain time failed");
            vTaskDelete(NULL);
        }
        // update 'now' variable with current time
        time(&now);
    }
    char strftime_buf[64];
    setenv("TZ", "CST-8", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_AUDIO_LOGI(ALARM_TAG, "The current date/time in Shanghai is: %s", strftime_buf);

    vTaskDelete(NULL);
}

static void DeviceEvtNotifiedToAlarm(DeviceNotification* note)
{
    AlarmService* service = (AlarmService*) note->receiver;

    if (DEVICE_NOTIFY_TYPE_WIFI == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg*) note->data);

        switch (msg) {
            case DEVICE_NOTIFY_WIFI_GOT_IP:
                if (xTaskCreate(sntp_init_task, "sntp_init_task", SNTP_SERVICE_TASK_SIZE, NULL,
                                SNTP_SERVICE_TASK_PRIORITY, NULL) != pdPASS) {
                    ESP_AUDIO_LOGE(ALARM_TAG, "Error create sntp init task");
                }
                break;

            case DEVICE_NOTIFY_WIFI_DISCONNECTED:
                break;

            default:
                break;
        }
    }
}

static void AlarmServiceActive(AlarmService *service)
{


}

static void AlarmServiceDeactive(AlarmService *service)
{
//    vQueueDelete(queRecData);
//    queRecData = NULL;
//    ESP_AUDIO_LOGI(RECOG_SERVICE_TAG, "recognitionDeactive\r\n");
}

AlarmService *AlarmServiceCreate()
{
    AlarmService *alarm = (AlarmService *) EspAudioAlloc(1, sizeof(AlarmService));
    ESP_ERROR_CHECK(!alarm);
    alarm->Based.deviceEvtNotified = DeviceEvtNotifiedToAlarm;
    alarm->Based.serviceActive = AlarmServiceActive;
    alarm->Based.serviceDeactive = AlarmServiceDeactive;
    ESP_AUDIO_LOGI(ALARM_TAG, "AlarmService has create\r\n");
    return alarm;
}
