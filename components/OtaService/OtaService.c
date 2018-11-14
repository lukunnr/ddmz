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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_audio_log.h"
#include "esp_system.h"

#include "Ota.h"
#include "MediaService.h"
#include "OtaService.h"

#include "DeviceCommon.h"

#define OTA_SERV_TAG "OTA_SERVICE"
#define OTA_SERVICE_TASK_PRIORITY               13
#define OTA_SERVICE_TASK_STACK_SIZE             3*1024

static void DeviceEvtNotifiedToOTA(DeviceNotification *note)
{
    OtaService *service = (OtaService *) note->receiver;
    if (DEVICE_NOTIFY_TYPE_WIFI == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg *) note->data);
        switch (msg) {
        case DEVICE_NOTIFY_WIFI_GOT_IP:
            xQueueSend(service->_OtaQueue, &msg, 0);
            break;
        case DEVICE_NOTIFY_WIFI_DISCONNECTED:
            xQueueSend(service->_OtaQueue, &msg, 0);
            break;
        default:
            break;
        }
    }
}

static void PlayerStatusUpdatedToOTA(ServiceEvent *event)
{

}

static void OtaServiceTask(void *pvParameters)
{
    OtaService *service  = (OtaService *) pvParameters;
    ESP_AUDIO_LOGI(OTA_SERV_TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    uint32_t wifiStatus = 0;
    while (1) {
        xQueueReceive(service->_OtaQueue, &wifiStatus, portMAX_DELAY);
        if (wifiStatus == DEVICE_NOTIFY_WIFI_GOT_IP) {
            ESP_AUDIO_LOGI(OTA_SERV_TAG, "Wifi has been connected ! Start to check bins version....");
            if (OtaServiceStart() == 0) {
                esp_restart();
            } else {
                ESP_AUDIO_LOGE(OTA_SERV_TAG, "Execute OTA failed");
            }
            wifiStatus = 0;
        }
    }
    vTaskDelete(NULL);
}

static void OtaServiceActive(OtaService *service)
{
    ESP_AUDIO_LOGI(OTA_SERV_TAG, "OtaService active");
    if (xTaskCreatePinnedToCore(OtaServiceTask,
                                "OtaServiceTask",
                                OTA_SERVICE_TASK_STACK_SIZE,
                                service,
                                OTA_SERVICE_TASK_PRIORITY,
                                NULL, xPortGetCoreID()) != pdPASS) {
        ESP_AUDIO_LOGE(OTA_SERV_TAG, "Error create OtaServiceTask");
    }
}

static void OtaServiceDeactive(OtaService *service)
{
    ESP_AUDIO_LOGI(OTA_SERV_TAG, "OtaServiceStop");
    vQueueDelete(service->_OtaQueue);
    service->_OtaQueue = NULL;
}

OtaService *OtaServiceCreate()
{
    OtaService *ota = (OtaService *) calloc(1, sizeof(OtaService));
    ESP_ERROR_CHECK(!ota);
    ota->_OtaQueue = xQueueCreate(1, sizeof(uint32_t));
    configASSERT(ota->_OtaQueue);
    ota->Based.deviceEvtNotified = DeviceEvtNotifiedToOTA;
    ota->Based.playerStatusUpdated = PlayerStatusUpdatedToOTA;
    ota->Based.serviceActive = OtaServiceActive;
    ota->Based.serviceDeactive = OtaServiceDeactive;

    return ota;
}
