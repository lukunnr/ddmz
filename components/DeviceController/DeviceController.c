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

/**
 * DeviceController interface
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "esp_audio_log.h"
#include "esp_err.h"

#include "DeviceController.h"
#include "DeviceControllerHelper.h"
#include "DeviceCommon.h"
#include "MediaService.h"
#include "DeviceManager.h"
/* only for public constructors */
#include "TouchManager.h"
#include "DspManager.h"
#include "WifiManager.h"
#include "SDCardManager.h"
#include "AuxInManager.h"
#include "EspAudioAlloc.h"
#include "esp_audio_log.h"


#define DEVICE_CTRL_TAG                     "DEVICE_CONTROLLER"

/* ================================================
 * DeviceController implementation for wifi manager
 * ================================================
 */
static void WifiSmartConfig(DeviceController *controller)
{
    WifiManager *wifi = (WifiManager *) controller->wifi;
    if (wifi) {
        wifi->smartConfig(wifi);
    } else {
        ESP_AUDIO_LOGE(DEVICE_CTRL_TAG, "WIFI manager not found");
    }
}

static void WifiBleConfig(DeviceController *controller)
{
	WifiManager *wifi = (WifiManager *) controller->wifi;
	if (wifi) {
		wifi->bleConfig(wifi);
	} else {
		ESP_AUDIO_LOGE(DEVICE_CTRL_TAG, "WIFI manager not found");
	}
}

static void WifiBleConfigStop(DeviceController *controller)
{
	WifiManager *wifi = (WifiManager *) controller->wifi;
	if (wifi) {
		wifi->bleConfigStop(wifi);
	} else {
		ESP_AUDIO_LOGE(DEVICE_CTRL_TAG, "WIFI manager not found");
	}
}

//XXX: alternative  --  active one manager a time
static void ControllerActiveManagers(DeviceController *controller)
{
    if (controller->wifi) {
        controller->wifi->active(controller->wifi);
    }
    if (controller->sdcard) {
        controller->sdcard->active(controller->sdcard);
    }
    if (controller->touch) {
        controller->touch->active(controller->touch);
    }
    if (controller->dsp) {
        controller->dsp->active(controller->dsp);
    }
    if (controller->auxIn) {
        controller->auxIn->active(controller->auxIn);
    }

}

static void ControllerEnableWifi(DeviceController *controller)
{
    controller->wifi = (DeviceManager *) WifiConfigCreate(controller);
}

static void ControllerEnableTouch(DeviceController *controller)
{
    controller->touch = (DeviceManager *) TouchManagerCreate(controller);
}

static void ControllerEnableDsp(DeviceController *controller)
{
    controller->dsp = (DeviceManager *) DspManagerCreate(controller);
}

static void ControllerEnableSDcard(DeviceController *controller)
{
    controller->sdcard = (DeviceManager *) SDCardManagerCreate(controller);
    controller->sdcard->pauseMedia = ControllerPauseMedia;
    controller->sdcard->resumeMedia = ControllerResumeMedia;
}

static void ControllerEnableAuxIn(DeviceController *controller)
{
    controller->auxIn = (DeviceManager *) AuxInManagerCreate(controller);  //TODO
    controller->auxIn->pauseMedia = ControllerPauseMedia;
}

DeviceController *DeviceCtrlCreate(struct MediaControl *ctrl)
{
    ESP_AUDIO_LOGI(DEVICE_CTRL_TAG, "Device Controller Create\r\n");
    DeviceController *deviceCtrl = (DeviceController *) EspAudioAlloc(1, sizeof(DeviceController));
    ESP_ERROR_CHECK(!deviceCtrl);
    deviceCtrl->instance = (void *) ctrl;
    deviceCtrl->enableWifi = ControllerEnableWifi;
    deviceCtrl->enableTouch = ControllerEnableTouch;
    deviceCtrl->enableDsp = ControllerEnableDsp;
    deviceCtrl->enableSDcard = ControllerEnableSDcard;
    deviceCtrl->enableAuxIn = ControllerEnableAuxIn;
    deviceCtrl->activeManagers = ControllerActiveManagers;
    deviceCtrl->wifiSmartConfig = WifiSmartConfig;
    deviceCtrl->wifiBleConfig = WifiBleConfig;
    deviceCtrl->wifiBleConfigStop = WifiBleConfigStop;
    return deviceCtrl;
}
