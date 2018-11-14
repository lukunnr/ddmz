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

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "esp_audio_log.h"

#include "DspManager.h"
#include "NotifyHelper.h"
#include "DeviceCommon.h"

//#include "recorder_engine.h"

#define DSP_TAG                              "DspManager"

#define DSP_MANAGER_TASK_PRIORITY             6
#define DSP_MANAGER_TASK_STACK_SIZE           2048

SemaphoreHandle_t dsp_asr_sema = NULL;

static TaskHandle_t DspManageHandle;

static void DspManagerEvtTask(void* pvParameters)
{
    DspManager* manager = (DspManager*) pvParameters;
    DeviceNotifyMsg msg = DEVICE_NOTIFY_DSP_WAKEUP;
    vSemaphoreCreateBinary(dsp_asr_sema);
    xSemaphoreTake(dsp_asr_sema, 0);

    while (1) {
        xSemaphoreTake(dsp_asr_sema, portMAX_DELAY);
        // Filter all false triggers
        vTaskDelay(100 / portTICK_RATE_MS);
        xSemaphoreTake(dsp_asr_sema, 0);
        msg = DEVICE_NOTIFY_DSP_WAKEUP;

        if (msg != DEVICE_NOTIFY_KEY_UNDEFINED) {
            manager->Based.notify((struct DeviceController*) manager->Based.controller, DEVICE_NOTIFY_TYPE_DSP, &msg, sizeof(DeviceNotifyMsg));
        }
    }

    vTaskDelete(NULL);
}


static int DspManagerActive(DeviceManager* self)
{
    DspManager* manager = (DspManager*) self;

    if (xTaskCreatePinnedToCore(DspManagerEvtTask,
                                "DspManagerEvtTask",
                                DSP_MANAGER_TASK_STACK_SIZE,
                                manager,
                                DSP_MANAGER_TASK_PRIORITY,
                                &DspManageHandle, xPortGetCoreID()) != pdPASS) {
        ESP_AUDIO_LOGE(DSP_TAG, "Error create DspManagerTask");
        return -1;
    }

    return 0;
}

static void DspManagerDeactive(DeviceManager* self)
{
    //TODO
    ESP_AUDIO_LOGI(DSP_TAG, "DspManagerStop\r\n");
    //vQueueDelete(xQueueTouch);
    //xQueueTouch = NULL;
}

DspManager* DspManagerCreate(struct DeviceController* controller)
{
    if (!controller) {
        return NULL;
    }

    ESP_AUDIO_LOGI(DSP_TAG, "DspManagerCreate\r\n");
    DspManager* dsp = (DspManager*) calloc(1, sizeof(DspManager));
    ESP_ERROR_CHECK(!dsp);
    InitManager((DeviceManager*) dsp, controller);
    dsp->Based.active = DspManagerActive;
    dsp->Based.deactive = DspManagerDeactive;
    dsp->Based.notify = DspEvtNotify;  //TODO
    return dsp;
}
