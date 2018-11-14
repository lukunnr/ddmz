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

#include "duer_profile_config.h"
#include <stdio.h>
#include <stdlib.h>
#include "EspAudioAlloc.h"
#include "esp_partition.h"
#include "esp_log.h"

#define DUER_CONF_KEY_UUID       "uuid"
#define DUER_CONF_KEY_TOKEN      "token"
#define DUER_CONF_KEY_BINDTOKEN  "bindToken"
#define DUER_CONF_KEY_HOST       "serverAddr"
#define DUER_CONF_KEY_PORT_REG   "lwm2mPort"
#define DUER_CONF_KEY_PORT_RPT   "coapPort"
#define DUER_CONF_KEY_CERT       "rsaCaCrt"

#define PARTITION_PROFILE_MAGIC_NUM 0xA55A5AA5

// #define ENABLE_EMBED_PROFILE

static char *TAG = "DUER_PROFILE";
const char *parti_label = "profile";

static esp_err_t duer_profile_check(char *profile)
{
    int len = strlen(profile);
    profile_conf_t pconf = {0};
    ESP_LOGD(TAG, "%s\n", profile);
    pconf.conf = duer_conf_create(profile, len);
    if (!pconf.conf) {
        ESP_LOGE(TAG, "Profile: duer_conf_create failed!");
        return ESP_FAIL;
    }

    pconf.uuid = duer_conf_get_string(pconf.conf, DUER_CONF_KEY_UUID);
    if (!pconf.uuid) {
        ESP_LOGE(TAG, "Profile: obtain uuid failed!");
        duer_conf_destroy(pconf.conf);
        return ESP_FAIL;
    }
    ESP_LOGI(TAG, "DuerOS profile uuid: %s", pconf.uuid);
    duer_conf_destroy(pconf.conf);
    return ESP_OK;
}

static const char *load_profile_by_partition()
{
    char *data = NULL;
    unsigned char len_buf[4] = {0};
    int *data_len = NULL;
    int offset = 0;
    int ret = -1;
    const esp_partition_t *parti = NULL;
    do {
        parti = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY,
                                         parti_label);
        if (parti == NULL) {
            ESP_LOGE(TAG, "cannot find partition lable %s.", parti_label);
            break;
        }
        ret = esp_partition_read(parti, offset, len_buf, sizeof(len_buf));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "read partition failed, offset=%d.", offset);
            break;
        }
        data_len = &len_buf;
        if (PARTITION_PROFILE_MAGIC_NUM != *data_len) {
            ESP_LOGE(TAG, "partition profile magic number invalid,%x", *data_len);
            break;
        }
        offset += sizeof(len_buf);
        ret = esp_partition_read(parti, offset, len_buf, sizeof(len_buf));
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "read partition failed, offset=%d.", offset);
            break;
        }
        ESP_LOGI(TAG, "profile len=%d ", *data_len);

        // NOTE: PSRAM can't be used as it also use SPI same with flash
        data = (char *)malloc(*data_len + 1);
        if (data == NULL) {
            ESP_LOGE(TAG, "Alloc the profile data failed with size = %d", *data_len);
            break;
        }
        offset += sizeof(len_buf);
        ret = esp_partition_read(parti, offset, data, *data_len);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "read partition failed, offset=%d.", offset);
            free(data);
            data = NULL;
            break;
        }
        data[*data_len] = '\0';
        if (*data_len != strlen(data)) {
            ESP_LOGE(TAG, "length of partition profile invalid");
            free(data);
            data = NULL;
            break;
        }
    } while (0);

    return data;
}

static const char *load_profile_by_file(const char *path)
{
    char *data = NULL;
    FILE *file = NULL;

    do {
        int rs;
        file = fopen(path, "rb");
        if (file == NULL) {
            ESP_LOGE(TAG, "Failed to open file: %s", path);
            break;
        }
        rs = fseek(file, 0, SEEK_END);
        if (rs != 0) {
            ESP_LOGE(TAG, "Seek to file tail failed, rs = %d", rs);
            break;
        }
        long size = ftell(file);
        if (size < 0) {
            ESP_LOGE(TAG, "Seek to file tail failed, rs = %d", rs);
            break;
        }
        rs = fseek(file, 0, SEEK_SET);
        if (rs != 0) {
            ESP_LOGE(TAG, "Seek to file head failed, rs = %d", rs);
            break;
        }
        data = (char *)DUER_MALLOC(size + 1);
        if (data == NULL) {
            ESP_LOGE(TAG, "Alloc the profile data failed with size = %ld", size);
            break;
        }
        rs = fread(data, 1, size, file);
        if (rs < 0) {
            ESP_LOGE(TAG, "Read file failed, rs = %d", rs);
            free(data);
            data = NULL;
            break;
        }
        data[size] = '\0';
    } while (0);

    if (file) {
        fclose(file);
        file = NULL;
    }
    return data;
}

#ifdef ENABLE_EMBED_PROFILE
extern const uint8_t _duer_profile_start[] asm("_binary_duer_profile_start");
extern const uint8_t _duer_profile_end[]   asm("_binary_duer_profile_end");
static const char *load_profile_by_binary()
{
    int sz = _duer_profile_end - _duer_profile_start;
    char *data = EspAudioAllocInner(1, sz);
    if (NULL == data) {
        ESP_LOGE(TAG, "audio_malloc failed");
    }
    memcpy(data, _duer_profile_start, sz);
    return data;
}
#endif

const char *duer_load_profile(const char *path)
{
    char *profile = NULL;
    profile = load_profile_by_partition();
    if (profile && duer_profile_check(profile) == ESP_OK) {
        ESP_LOGI(TAG, "Read dueros profile from partition successfully");
        return profile;
    }
    profile = load_profile_by_file("/sdcard/profile");
    if (profile && duer_profile_check(profile) == ESP_OK) {
        ESP_LOGI(TAG, "Read dueros profile from sdcard file successfully");
        return profile;
    }
#ifdef ENABLE_EMBED_PROFILE
    profile = load_profile_by_binary();
    if (profile && duer_profile_check(profile) == ESP_OK) {
        ESP_LOGI(TAG, "Read dueros profile from binary successfully");
        return profile;
    }
#endif
    ESP_LOGE(TAG, "There is no valid dueros profile");
    return NULL;
}