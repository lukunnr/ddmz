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
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "EspAppControlService.h"
#include "MediaService.h"
#include "PlaylistDef.h"
#include "webserver.h"
#include "DeviceCommon.h"
#include "AudioDef.h"
#include "toneuri.h"
#include "EspAudioAlloc.h"
#include "esp_audio_log.h"


#define ESP_APP_CTRL_TAG                "ESP_APP_CTRL"
#define ESP_APP_SERV_TASK_PRIORITY      4
#define ESP_APP_SERV_TASK_STACK_SIZE    3072
#define ITEM_SIZE_QUE_WEB_CTRL          sizeof(MsgCtrlPkg)
#define ESP_APP_URI_DEFAULT             "http://download.xmcdn.com/group18/M01/BB/E9/wKgJJVe_ucqygU53AGVDRMtGrPY469.aac"

static xQueueHandle queWebCtrl;

static void DeviceEvtNotifiedToApp(DeviceNotification *note)
{
    EspAppControlService *service = (EspAppControlService *) note->receiver;
    DeviceNotifyMsg msg = *((DeviceNotifyMsg *) note->data);
    if (note->type == DEVICE_NOTIFY_TYPE_WIFI) {
        service->_toneEnable = 1;
        switch (msg) {
        case DEVICE_NOTIFY_WIFI_GOT_IP:
//        ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "Got notified for status GOT IP");
            service->_wifiConnected = 1;
            user_devicefind_start();
            if (NULL == queWebCtrl) {
                queWebCtrl = xQueueCreate(3, ITEM_SIZE_QUE_WEB_CTRL);
                configASSERT(queWebCtrl);
            }
            user_webserver_start(queWebCtrl);
            break;
        case DEVICE_NOTIFY_WIFI_SC_DISCONNECTED:
            service->_toneEnable = 0;
        case DEVICE_NOTIFY_WIFI_DISCONNECTED:
//        ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "Got notified for status DISCONNECTED");
            service->_wifiConnected = 0;
            user_devicefind_stop();
            user_webserver_stop();
            break;
        default:
            break;
        }
    } else if (note->type == DEVICE_NOTIFY_TYPE_AUXIN) {
        switch (msg) {
        case DEVICE_NOTIFY_AUXIN_INSERTED:
            service->Based._blocking = 1;
            break;
        case DEVICE_NOTIFY_AUXIN_REMOVED:
            service->Based._blocking = 0;
            break;
        default:
            break;
        }
    } else if (note->type == DEVICE_NOTIFY_TYPE_SDCARD) {
        switch (msg) {
        case DEVICE_NOTIFY_SD_MOUNTED:
            service->_sd_mounted = DEVICE_NOTIFY_SD_MOUNTED;
            break;
        case DEVICE_NOTIFY_SD_UNMOUNTED:
            service->_sd_mounted = DEVICE_NOTIFY_SD_UNMOUNTED;
            break;
        default:
            break;
        }
    }
}
static void PlayerStatusUpdatedToApp(ServiceEvent *event)
{

}

