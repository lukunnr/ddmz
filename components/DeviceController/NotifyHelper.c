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

#include <string.h>
#include "NotifyHelper.h"
#include "DeviceController.h"
#include "MediaControl.h"
#include "MediaService.h"
#include "PlaylistManager.h"
#include "DeviceCommon.h"

#define NOTIFY_TAG  "EVENT_NOTIFY"

void SDCardEvtNotify(DeviceController *controller, DeviceNotifyType type, void *data, int len)
{
    MediaControl *ctrl = (MediaControl *) controller->instance;
    // AudioStream *inputStream = ctrl->inputStream;
    // AudioStream *outputStream = ctrl->outputStream;
    DeviceNotification note;
    memset(&note, 0x00, sizeof(DeviceNotification));
    note.type = type;
    //reserve note.targetService to be set at controller->notifyServices
    note.data = data;
    note.len = len;

    PlaylistManager *playlistManager = ctrl->playlistManager;
    if(playlistManager){
        note.receiver = (PlaylistManager *) playlistManager;
        playlistManager->deviceEvtNotified(&note);
    }
}

static void NotifyServices(DeviceController *controller, DeviceNotifyType type, void *data, int len)
{
    MediaControl *ctrl = (MediaControl *) controller->instance;
    MediaService *service = ctrl->services;
    DeviceNotification note;
    memset(&note, 0x00, sizeof(DeviceNotification));
    note.type = type;
    note.data = data;
    note.len = len;
    while (service->next != NULL) {
        service = service->next;
        note.receiver = (void *) service;
        if (service->deviceEvtNotified) {
            service->deviceEvtNotified(&note);
        }
    }
}

void TouchpadEvtNotify(DeviceController *controller, DeviceNotifyType type, void *data, int len)
{
    NotifyServices(controller, type, data, len);
}

void DspEvtNotify(DeviceController *controller, DeviceNotifyType type, void *data, int len)
{
    NotifyServices(controller, type, data, len);
}

void WifiEvtNotify(DeviceController *controller, DeviceNotifyType type, void *data, int len)
{
    NotifyServices(controller, type, data, len);
}

void AuxInEvtNotify(DeviceController *controller, DeviceNotifyType type, void *data, int len)
{
    MediaControl *ctrl = (MediaControl *) controller->instance;
    if (*((DeviceNotifyAuxInMsg *) data) == DEVICE_NOTIFY_AUXIN_INSERTED) {
        ctrl->audioInfo.player.status = AUDIO_STATUS_AUX_IN;
    } else if (*((DeviceNotifyAuxInMsg *) data) == DEVICE_NOTIFY_AUXIN_REMOVED) {
        ctrl->audioInfo.player.status = AUDIO_STATUS_PAUSED;
    }
    NotifyServices(controller, type, data, len);
}
