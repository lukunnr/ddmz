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

#include <stdint.h>
#include <stdio.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_ota_ops.h"
#include "lightduer_types.h"
#include "lightduer_ota_unpack.h"
#include "lightduer_ota_updater.h"
#include "lightduer_ota_notifier.h"
#include "lightduer_ota_installer.h"

typedef struct _duer_ota_installer_handle_s {
    duer_ota_updater_t *updater;
    esp_ota_handle_t update_handle;
    const esp_partition_t *update_partition;
    unsigned int firmware_size;
    int firmware_offset;
} duer_ota_installer_handle_t;

static char* TAG="DUER_OTA";

static duer_package_info_t s_package_info = {
    .product = "ESP32 Demo",
    .batch   = "12",
    .os_info = {
        .os_name = "FreeRTOS",
        .developer = "Allen",
        .staged_version = "1.0.0.0",
    }
};

static int ota_notify_data_begin(void *ctx)
{
    esp_err_t err;
    int ret = DUER_OK;
    duer_ota_installer_handle_t *pdata = NULL;
    const esp_partition_t *update_partition = NULL;

    if (ctx == NULL) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    pdata = (duer_ota_installer_handle_t *)ctx;

    update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition != NULL) {
        ESP_LOGI(TAG, "OTA Unpack OPS: Writing to partition subtype %d at offset 0x%x",
             update_partition->subtype, update_partition->address);
    } else {
        ESP_LOGE(TAG, "OTA Unpack OPS: Get update partition failed");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    pdata->update_partition = update_partition;
    pdata->firmware_offset = 0;
out:
    return ret;
}

static int ota_notify_meta_data(void *cxt, duer_ota_package_meta_data_t *meta)
{
    esp_err_t err;
    int ret = DUER_OK;
    esp_ota_handle_t update_handle = 0 ;
    duer_ota_installer_handle_t *pdata = NULL;

    if (cxt == NULL || meta == NULL) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Argument Error");

        return DUER_ERR_INVALID_PARAMETER;
    }

    pdata = (duer_ota_installer_handle_t *)cxt;

    pdata->firmware_size = meta->install_info.module_list->module_size;

    err = esp_ota_begin(pdata->update_partition, pdata->firmware_size, &update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Init OTA failed, error = %d", err);

        ret = DUER_ERR_FAILED;

        goto out;
    }

    pdata->update_handle = update_handle;

#if 1
    ESP_LOGI(TAG, "OTA Unpack OPS: Basic info:");
    ESP_LOGI(TAG, "OTA Unpack OPS: Package name: %s", meta->basic_info.package_name);
    ESP_LOGI(TAG, "OTA Unpack OPS: Package type: %c", meta->basic_info.package_type);
    ESP_LOGI(TAG, "OTA Unpack OPS: Package update: %c", meta->basic_info.package_update);

    ESP_LOGI(TAG, "OTA Unpack OPS: Install info:");
    ESP_LOGI(TAG, "OTA Unpack OPS: Install path: %s", meta->install_info.package_install_path);
    ESP_LOGI(TAG, "OTA Unpack OPS: Module count: %d", meta->install_info.module_count);

    ESP_LOGI(TAG, "OTA Unpack OPS: Update info:");
    ESP_LOGI(TAG, "OTA Unpack OPS: Package version: %s", meta->update_info.package_version);

    ESP_LOGI(TAG, "OTA Unpack OPS: Extension info:");
    ESP_LOGI(TAG, "OTA Unpack OPS: Pair count: %d", meta->extension_info.pair_count);
#endif

out:
    return ret;
}

static int ota_notify_module_data(
        void *cxt,
        unsigned int offset,
        unsigned char *data,
        unsigned int size)
{
    esp_err_t err;
    int ret = DUER_OK;
    esp_ota_handle_t update_handle = 0 ;
    duer_ota_installer_handle_t *pdata = NULL;

    if (cxt == NULL) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_FAILED;

        goto out;
    }

    pdata = (duer_ota_installer_handle_t *)cxt;
    update_handle = pdata->update_handle;

    if (pdata->firmware_offset > pdata->firmware_size) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Buffer overflow");
        ESP_LOGE(TAG, "OTA Unpack OPS: buf offset: %d", pdata->firmware_offset);
        ESP_LOGE(TAG, "OTA Unpack OPS: firmware size: %d", pdata->firmware_size);

        ret = DUER_ERR_FAILED;

        goto out;
    }

    err = esp_ota_write(update_handle, (const void *)data, size);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Write OTA data failed! err: 0x%x", err);

        ret = DUER_ERR_FAILED;
    }

    pdata->firmware_offset += size;

out:
    return ret;
}

