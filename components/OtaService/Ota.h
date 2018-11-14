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

#ifndef _OTA_AUDIO_H_
#define _OTA_AUDIO_H_

#define EXAMPLE_WIFI_SSID ""
#define EXAMPLE_WIFI_PASS ""
#define EXAMPLE_SERVER_IP   "192.168.0.200"
#define EXAMPLE_SERVER_PORT "8080"
#define BUFFSIZE 1024
#define TEXT_BUFFSIZE 1024
#include "esp_ota_ops.h"
#include "esp_partition.h"


typedef struct {
    esp_partition_t wifi_partition;
    esp_partition_t bt_partition;
    uint8_t wifi_flag;  /*1 : if wifi ota exist*/
    uint8_t bt_flag;    /*1 : if bt ota exist*/
} audio_partition;

typedef struct {
    size_t wifi_image_size;
    size_t bt_image_size;
} audio_size;

typedef struct {
    esp_ota_handle_t out_wifi_handle;
    esp_ota_handle_t out_bt_handle;
} audio_handle;

/*switch between wifi bin and bt bin*/
void esp_ota_audio_switch_wifi_bt();
esp_err_t esp_ota_audio_begin(audio_partition partition, audio_size image_size, audio_handle *out_handle);
esp_err_t esp_ota_audio_write(audio_handle handle, const void *data, size_t size, int in_ota_select);
esp_err_t esp_ota_audio_end(audio_handle handle);
esp_err_t esp_ota_audio_set_boot_partition(audio_partition operate_partition);
esp_err_t OtaServiceStart(void);
esp_err_t SwitchBootPartition(void);
esp_err_t QueryBootPartition(void);

#endif