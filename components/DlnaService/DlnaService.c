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
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"

#include "esp_audio_log.h"
#include "esp_system.h"
#include "DlnaService.h"
#include "AudioDef.h"

#include "espdlna.h"
#include "EspAudioAlloc.h"
#include "EspAudio.h"
#include "DeviceCommon.h"

#include "MediaControl.h"
#include "esp_audio_log.h"

const char *DLNA_TAG = "DLNA";

#define DLNA_TASK_STACK_SIZE (2048 + 512)
#define DLNA_TASK_PRIORITY 5
static int HighWaterLine = 0;
TaskHandle_t DlnaHandle;

int onMcpRequest(void *context, int type, void *data)
{
    DlnaService *service = (DlnaService *)context;
    MediaControl *ctrl = (MediaControl *) service->Based.instance;
    MusicInfo info;
    int vol;
    switch (type) {
        case RENDERER_SET_URI:
            if (service->Based.addUri)
                service->Based.addUri(service, (const char*)data);
            break;
        case RENDERER_PLAYMODE_PLAY:
            EspAudioPlayerSrcSet(MEDIA_SRC_DLNA);
            if (ctrl->curPlayer.status == AUDIO_STATUS_PAUSED) {
                service->Based.mediaResume(service);
            } else if (service->Based.mediaPlay) {
                service->Based.mediaPlay(service);
            }
            break;
        case RENDERER_PLAYMODE_PAUSE:
            if (service->Based.mediaPause)
                service->Based.mediaPause(service);
            break;
        case RENDERER_PLAYMODE_STOP:
            if (service->Based.mediaStop)
                service->Based.mediaStop(service);
            EspAudioPlayerSrcSet(MEDIA_SRC_NULL);
            break;
        case RENDERER_GET_CURRENT_POS:
            return service->Based.getPosByTime((MediaService*)service);
        case RENDERER_SET_TIMEPOS:
            return service->Based.seekByTime((MediaService*)service, (int)data);
        case RENDERER_GET_TOTAL_DURATION:
            service->Based.getSongInfo((MediaService*)service, &info);
            return (info.totalTime);
        case RENDERER_CHECK_PLAYING:
            return (ctrl->curPlayer.status == AUDIO_STATUS_PLAYING);
        case RENDERER_GETSET_VOLUME:
            vol = (int)data;
            if (vol >= 0)
                service->Based.setVolume((MediaService*)service, 0, vol);
            return service->Based.getVolume((MediaService*)service);
    }
    return 0;
}

static void DeviceEvtNotifiedToDlna(DeviceNotification* note)
{
    DlnaService* service = (DlnaService*) note->receiver;
    DeviceNotifyMsg msg = *((DeviceNotifyMsg*) note->data);

    if (note->type == DEVICE_NOTIFY_TYPE_WIFI) {
        switch (msg) {
            case DEVICE_NOTIFY_WIFI_GOT_IP:

                service->wifiState = 1;
                break;
            case DEVICE_NOTIFY_WIFI_DISCONNECTED:
                service->wifiState = 0;
                break;

            default:
                break;
        }
    }
}

void submitPlayerChanged()
{
    //volumn change
    //mute change
    //Song change
    //Mode change
}

void dlnaControlTask(void *pv)
{
    DlnaService *service  = (DlnaService *) pv;
    PlayerStatus playerStatus;

    // while (service->wifiState == 0) {
    //     vTaskDelay(1000 / portTICK_RATE_MS);
    //     ESP_AUDIO_LOGI(DLNA_TAG, "waiting for wifi connecting");
    // }

    xQueueHandle xQueuePlayerStatus = xQueueCreate(2, sizeof(PlayerStatus));
    service->Based.addListener((MediaService *)service, xQueuePlayerStatus);
    renderer_t *renderer = service->dlna->renderer;
    MusicInfo info = {0};
    PlaySrc src;

    while (service->_run) {
        if (xQueueReceive(xQueuePlayerStatus, &playerStatus, portMAX_DELAY)) {
            EspAudioPlayerSrcGet(&src);
            ESP_AUDIO_LOGE(DLNA_TAG, "Player status update = %d", playerStatus.status);
            // if (src != MEDIA_SRC_DLNA) {
            //     continue;
            // }
            switch (playerStatus.status) {
                case AUDIO_STATUS_PLAYING:
                    service->Based.getSongInfo((MediaService*)service, &info);
                    renderer_playing_notify(renderer, 1, 1, info.totalTime);
                    break;
                case AUDIO_STATUS_PAUSED:
                    break;
                case AUDIO_STATUS_STOP:
                case AUDIO_STATUS_FINISHED:
                    renderer_stop_notify(renderer);
                case AUDIO_STATUS_UNKNOWN:
                case AUDIO_STATUS_ERROR:
                case AUDIO_STATUS_AUX_IN:
                    break;
            }
#if EN_STACK_TRACKER
            if (uxTaskGetStackHighWaterMark(DlnaHandle) > HighWaterLine) {
                HighWaterLine = uxTaskGetStackHighWaterMark(DlnaHandle);
                ESP_AUDIO_LOGI("STACK", "%s %d\n\n\n", __func__, HighWaterLine);
            }
#endif
        }
    }
    vQueueDelete(xQueuePlayerStatus);
    vTaskDelete(NULL);
}

void dlnaControlActive(DlnaService *service)
{
    ESP_AUDIO_LOGV(DLNA_TAG, "dlnaControlActive");
    EspAudioPrintMemory(DLNA_TAG);

    service->_run = 1;
    service->dlna = dlna_init("ESP32 MD (ESP32 Renderer)", "8db0797a-f01a-4949-8f59-51188b181809", "00001", 80, service, onMcpRequest);

    if (xTaskCreatePinnedToCore(dlnaControlTask,
                                "dlnaControlTask",
                                DLNA_TASK_STACK_SIZE,
                                service,
                                DLNA_TASK_PRIORITY,
                                &DlnaHandle, xPortGetCoreID()) != pdPASS) {
        ESP_AUDIO_LOGE(DLNA_TAG, "Error create controlTask");
    }
    //active dlna callback
}

void dlnaControlDeactive(DlnaService *service)
{
    service->_run = 0;
    while (service->_run) {
        ESP_AUDIO_LOGI(DLNA_TAG, "Stopping dlna service...");
        vTaskDelay(10 / portTICK_RATE_MS);
    }
    if (service->dlna) {
        dlna_destroy(service->dlna);
        service->dlna = NULL;
    }
    ESP_AUDIO_LOGV(DLNA_TAG, "dlnaControlStop\r\n");

}

DlnaService *DlnaCreate(const char *name, const char *udn, const char *serial)
{
    DlnaService *dlnaService = (DlnaService *) EspAudioAlloc(1, sizeof(DlnaService));
    ESP_ERROR_CHECK(!dlnaService);

    dlnaService->Based.playerStatusUpdated = NULL;
    dlnaService->Based.deviceEvtNotified = DeviceEvtNotifiedToDlna;
    dlnaService->Based.serviceActive = dlnaControlActive;
    dlnaService->Based.serviceDeactive = dlnaControlDeactive;
    return dlnaService;
}
