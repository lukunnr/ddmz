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

#ifndef DUER_AUDIO_WRAPPER_H
#define DUER_AUDIO_WRAPPER_H

#include "lightduer_dcs.h"
#include "AudioDef.h"
typedef  int duer_audio_play_type_t;

typedef enum {
    DUER_AUDIO_TYPE_UNKOWN,
    DUER_AUDIO_TYPE_SPEECH,
    DUER_AUDIO_TYPE_MUSIC,
    DUER_AUDIO_TYPE_TONE,
    DUER_AUDIO_TYPE_ALERT_SPEECH,
    DUER_AUDIO_TYPE_ALERT_RINGING,
} duer_audio_type_t;

typedef void (*duer_audio_wrapper_cb)(duer_audio_type_t type, AudioStatus status);

/*
 * init audio wrapper
 *
 * @param none
 *
 * @return ESP_OK or ESP_FAIL
 *
 */
esp_err_t duer_audio_wrapper_init();

/*
 * set audio wrapper callback function
 *
 * @param none
 *
 * @return ESP_OK or ESP_FAIL
 *
 */
esp_err_t duer_audio_wrapper_callback_register(duer_audio_wrapper_cb cb);

/*
 * deinit audio wrapper
 *
 * @param none
 *
 * @return ESP_OK or ESP_FAIL
 *
 */
esp_err_t duer_audio_wrapper_deinit();

/*
 * play alert type url
 *
 * @param none
 *
 * @return none
 *
 */
void duer_audio_wrapper_alert_play(const char *url, duer_audio_type_t type);

/*
 * Stop the alert playing.
 *
 * @param none
 *
 * @return none
 *
 */
void duer_audio_wrapper_alert_stop();

/*
 * Get audio wrapper audio type
 *
 * @param none
 *
 * @return duer_audio_type_t
 *
 */
duer_audio_type_t duer_audio_wrapper_get_type();

/*
 * Set audio wrapper audio type
 *
 * @param num: duer_audio_type_t
 *
 * @return success:0
 *             fail: others
 */
int duer_audio_wrapper_set_type(duer_audio_type_t num);

/*
 * DCS init function
 *
 * @param void:
 *
 * @return void:
 */
void duer_dcs_init(void);

/*
 * DCS audio on_started callback function
 *
 * @param flag: duer_audio_play_type_t
 *
 * @return success:0
 *             fail: others
 */
int duer_dcs_audio_on_started_cb(duer_audio_play_type_t flag);

/*
 * DCS audio on_finished callback function
 *
 * @param flag: duer_audio_play_type_t
 *
 * @return success:0
 *             fail: others
 */
int duer_dcs_audio_on_finished_cb(duer_audio_play_type_t flag);

/*
 * Send DCS_PAUSE_CMD to DCS
 *
 * @param : none
 *
 * @return : none
 */
void duer_dcs_audio_active_paused();

/*
 * Send DCS_PLAY_CMD to DCS
 *
 * @param : none
 *
 * @return : none
 */
void duer_dcs_audio_active_play();

/*
 * Send DCS_PREVIOUS_CMD to DCS
 *
 * @param : none
 *
 * @return : none
 */
void duer_dcs_audio_active_previous();

/*
 * Send DCS_NEXT_CMD to DCS
 *
 * @param : none
 *
 * @return : none
 */
void duer_dcs_audio_active_next();

/*
 * Play tone
 *
 * @param : url
 *
 * @return : none
 */
void duer_dcs_tone_handler(const char *url);

#endif //