static int ota_notify_data_end(void *ctx)
{
    esp_err_t err;
    int ret = DUER_OK;
    esp_ota_handle_t update_handle = 0 ;
    const esp_partition_t *update_partition = NULL;
    duer_ota_installer_handle_t *pdata = NULL;

    if (ctx == NULL) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    pdata = (duer_ota_installer_handle_t *)ctx;
    update_handle = pdata->update_handle;
    update_partition = pdata->update_partition;

    err = esp_ota_end(update_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA Unpack OPS: End OTA failed! err: %d", err);

        ret = DUER_ERR_FAILED;

        goto out;
    }

    err = esp_ota_set_boot_partition(update_partition);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Set boot partition failed! err = 0x%x", err);

        ret = DUER_ERR_FAILED;

        goto out;
    }
out:
    return ret;
}

static int ota_update_img_begin(void *ctx)
{
    int ret = DUER_OK;

    ESP_LOGI(TAG, "OTA Unpack OPS: update image begin");

    return ret;

}

static int ota_update_img(void *ctx)
{
    int ret = DUER_OK;

    ESP_LOGI(TAG, "OTA Unpack OPS: updating image");

    return ret ;
}

static int ota_update_img_end(void *ctx)
{
    int ret = DUER_OK;

    ESP_LOGI(TAG, "OTA Unpack OPS: update image end");

    return ret;
}

static duer_ota_installer_t ota_installer = {
    .notify_data_begin  = ota_notify_data_begin,
    .notify_meta_data   = ota_notify_meta_data,
    .notify_module_data = ota_notify_module_data,
    .notify_data_end    = ota_notify_data_end,
    .update_img_begin   = ota_update_img_begin,
    .update_img         = ota_update_img,
    .update_img_end     = ota_update_img_end,
};

static int duer_ota_init_updater(duer_ota_updater_t *ota_updater)
{
    int ret = DUER_OK;
    duer_ota_installer_handle_t *ota_install_handle = NULL;

    ota_install_handle = (duer_ota_installer_handle_t *)malloc(sizeof(*ota_install_handle));
    if (ota_install_handle == NULL) {

        ESP_LOGE(TAG, "OTA Unpack OPS: Malloc failed");

        ret = DUER_ERR_MEMORY_OVERLOW;

        goto out;
    }

    ota_install_handle->updater = ota_updater;

    ret = duer_ota_unpack_register_installer(ota_updater->unpacker, &ota_installer);
    if (ret != DUER_OK) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Register installer failed ret:%d", ret);

        goto out;
    }

    ret = duer_ota_unpack_set_private_data(ota_updater->unpacker, ota_install_handle);
    if (ret != DUER_OK) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Set private data failed ret:%d", ret);
    }
out:
    return ret;
}

static int duer_ota_uninit_updater(duer_ota_updater_t *ota_updater)
{
    duer_ota_installer_handle_t *ota_install_handle = NULL;

    ota_install_handle = duer_ota_unpack_get_private_data(ota_updater->unpacker);
    if (ota_install_handle == NULL) {
        ESP_LOGE(TAG, "OTA Unpack OPS: Get private data failed");

        return DUER_ERR_INVALID_PARAMETER;
    }

    free(ota_install_handle);

    return DUER_OK;
}

static int reboot(void *arg)
{
    int ret = DUER_OK;

    ESP_LOGE(TAG, "OTA Unpack OPS: Prepare to restart system");

    esp_restart();

    return ret;
}

static int get_package_info(duer_package_info_t *info)
{
    int ret = DUER_OK;
    char firmware_version[FIRMWARE_VERSION_LEN + 1];

    if (info == NULL) {
        ESP_LOGE(TAG, "Argument Error");

        ret = DUER_ERR_INVALID_PARAMETER;

        goto out;
    }

    memset(firmware_version, 0, sizeof(firmware_version));

    ret = duer_get_firmware_version(firmware_version, sizeof(firmware_version));
    if (ret != DUER_OK) {
        ESP_LOGE(TAG, "Get firmware version failed");

        goto out;
    }

    strncpy((char *)&s_package_info.os_info.os_version,
            firmware_version,
            FIRMWARE_VERSION_LEN + 1);
    memcpy(info, &s_package_info, sizeof(*info));

out:
    return ret;
}

static duer_package_info_ops_t s_package_info_ops = {
    .get_package_info = get_package_info,
};

static duer_ota_init_ops_t s_ota_init_ops = {
    .init_updater = duer_ota_init_updater,
    .uninit_updater = duer_ota_uninit_updater,
    .reboot = reboot,
};

int duer_initialize_ota(void)
{
    int ret = DUER_OK;

    ret = duer_init_ota(&s_ota_init_ops);
    if (ret != DUER_OK) {
        ESP_LOGE(TAG, "Init OTA failed");
    }

    ret = duer_ota_register_package_info_ops(&s_package_info_ops);
    if (ret != DUER_OK) {
        ESP_LOGE(TAG, "Register OTA package info ops failed");
    }

    return ret;
}
