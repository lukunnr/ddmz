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

#include "duerapp_alert.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "lightduer_mutex.h"
#include "lightduer_memory.h"
#include "lightduer_dcs_alert.h"
#include "lightduer_net_ntp.h"
#include "lightduer_timers.h"
#include "lightduer_coap_defs.h"
#include "duer_audio_wrapper.h"
#include "toneuri.h"
#include "esp_log.h"

static char *TAG = "DUER_ALERT";

typedef struct {
    duer_dcs_alert_info_type alert_info;
    duer_timer_handler handle;
    bool is_active;
    uint8_t tone_count;
    uint8_t current_tone_index;
    uint8_t loop_count;
    char *tone_tab[1];
} duerapp_alert_data;

typedef struct _duer_alert_list_node {
    struct _duer_alert_list_node *next;
    duerapp_alert_data *data;
} duer_alert_list_node_t;

static duer_alert_list_node_t s_alert_list;
static duer_mutex_t s_alert_mutex = NULL;
static duer_mutex_t s_queue_mutex = NULL;

static duer_alert_list_node_t s_alert_queue;
static const int ALERT_TREAD_STACK_SIZE = 1024 * 4;
static const char *const s_ntp_server_tab[] = {"s1a.time.edu.cn",
                                               "s1b.time.edu.cn",
                                               "s2c.time.edu.cn",
                                               "time-a.nist.gov"
                                              };
static const uint8_t DEFAULT_ALERT_TONE_LOOP_COUNT = 10;
static duer_alert_list_node_t *s_ringing_alert_node = NULL;

static duer_alert_list_node_t *duer_find_active_alert(duer_alert_list_node_t *alert_node)
{
    duer_alert_list_node_t *node = alert_node;

    while (node) {
        node = node->next;

        if (node && node->data && node->data->is_active) {
            return node;
        }
    }
    node = s_alert_list.next;
    while (node) {
        if (node->data && node->data->is_active) {
            return node;
        }

        node = node->next;
    }
    return NULL;
}

static int duer_audio_player_on_start(duer_audio_type_t type)
{
    return 0;
}

static int duer_audio_player_on_stop(duer_audio_type_t type)
{
    duer_alert_list_node_t *alert_node = NULL;
    duer_mutex_lock(s_alert_mutex);
    do {
        alert_node = duer_find_active_alert(NULL);
        if (alert_node && alert_node->data) {
            alert_node->data->is_active = false;
            duer_dcs_report_alert_event(alert_node->data->alert_info.token, ALERT_STOP);
        }
    } while (alert_node);
    s_ringing_alert_node = NULL;
    duer_mutex_unlock(s_alert_mutex);

    return 0;
}

static int duer_audio_player_on_finish(duer_audio_type_t type)
{
    duerapp_alert_data *alert = NULL;
    duer_mutex_lock(s_alert_mutex);
    alert = s_ringing_alert_node ? s_ringing_alert_node->data : NULL;

    if (alert) {
        if (alert->current_tone_index + 1 < alert->tone_count) {
            alert->current_tone_index++;
            duer_audio_wrapper_alert_play(alert->tone_tab[alert->current_tone_index], DUER_AUDIO_TYPE_ALERT_SPEECH);
            duer_mutex_unlock(s_alert_mutex);
            return 0;
        } else {
            alert->loop_count--;
            alert->current_tone_index = 0;
            if (alert->loop_count == 0) {
                alert->is_active = false;
                duer_dcs_report_alert_event(alert->alert_info.token, ALERT_STOP);
            }
        }
    }
    s_ringing_alert_node = duer_find_active_alert(s_ringing_alert_node);
    alert = s_ringing_alert_node ? s_ringing_alert_node->data : NULL;
    if (alert) {
        if (alert->tone_count) {
            duer_audio_wrapper_alert_play(alert->tone_tab[alert->current_tone_index], DUER_AUDIO_TYPE_ALERT_SPEECH);
        } else {
            duer_audio_wrapper_alert_play(tone_uri[TONE_TYPE_ALARM], DUER_AUDIO_TYPE_ALERT_RINGING);
        }
    }
    duer_mutex_unlock(s_alert_mutex);
    return 0;
}

