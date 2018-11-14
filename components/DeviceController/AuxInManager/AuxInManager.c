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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_audio_log.h"
#include "esp_err.h"

#include "DeviceManager.h"
#include "DeviceCommon.h"
#include "NotifyHelper.h"
#include "AuxInManager.h"
#include "auxinconfig.h"
#include "MediaHal.h"
#include "esp_audio_log.h"

#define AUXIN_MANAGER_TAG  "AUX_IN"

#define AUXIN_DETECTION_TASK_PRORITY                    1
#define AUXIN_DETECTION_TASK_STACK_SIZE                 (2048 + 512)
#define AUXIN_EVT_QUEUE_LEN                             1

static xQueueHandle auxInEvtQueue;
TaskHandle_t AuxInTaskHandle;
static int HighWaterLine = 0;

static void AuxInDetectionTask(void *pvParameters)
{
    AuxInManager *manager = (AuxInManager *) pvParameters;
    int trigger;
    while (1) {
        if (xQueuePeek(auxInEvtQueue, &trigger, portMAX_DELAY)) {
            DeviceNotifyAuxInMsg msg = DEVICE_NOTIFY_AUXIN_UNDEFINED;
            int status = auxin_status_detect();
            if (manager->Based.pauseMedia) {
                manager->Based.pauseMedia((DeviceManager *) manager);
            }
            if (status == 0) {  //auxin inserted
                ESP_AUDIO_LOGI(AUXIN_MANAGER_TAG, "Aux in cable inserted");
                msg = DEVICE_NOTIFY_AUXIN_INSERTED;
                MediaHalStart(CODEC_MODE_LINE_IN);

            } else {
                ESP_AUDIO_LOGI(AUXIN_MANAGER_TAG, "Aux in cable removed");
                msg = DEVICE_NOTIFY_AUXIN_REMOVED;
                MediaHalStop(CODEC_MODE_LINE_IN);
                MediaHalStart(CODEC_MODE_DECODE_ENCODE);
            }
            manager->Based.notify((struct DeviceController *) manager->Based.controller, DEVICE_NOTIFY_TYPE_AUXIN, &msg, sizeof(DeviceNotifyAuxInMsg));
            xQueueReceive(auxInEvtQueue, &trigger, portMAX_DELAY);
            #if EN_STACK_TRACKER
            if(uxTaskGetStackHighWaterMark(AuxInTaskHandle) > HighWaterLine){
                HighWaterLine = uxTaskGetStackHighWaterMark(AuxInTaskHandle);
                ESP_AUDIO_LOGI("STACK", "%s %d\n\n\n", __func__, HighWaterLine);
            }
            #endif
        }
    }
    vTaskDelete(NULL);
}

static int AuxInManagerActive(DeviceManager *self)
{
    ESP_AUDIO_LOGI(AUXIN_MANAGER_TAG, "AuxInManager Active\r\n");
    AuxInManager *manager = (AuxInManager *) self;
    auxInEvtQueue = xQueueCreate(AUXIN_EVT_QUEUE_LEN, sizeof(int));
    configASSERT(auxInEvtQueue);
    auxin_intr_init(auxInEvtQueue);
    int status = auxin_status_detect();
    if (status == 0) {
        if (manager->Based.pauseMedia) {
            manager->Based.pauseMedia((DeviceManager *) manager);
        }
        MediaHalStart(CODEC_MODE_LINE_IN);
        DeviceNotifyAuxInMsg msg = DEVICE_NOTIFY_AUXIN_INSERTED;
        manager->Based.notify((struct DeviceController *) manager->Based.controller, DEVICE_NOTIFY_TYPE_AUXIN, &msg, sizeof(DeviceNotifyAuxInMsg));
    }
    if (xTaskCreatePinnedToCore(AuxInDetectionTask,
                                "AuxInDetectionTask",
                                AUXIN_DETECTION_TASK_STACK_SIZE,
                                manager,
                                AUXIN_DETECTION_TASK_PRORITY,
                                &AuxInTaskHandle, 0) != pdPASS) {

        ESP_AUDIO_LOGE(AUXIN_MANAGER_TAG, "ERROR creating AuxInDetectionTask! Out of memory?");
        return -1;
    }
    return 0;
}
static void AuxInManagerDeactive(DeviceManager *self)
{
    //TODO
}

AuxInManager *AuxInManagerCreate(struct DeviceController *controller)
{
    if (!controller)
        return NULL;
    ESP_AUDIO_LOGI(AUXIN_MANAGER_TAG, "AuxInManager Create\r\n");
    AuxInManager *auxIn = (AuxInManager *) calloc(1, sizeof(AuxInManager));
    ESP_ERROR_CHECK(!auxIn);

    InitManager((DeviceManager *) auxIn, controller);

    auxIn->Based.active = AuxInManagerActive;
    auxIn->Based.deactive = AuxInManagerDeactive;
    auxIn->Based.notify = AuxInEvtNotify;
    return auxIn;
}