static void EspAppControlTask(void *pvParameters)
{
    EspAppControlService *service  = (EspAppControlService *) pvParameters;

    MsgCtrlPkg *pQueRecv = EspAudioAlloc(1, sizeof(MsgUrlPkg));
    if (NULL == pQueRecv) {
        ESP_AUDIO_LOGE(ESP_APP_CTRL_TAG, "pQueRecv malloc failed.");
        vTaskDelete(NULL);
        return;
    }
    memset(pQueRecv, 0, sizeof(MsgUrlPkg));
    int webserverInited = 0;
    ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "[APP] Free memory: %d bytes\n", esp_get_free_heap_size());
    static QueueSetHandle_t xQueSet;
    QueueSetMemberHandle_t activeMember;
    xQueSet = xQueueCreateSet(3 + 2);
    xQueueHandle xQuePlayerStatus = xQueueCreate(2, sizeof(PlayerStatus));
    configASSERT(xQueSet);
    configASSERT(xQuePlayerStatus);
    service->Based.addListener((MediaService *)service, xQuePlayerStatus);
    xQueueAddToSet(xQuePlayerStatus, xQueSet);
    xQueueAddToSet(queWebCtrl, xQueSet);
    service->_sd_mounted = -1;
    while (1) {
        if (service->_wifiConnected) {  //Wifi connected
            if (!webserverInited) {  //XXX: better way to do these in WifiManager
                webserverInited = 1;
                if ((service->_toneEnable) && service->Based.playTone && (!service->Based._blocking)) {
                    service->Based.playTone(service, tone_uri[TONE_TYPE_WIFI_SUCCESS], TERMINATION_TYPE_DONE);
                }
            }
            activeMember = xQueueSelectFromSet( xQueSet, 2000 / portTICK_PERIOD_MS );
            // ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "--------time is %d", service->Based.getPosByTime((MediaService *)service));
            if ( activeMember == queWebCtrl ) {
                xQueueReceive(activeMember, pQueRecv, 0);
                MsgCtrlPkg *pCtrl = (MsgCtrlPkg *)pQueRecv;
                ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "queWebCtrl has been received\r\n");
                if (!service->Based._blocking) {
                    if (AudioCtlType_Url == pCtrl->type) {
                        MsgUrlPkg *pUrl = (MsgUrlPkg *)pCtrl;
                        ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "Recv a WebUrlPkg Evt.Type=%d,live=%d,url=%s,name=%s", pUrl->type, pUrl->live, pUrl->url, pUrl->name);
//                        if (service->Based.saveToPlaylist) {
//                            service->Based.saveToPlaylist((MediaService *) service, PLAYLIST_IN_FLASH_DEFAULT, pUrl->url);
//                        }
                        if (service->Based.setPlayMode) {
                            service->Based.setPlayMode((MediaService *) service, MEDIA_PLAY_ONE_SONG);
                        }
                        if (service->Based.addUri) {  //XXX: need a return of the track being added, so use the ctrl interface addTrack instead of service->addUri
                            service->Based.addUri((MediaService *) service, pUrl->url);
//                            AudioTrack *track = service->Based.addUri((MediaService *) service, pUrl->url);
                            // if (service->Based.setTrack) {
                            //     service->Based.setTrack(service->Based.playlist, track);
                            // }
                        }
                        if (service->Based.mediaPlay) {
                            service->Based.mediaPlay((MediaService *) service);
                        }
                        if (pUrl->url) {
                            free(pUrl->url);
                            pUrl->url = NULL;
                        }
                        if (pUrl->name) {
                            free(pUrl->name);
                            pUrl->name = NULL;
                        }

                    } else  if (AudioCtlType_Play == pCtrl->type) {
                        ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "recv AudioCtlType_Play %d (line: %d)", pCtrl->value, __LINE__);
                        if (MediaPlayStatus_Start == pCtrl->value) {
                            if (service->Based.mediaResume) {
                                service->Based.mediaResume((MediaService *) service);
                            }
                        } else if (MediaPlayStatus_Pause == pCtrl->value) {
                            if (service->Based.mediaPause) {
                                service->Based.mediaPause((MediaService *) service);
                            }
                        } else if (MediaPlayStatus_Stop == pCtrl->value) {
                            if (service->Based.mediaStop) {
                                service->Based.mediaStop((MediaService *) service);
                            }
                        } else {
                            ESP_AUDIO_LOGE(ESP_APP_CTRL_TAG, "Recv a WebCtrlPkg value=%d.But it will not be here", pCtrl->value);
                        }

                    } else if (AudioCtlType_Vol == pCtrl->type) {
                        ESP_AUDIO_LOGV(ESP_APP_CTRL_TAG, "recv AudioCtlType_Vol %d", pCtrl->value);
                        /* cell phone & ESP32 interface */
                        //TODO: need a better range conversion
                        // value := 50 - 100
                        int value = (pCtrl->value - 30) * 1.5;
                        if (value > 100)
                            value = 100;
                        else if (value < 30)
                            value = 30;
                        if (service->Based.setVolume) {
                            service->Based.setVolume((MediaService *) service, 0, pCtrl->value);
                        }
                    } else if (AudioCtlType_List == pCtrl->type) {
                        MsgListPkg *pList = (MsgListPkg *)pCtrl;
                        ESP_AUDIO_LOGV(ESP_APP_CTRL_TAG, "recv AudioCtlType_List live=%d, count=%d", pList->live, pList->count);
                        PlayListId dest = 0;
                        if (service->_sd_mounted != DEVICE_NOTIFY_SD_MOUNTED) {
                            dest = getDefaultFlashIndex();
                        } else {
                            dest = pList->live ? getDefaultSdIndex() + 2 : getDefaultSdIndex() + 1;
                        }
                        int ind = 0;
                        int res;
                        for (int i = 0; i < pList->count; i++) {
//                            ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "Received: %s", &pList->buf[ind]);
                            res = service->Based.saveToPlaylist(dest, &pList->buf[ind]);
                            if (res < 0) {
                                break;
                            }
                            ind += (strlen(&pList->buf[ind]) + 1);
                        }
                        service->Based.closePlaylist(dest);
                        if (pList->buf) {
                            free(pList->buf);
                            pList->buf = NULL;
                        }
                    } else {
                        ESP_AUDIO_LOGE(ESP_APP_CTRL_TAG, "Not support!It will not be here. Type=%d", pCtrl->type);
                    }
                }
            } else if ( activeMember == xQuePlayerStatus ) {
                PlayerStatus player = {0};
                xQueueReceive( activeMember, &player, 0 );
                ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, " \r\n\r\n--------%s, handle = %p, Mode:%d,Status:0x%02x,ErrMsg:%x --------\r\n\r\n",
                         __func__, activeMember, player.mode, player.status, player.errMsg);
            } else {
                // TODO nothing
            }
        } else {
            if (webserverInited) {
                webserverInited = 0;
                if ((service->_toneEnable) && service->Based.playTone && (!service->Based._blocking)) {
                    service->Based.playTone(service, tone_uri[TONE_TYPE_WIFI_RECONNECT], TERMINATION_TYPE_NOW);
                }
            } else {
                vTaskDelay(1000 / portTICK_PERIOD_MS);  //XXX: for the case when wifi is not connected. can be improved
            }
        }
    }
    if (pQueRecv) {
        free(pQueRecv);
    }
    vTaskDelete(NULL);
}