static int duer_audio_player_on_error(duer_audio_type_t type)
{
    duerapp_alert_data *alert = NULL;

    duer_mutex_lock(s_alert_mutex);

    alert = s_ringing_alert_node ? s_ringing_alert_node->data : NULL;
    if (type == DUER_AUDIO_TYPE_ALERT_RINGING) {
        if (alert) {
            alert->is_active = false;
            duer_dcs_report_alert_event(alert->alert_info.token, ALERT_STOP);
        }
    } else {
        // duer_audio_player_play(ALERT_FILE, DUER_AUDIO_TYPE_ALERT_RINGING, 0);
        duer_audio_wrapper_alert_play(tone_uri[TONE_TYPE_ALARM], DUER_AUDIO_TYPE_ALERT_RINGING);
        duer_mutex_unlock(s_alert_mutex);
        return 0;
    }

    s_ringing_alert_node = duer_find_active_alert(s_ringing_alert_node);
    alert = s_ringing_alert_node ? s_ringing_alert_node->data : NULL;
    if (alert) {
        if (alert->tone_count) {
            duer_audio_wrapper_alert_play(alert->tone_tab[alert->current_tone_index], DUER_AUDIO_TYPE_ALERT_SPEECH);
        } else {
            // duer_audio_player_play(ALERT_FILE, DUER_AUDIO_TYPE_ALERT_RINGING, 0);
            duer_audio_wrapper_alert_play(tone_uri[TONE_TYPE_ALARM], DUER_AUDIO_TYPE_ALERT_RINGING);
        }
    }

    duer_mutex_unlock(s_alert_mutex);

    return 0;
}

void alert_paly_status_callback(duer_audio_type_t type, AudioStatus status)
{
    if (type != DUER_AUDIO_TYPE_ALERT_SPEECH && type != DUER_AUDIO_TYPE_ALERT_RINGING) {
        return;
    }
    if (status == AUDIO_STATUS_PLAYING) {

    } else if (status == AUDIO_STATUS_STOP) {
        duer_audio_player_on_stop(type);
    } else if (status == AUDIO_STATUS_FINISHED) {
        duer_audio_player_on_finish(type);
    } else if (status == AUDIO_STATUS_ERROR) {
        duer_audio_player_on_error(type);
    }
}

static duer_errcode_t duer_alert_list_push(duer_alert_list_node_t *head, duerapp_alert_data *data)
{
    duer_alert_list_node_t *new_node = NULL;
    duer_alert_list_node_t *tail = head;

    new_node = (duer_alert_list_node_t *)DUER_MALLOC(
                   sizeof(duer_alert_list_node_t));

    if (!new_node) {
        ESP_LOGE(TAG, "Memory too low");
        return DUER_ERR_MEMORY_OVERLOW;
    }
    new_node->next = NULL;
    new_node->data = data;
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = new_node;
    return DUER_OK;
}

static duer_errcode_t duer_alert_list_remove(duer_alert_list_node_t *head,
        duer_alert_list_node_t *node)
{
    duer_alert_list_node_t *pre = head;
    duer_alert_list_node_t *cur = NULL;

    while (pre->next) {
        cur = pre->next;
        if (cur == node) {
            pre->next = cur->next;
            DUER_FREE(cur);
            return DUER_OK;
        }
        pre = pre->next;
    }
    return DUER_ERR_FAILED;
}

static char *duerapp_alert_strdup(const char *str)
{
    int len = 0;
    char *dest = NULL;
    if (!str) {
        return NULL;
    }
    len = strlen(str);
    dest = (char *)DUER_MALLOC(len + 1);
    if (!dest) {
        return NULL;
    }
    snprintf(dest, len + 1, "%s", str);
    return dest;
}

