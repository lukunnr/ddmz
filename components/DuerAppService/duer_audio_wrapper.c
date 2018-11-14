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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/timers.h"
#include <freertos/semphr.h>

#include "duer_audio_wrapper.h"
#include "lightduer_voice.h"
#include "lightduer_ds_log_e2e.h"
#include "duerapp_alert.h"
#include "recorder_engine.h"
#include "EspAudio.h"
#include "esp_log.h"

static SemaphoreHandle_t s_mutex;
static duer_audio_type_t speech_type = 0;
static TaskHandle_t task_audio_duer;
static duer_audio_wrapper_cb g_wrapper_cb;
static bool g_duer_audio_wrapper_run;

const char *TAG = "DUER_WRAPPER";

static void duer_audio_wrapper_task (void *para)
{
    QueueHandle_t que = (QueueHandle_t) para;
    PlayerStatus player = {0};
    while (g_duer_audio_wrapper_run) {
        xQueueReceive(que, &player, portMAX_DELAY);
        ESP_LOGI(TAG, "*** status:%d, mode:%d,speech_type:%d ***", player.status, player.mode, speech_type);
        if ((player.status == AUDIO_STATUS_ERROR)
            || (player.status == AUDIO_STATUS_STOP)
            // || (player.status == AUDIO_STATUS_PAUSED)
            || (player.status == AUDIO_STATUS_FINISHED)) {
            if (speech_type == DUER_AUDIO_TYPE_SPEECH) {
                speech_type = DUER_AUDIO_TYPE_UNKOWN;
                duer_dcs_speech_on_finished();
                ESP_LOGI(TAG, "duer_dcs_speech_on_finished, %d", speech_type);
            } else if (speech_type == DUER_AUDIO_TYPE_MUSIC) {
                duer_dcs_audio_on_finished();
                ESP_LOGI(TAG, "duer_dcs_audio_on_finished, %d", speech_type);
            } else if (speech_type == DUER_AUDIO_TYPE_TONE) {
                ESP_LOGI(TAG, "DUER_AUDIO_TYPE_TONE, %d", speech_type);
                rec_engine_detect_suspend(REC_VOICE_SUSPEND_OFF);
            }
        } else if (player.status == AUDIO_STATUS_PAUSED) {
            ESP_LOGI(TAG, "WRAPPER task, AUDIO_STATUS_PAUSED, %d", speech_type);
            // duer_dcs_audio_on_stopped();
        }
        if (g_wrapper_cb) {
            g_wrapper_cb(speech_type, player.status);
        }
    }
    vQueueDelete(que);
    task_audio_duer = NULL;
    vTaskDelete(NULL);
}