static void EspAppControlActive(MediaService *self)
{
    EspAppControlService *service = (EspAppControlService *) self;
    ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "EspAppControl active \r\n");
    if (NULL == queWebCtrl) {
        queWebCtrl = xQueueCreate(3, ITEM_SIZE_QUE_WEB_CTRL);
        configASSERT(queWebCtrl);
    }
    if (xTaskCreatePinnedToCore(EspAppControlTask,
                                "EspAppControlTask",
                                ESP_APP_SERV_TASK_STACK_SIZE,
                                service,
                                ESP_APP_SERV_TASK_PRIORITY,
                                NULL, xPortGetCoreID()) != pdPASS) {
        ESP_AUDIO_LOGE(ESP_APP_CTRL_TAG, "Error create controlTask");
    }
}

static void EspAppControlDeactive(MediaService *self)
{
//    EspAppControlService *service = (EspAppControlService *) self;
    ESP_AUDIO_LOGI(ESP_APP_CTRL_TAG, "EspAppControlStop\r\n");
    vQueueDelete(queWebCtrl);
    queWebCtrl = NULL;
    //TODO: is it necessary to remove the reserved track in playlist?
}

EspAppControlService *EspAppControlCreate()
{
    EspAppControlService *mobile = (EspAppControlService *) EspAudioAlloc(1, sizeof(EspAppControlService));
    ESP_ERROR_CHECK(!mobile);

    mobile->Based.deviceEvtNotified = DeviceEvtNotifiedToApp;
    mobile->Based.playerStatusUpdated = PlayerStatusUpdatedToApp;
    mobile->Based.serviceActive = EspAppControlActive;
    mobile->Based.serviceDeactive = EspAppControlDeactive;
    return mobile;
}