static void duer_free_alert_data(duerapp_alert_data *alert)
{
    int i = 0;

    if (alert) {
        if (alert->alert_info.type) {
            DUER_FREE(alert->alert_info.type);
            alert->alert_info.type = NULL;
        }
        if (alert->alert_info.time) {
            DUER_FREE(alert->alert_info.time);
            alert->alert_info.time = NULL;
        }
        if (alert->alert_info.token) {
            DUER_FREE(alert->alert_info.token);
            alert->alert_info.token = NULL;
        }
        if (alert->handle) {
            duer_timer_release(alert->handle);
            alert->handle = NULL;
        }
        while (i < alert->tone_count) {
            if (alert->tone_tab[i]) {
                DUER_FREE(alert->tone_tab[i]);
                alert->tone_tab[i] = NULL;
            }
            i++;
        }
        DUER_FREE(alert);
    }
}

static char *duer_find_target_url(baidu_json *assets, char *assetId)
{
    int asset_count = 0;
    int i = 0;
    baidu_json *asset_item = NULL;
    baidu_json *asset_id_item = NULL;
    baidu_json *url_item = NULL;

    asset_count = baidu_json_GetArraySize(assets);
    while (i < asset_count) {
        asset_item = baidu_json_GetArrayItem(assets, i);
        if (!asset_item) {
            break;
        }
        asset_id_item = baidu_json_GetObjectItem(asset_item, "assetId");
        if (!asset_id_item || !asset_id_item->valuestring) {
            break;
        }
        if (strcmp(asset_id_item->valuestring, assetId) == 0) {
            url_item = baidu_json_GetObjectItem(asset_item, "url");
            break;
        }
        i++;
    }
    if (url_item) {
        return url_item->valuestring;
    } else {
        return NULL;
    }
}

static void duer_set_alert_tone(duerapp_alert_data *alert,
                                baidu_json *assets,
                                baidu_json *play_order)
{
    int i = 0;
    baidu_json *asset_id = NULL;
    char *url = NULL;

    while (i < alert->tone_count) {
        asset_id = baidu_json_GetArrayItem(play_order, i);
        if (!asset_id || !asset_id->valuestring) {
            ESP_LOGE(TAG, "Failed to get assetId");
            break;
        }
        url = duer_find_target_url(assets, asset_id->valuestring);
        if (!url) {
            ESP_LOGE(TAG, "Failed to get tone url");
            break;
        }
        alert->tone_tab[i] = duerapp_alert_strdup(url);
        if (!alert->tone_tab[i]) {
            ESP_LOGE(TAG, "Failed to set tone url: no memory");
            break;
        }
        i++;
    }
    alert->tone_count = i;
}

static duerapp_alert_data *duer_create_alert_data(const duer_dcs_alert_info_type *alert_info,
        const baidu_json *payload)
{
    duerapp_alert_data *alert = NULL;
    baidu_json *assets = NULL;
    baidu_json *play_order = NULL;
    baidu_json *loop_count = NULL;
    int tone_count = 0;
    size_t alert_data_size = sizeof(duerapp_alert_data);
    int ret = 0;
    do {
        play_order = baidu_json_GetObjectItem(payload, "assetPlayOrder");
        if (!play_order) {
            ESP_LOGE(TAG, "Failed to get assetPlayOrder array");
            ret = -1;
            break;
        }
        tone_count = baidu_json_GetArraySize(play_order);
        if (tone_count > 0) {
            alert_data_size += (tone_count) * sizeof(char *);
        }
        alert = (duerapp_alert_data *)DUER_MALLOC(alert_data_size);
        if (!alert) {
            ESP_LOGE(TAG, "Memory too low");
            ret = -1;
            break;
        }
        memset(alert, 0, alert_data_size);
        alert->tone_count = tone_count;

        alert->alert_info.type = duerapp_alert_strdup(alert_info->type);
        if (!alert->alert_info.type) {
            ESP_LOGE(TAG, "Memory too low");
            ret = -1;
            break;
        }
        alert->alert_info.time = duerapp_alert_strdup(alert_info->time);
        if (!alert->alert_info.time) {
            ESP_LOGE(TAG, "Memory too low");
            ret = -1;
            break;
        }
        alert->alert_info.token = duerapp_alert_strdup(alert_info->token);
        if (!alert->alert_info.token) {
            ESP_LOGE(TAG, "Memory too low");
            ret = -1;
            break;
        }
        loop_count = baidu_json_GetObjectItem(payload, "loopCount");
        if (loop_count) {
            alert->loop_count = loop_count->valueint;
        }
        if (alert->loop_count == 0) {
            alert->loop_count = DEFAULT_ALERT_TONE_LOOP_COUNT;
        }
        assets = baidu_json_GetObjectItem(payload, "assets");
        if (!assets) {
            ESP_LOGE(TAG, "Failed to get assets array");
            ret = -1;
            break;
        }

        duer_set_alert_tone(alert, assets, play_order);
        alert->handle = NULL;
    } while (0);
    if (ret < 0) {
        duer_free_alert_data(alert);
        alert = NULL;
    }
    return alert;
}