esp_err_t duer_audio_wrapper_init()
{
    if (NULL == task_audio_duer) {
        QueueHandle_t que = xQueueCreate(2, sizeof(PlayerStatus));
        if (que == NULL) {
            return ESP_FAIL;
        }
        g_duer_audio_wrapper_run = true;
        EspAudioStatusListenerAdd(que);
        if (xTaskCreate(duer_audio_wrapper_task, "wrapper_tsk", 3 * 1024, que, 4, &task_audio_duer) != pdPASS) {
            vQueueDelete(que);
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t duer_audio_wrapper_callback_register(duer_audio_wrapper_cb cb)
{
    g_wrapper_cb = cb;
    return ESP_OK;
}

esp_err_t duer_audio_wrapper_deinit()
{
    g_duer_audio_wrapper_run = false;

    return ESP_OK;
}

void duer_audio_wrapper_alert_play(const char *url, duer_audio_type_t type)
{
    EspAudioTonePlay(url, TERMINATION_TYPE_NOW, 0);
    speech_type = type;
}

void duer_audio_wrapper_alert_stop()
{
    if ((speech_type == DUER_AUDIO_TYPE_ALERT_SPEECH)
        || (speech_type == DUER_AUDIO_TYPE_ALERT_RINGING)) {
        EspAudioToneStop();
        speech_type = DUER_AUDIO_TYPE_UNKOWN;
        ESP_LOGI(TAG, "duer_audio_wrapper_alert_stop");
    }
}

void duer_dcs_init(void)
{
    static bool is_first_time = true;
    ESP_LOGI(TAG, "duer_dcs_init\n");

    duer_dcs_framework_init();
    duer_dcs_voice_input_init();
    duer_dcs_voice_output_init();
    duer_dcs_speaker_control_init();
    duer_dcs_audio_player_init();
    duer_alert_init();

    if (s_mutex == NULL) {
        s_mutex = xSemaphoreCreateMutex();
        if (s_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create mutex");
        }
    }

    if (is_first_time) {
        is_first_time = false;
        //freertos register_listener
        duer_dcs_sync_state();
    }
}
void duer_dcs_listen_handler(void)
{
    ESP_LOGI(TAG, "enable_listen_handler, open mic\n");
    // duer_recorder_start();
}

void duer_dcs_stop_listen_handler(void)
{
    ESP_LOGI(TAG, "stop_listen, close mic\n");
    rec_engine_trigger_stop();
}

void duer_dcs_volume_set_handler(int volume)
{
    int ret = EspAudioVolSet(volume);
    if (ret == 0) {
        ESP_LOGI(TAG, "set_volume to %d\n", volume);
        duer_dcs_on_volume_changed();
    }
}

void duer_dcs_volume_adjust_handler(int volume)
{
    int curVol = 0;
    EspAudioVolGet(&curVol);
    curVol += volume;
    if (curVol >= 100) {
        curVol = 100;
    } else if (curVol <= 0) {
        curVol = 0;
    }
    int ret = EspAudioVolSet(curVol);
    ESP_LOGI(TAG, "adj_volume by:%d,current vol:%d", volume, curVol);
    if (ret == 0) {
        duer_dcs_on_volume_changed();
    }
}

void duer_dcs_mute_handler(duer_bool is_mute)
{
    int ret = -1;
    if (is_mute) {
        ret = EspAudioVolSet(is_mute);
    }
    if (ret == 0) {
        ESP_LOGI(TAG, "set_mute to %d\n", (int)is_mute);
        duer_dcs_on_mute();
    }
}

void duer_dcs_get_speaker_state(int *volume, duer_bool *is_mute)
{
    ESP_LOGI(TAG, "duer_dcs_get_speaker_state\n");
    *volume = 60;
    *is_mute = false;
    int ret = 0;
    int vol = 0;
    int mute = 0;
    ret = EspAudioVolGet(&vol);
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to get volume");
    } else {
        *volume = vol;
        if (vol > 0) {
            *is_mute = true;
        } else {
            *is_mute = false;
        }
    }
}
void duer_dcs_tone_handler(const char *url)
{
    EspAudioTonePlay(url, TERMINATION_TYPE_NOW, 0);
    rec_engine_detect_suspend(REC_VOICE_SUSPEND_ON);
    speech_type = DUER_AUDIO_TYPE_TONE;
}

void duer_dcs_speak_handler(const char *url)
{
    // ESP_LOGI(TAG, "Playing speak: %s\n", url);
    EspAudioTonePlay(url, TERMINATION_TYPE_NOW, 0);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    speech_type = DUER_AUDIO_TYPE_SPEECH;
    xSemaphoreGive(s_mutex);
}

void duer_dcs_audio_play_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Playing audio item_id:%s, offset:%d, url:%s\n", audio_info->audio_item_id, audio_info->offset, audio_info->url);
    EspAudioPlay(audio_info->url, audio_info->offset);
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    speech_type = DUER_AUDIO_TYPE_MUSIC;
    xSemaphoreGive(s_mutex);
}

void duer_dcs_audio_stop_handler()
{
    ESP_LOGI(TAG, "Stop audio play");
    //if audio player is playing speech, no need to stop audio player
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    int status = speech_type;
    xSemaphoreGive(s_mutex);
    if (status == 1) {
        ESP_LOGI(TAG, "Is playing speech, no need to stop");
    } else {
        EspAudioStop();
    }
}

void duer_dcs_audio_pause_handler()
{
    ESP_LOGI(TAG, "Pause audio play");
    EspAudioPause();

}

void duer_dcs_audio_resume_handler(const duer_dcs_audio_info_t *audio_info)
{
    ESP_LOGI(TAG, "Resume audio play");
    EspAudioResume();
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    speech_type = DUER_AUDIO_TYPE_MUSIC;

    xSemaphoreGive(s_mutex);
}

int duer_dcs_audio_get_play_progress()
{
    int ret = 0;
    uint32_t total_size = 0;
    int position = 0;
    ret = EspAudioPosGet(&position);
    if (ret == 0) {
        ESP_LOGI(TAG, "Get play position %d of %d", position, total_size);
        return position;
    } else {
        ESP_LOGE(TAG, "Failed to get play progress.");
        return -1;
    }
}
duer_audio_type_t duer_audio_wrapper_get_type()
{
    return speech_type;
}

int duer_audio_wrapper_set_type(duer_audio_type_t num)
{
    speech_type = num;
    return 0;
}

void duer_dcs_audio_active_paused()
{
    duer_dcs_audio_on_stopped();
    ESP_LOGI(TAG, "duer_dcs_audio_active_paused");
}

void duer_dcs_audio_active_play()
{
    if (duer_dcs_send_play_control_cmd(DCS_PLAY_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PLAY_CMD to DCS failed");
    }
    ESP_LOGI(TAG, "duer_dcs_audio_active_play");
}

void duer_dcs_audio_active_previous()
{
    if (duer_dcs_send_play_control_cmd(DCS_PREVIOUS_CMD)) {
        ESP_LOGE(TAG, "Send DCS_PREVIOUS_CMD to DCS failed");
    }
    ESP_LOGI(TAG, "Fduer_dcs_audio_active_previous");
}

void duer_dcs_audio_active_next()
{
    if (duer_dcs_send_play_control_cmd(DCS_NEXT_CMD)) {
        ESP_LOGE(TAG, "Send DCS_NEXT_CMD to DCS failed");
    }
    ESP_LOGI(TAG, "duer_dcs_audio_active_next");
}