static void duer_alert_callback(void *param)
{
    duerapp_alert_data *alert = (duerapp_alert_data *)param;

    duer_alert_list_node_t *node = s_alert_list.next;
    duer_mutex_lock(s_alert_mutex);

    ESP_LOGD(TAG, "alert started: token: %s", alert->alert_info.token);
    alert->is_active = true;
    duer_dcs_report_alert_event(alert->alert_info.token, ALERT_START);
    if (s_ringing_alert_node) {
        duer_mutex_unlock(s_alert_mutex);
        return;
    }
    while (node) {
        if (node->data == alert) {
            s_ringing_alert_node = node;
            break;
        }
        node = node->next;
    }
    if (s_ringing_alert_node) {
        if (alert->tone_count) {
            duer_audio_wrapper_alert_play(alert->tone_tab[alert->current_tone_index], DUER_AUDIO_TYPE_ALERT_SPEECH);
        } else {
            // duer_audio_player_play("/sdcard/error.mp3", DUER_AUDIO_TYPE_ALERT_RINGING, 0);
            duer_audio_wrapper_alert_play(tone_uri[TONE_TYPE_ALARM], DUER_AUDIO_TYPE_ALERT_RINGING);
        }
    }
    duer_mutex_unlock(s_alert_mutex);
}

static time_t duer_dcs_get_time_stamp(const char *time)
{
    time_t time_stamp = 0;
    struct tm time_tm;
    int rs = 0;
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int min = 0;
    int sec = 0;

    rs = sscanf(time, "%d-%d-%dT%d:%d:%d+", &year, &month, &day, &hour, &min, &sec);
    if (rs != 6) {
        return (time_t) - 1;
    }

    time_tm.tm_year  = year - 1900;
    time_tm.tm_mon   = month - 1;
    time_tm.tm_mday  = day;
    time_tm.tm_hour  = hour;
    time_tm.tm_min   = min;
    time_tm.tm_sec   = sec;
    time_tm.tm_isdst = 0;

    time_stamp = mktime(&time_tm);
    return time_stamp;
}

/**
 * We use ntp to get current time, it might spend too long time and block the thread,
 * hence we use a new thread to create alert.
 */
static void duer_alert_thread(void *arg)
{
    DuerTime cur_time;
    size_t delay = 0;
    int rs = DUER_OK;
    duerapp_alert_data *alert = NULL;
    time_t cur_timestamp = 0;
    uint32_t time = 0;
    int i = 0;
    ESP_LOGI(TAG, "alert thread start....");

    while (1) {
        if (rs != DUER_OK) {
            duer_dcs_report_alert_event(alert->alert_info.token, SET_ALERT_FAIL);
            duer_free_alert_data(alert);
            rs = DUER_OK;
        }
        alert = NULL;

        duer_mutex_lock(s_queue_mutex);
        if (s_alert_queue.next != NULL) {
            alert = s_alert_queue.next->data;
            duer_alert_list_remove(&s_alert_queue, s_alert_queue.next);
        }
        duer_mutex_unlock(s_queue_mutex);
        if (alert == NULL) {
            ESP_LOGW(TAG, "Exit alert thread by a empty queue");
            break;
        }
        time = duer_dcs_get_time_stamp(alert->alert_info.time);
        if ((int)time < 0) {
            rs = DUER_ERR_FAILED;
            continue;
        }
        for (i = 0; i < sizeof(s_ntp_server_tab) / sizeof(s_ntp_server_tab[0]); i++) {
            rs = duer_ntp_client((char *)s_ntp_server_tab[i], 3000, &cur_time, NULL);
            if (rs >= 0) {
                break;
            }
        }
        if (rs < 0) {
            ESP_LOGE(TAG, "Failed to get NTP time");
            rs = DUER_ERR_FAILED;
            continue;
        }
        cur_timestamp = cur_time.sec * 1000 + cur_time.usec / 1000;
        if (time * 1000 <= cur_timestamp) {
            ESP_LOGE(TAG, "The alert is expired");
            rs = DUER_ERR_FAILED;
            continue;
        }
        delay = time * 1000 - cur_timestamp;
        alert->handle = duer_timer_acquire(duer_alert_callback,
                                           alert,
                                           DUER_TIMER_ONCE);
        if (!alert->handle) {
            ESP_LOGE(TAG, "Failed to create timer");
            rs = DUER_ERR_FAILED;
            continue;
        }
        ESP_LOGW(TAG, "Set delay time is %d\n",  delay);
        rs = duer_timer_start(alert->handle, delay);
        if (rs != DUER_OK) {
            ESP_LOGE(TAG, "Failed to start timer");
            rs = DUER_ERR_FAILED;
            continue;
        }

        /*
         * The alerts is storaged in the ram, hence the alerts will be lost after close the device.
         * You could stoage them into flash or sd card, and restore them after restart the device
         * according to your request.
         */
        duer_mutex_lock(s_alert_mutex);
        rs = duer_alert_list_push(&s_alert_list, alert);
        duer_mutex_unlock(s_alert_mutex);
        if (rs == DUER_OK) {
            duer_dcs_report_alert_event(alert->alert_info.token, SET_ALERT_SUCCESS);
        }
    }
    vTaskDelete(NULL);
}

duer_status_t duer_dcs_tone_alert_set_handler(const baidu_json *directive)
{
    baidu_json *payload = NULL;
    baidu_json *scheduled_time = NULL;
    baidu_json *token = NULL;
    baidu_json *type = NULL;
    duer_dcs_alert_info_type alert_info;
    duerapp_alert_data *alert = NULL;
    duer_status_t ret = DUER_OK;
    bool empty = false;

    if (!directive) {
        ESP_LOGE(TAG, "Invalid param: directive is null");
        return DUER_MSG_RSP_BAD_REQUEST;
    }
    payload = baidu_json_GetObjectItem(directive, "payload");
    if (!payload) {
        ESP_LOGE(TAG, "Failed to get payload");
        return DUER_MSG_RSP_BAD_REQUEST;
    }
    token = baidu_json_GetObjectItem(payload, "token");
    if (!token) {
        ESP_LOGE(TAG, "Failed to get token");
        return DUER_MSG_RSP_BAD_REQUEST;
    }
    scheduled_time = baidu_json_GetObjectItem(payload, "scheduledTime");
    if (!scheduled_time) {
        ESP_LOGE(TAG, "Failed to get scheduledTime");
        return DUER_MSG_RSP_BAD_REQUEST;
    }
    type = baidu_json_GetObjectItem(payload, "type");
    if (!type) {
        ESP_LOGE(TAG, "Failed to get alert type");
        return DUER_MSG_RSP_BAD_REQUEST;
    }
    alert_info.time = scheduled_time->valuestring;
    alert_info.token = token->valuestring;
    alert_info.type = type->valuestring;
    ESP_LOGI(TAG, "set alert: scheduled_time: %s, token: %s",
             alert_info.time,
             alert_info.token);

    alert = duer_create_alert_data(&alert_info, payload);
    if (!alert) {
        duer_dcs_report_alert_event(alert_info.token, SET_ALERT_FAIL);
        ret = DUER_ERR_FAILED;
    } else {
        // create alert in duer_alert_thread and return immediately
        duer_mutex_lock(s_queue_mutex);
        if (s_alert_queue.next == NULL) {
            empty = true;
        }
        ret = duer_alert_list_push(&s_alert_queue, alert);
        if (ret < DUER_OK) {
            duer_dcs_report_alert_event(alert_info.token, SET_ALERT_FAIL);
        } else if (empty) {
            // ret = duer_audio_thread_start(s_alert_thread, duer_alert_thread, NULL);
            if (xTaskCreate(duer_alert_thread, "alert_task", 6 * 1024, NULL,
                            3, NULL) != pdPASS) {
                ESP_LOGE(TAG, "Error create dueros_task");
                return;
            }
            ESP_LOGI(TAG, "sduer_alert_list_push: %d", ret);
        }
        duer_mutex_unlock(s_queue_mutex);
    }
    return ret;
}

static duer_alert_list_node_t *duer_find_target_alert_node(const char *token)
{
    duer_alert_list_node_t *node = s_alert_list.next;
    while (node) {
        if (node->data) {
            if (strcmp(token, node->data->alert_info.token) == 0) {
                return node;
            }
        }
        node = node->next;
    }
    return NULL;
}

void duer_dcs_get_all_alert(baidu_json *alert_list)
{
    duer_alert_list_node_t *node = s_alert_list.next;
    while (node && node->data) {
        duer_insert_alert_list(alert_list, &node->data->alert_info, node->data->is_active);
        node = node->next;
    }
}

void duer_dcs_alert_delete_handler(const char *token)
{
    duer_alert_list_node_t *target_alert_node = NULL;

    ESP_LOGI(TAG, "delete alert: token %s", token);
    duer_mutex_lock(s_alert_mutex);
    target_alert_node = duer_find_target_alert_node(token);
    if (!target_alert_node) {
        duer_dcs_report_alert_event(token, DELETE_ALERT_FAIL);
        duer_mutex_unlock(s_alert_mutex);
        ESP_LOGE(TAG, "Cannot find the target alert");
        return;
    }
    if (target_alert_node == s_ringing_alert_node) {
        s_ringing_alert_node = NULL;
        duer_audio_wrapper_alert_stop();
    }

    if (target_alert_node->data->is_active) {
        duer_dcs_report_alert_event(token, ALERT_STOP);
    }

    duer_free_alert_data(target_alert_node->data);
    duer_alert_list_remove(&s_alert_list, target_alert_node);

    duer_mutex_unlock(s_alert_mutex);
    duer_dcs_report_alert_event(token, DELETE_ALERT_SUCCESS);
}

int duer_alert_init()
{
    static bool is_first_time = true;
    int ret = DUER_OK;
    /**
     * The init function might be called sevaral times when ca re-connect,
     * but some operations only need to be done once.
     */
    if (!is_first_time) {
        return ret;
    }
    is_first_time = false;
    do {
        s_alert_mutex = duer_mutex_create();
        if (s_alert_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create s_alert_mutex.");
            ret = DUER_ERR_FAILED;
            break;
        }
        s_queue_mutex = duer_mutex_create();
        if (s_queue_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create s_queue_mutex.");
            ret = DUER_ERR_FAILED;
            break;
        }
        duer_audio_wrapper_callback_register(alert_paly_status_callback);
        duer_dcs_alert_init();
    } while (0);

    if (ret != DUER_OK) {
        if (s_alert_mutex != NULL) {
            duer_mutex_destroy(s_alert_mutex);
            s_alert_mutex = NULL;
        }
        if (s_queue_mutex != NULL) {
            duer_mutex_destroy(s_queue_mutex);
            s_queue_mutex = NULL;
        }
    }
    return ret;
}
