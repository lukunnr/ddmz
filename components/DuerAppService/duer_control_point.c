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
#include "esp_log.h"
#include "esp_err.h"
#include "lightduer_timestamp.h"
#include "lightduer_connagent.h"
#include "mesh_ctrl.h"

static const char *TAG              = "DUEROS_CTRL";

esp_err_t execute_ctrl_light(int index)
{
    if (index == 0) {
        ESP_LOGI(TAG, "Voice Control: Turn OFF the light");
        mesh_light_switch(CTRL_LIGHT_OFF);
    } else if (index == 1) {
        ESP_LOGI(TAG, "Voice Control: Turn ON the light");
        mesh_light_switch(CTRL_LIGHT_ON);
    } else {
        ESP_LOGE(TAG, "Execute_ctrl_light: Invalid voice control command");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t execute_switch_mode(int index)
{
    if (index == 0) {
        ESP_LOGI(TAG, "Voice Control Music mode");
    } else if (index == 1) {
        ESP_LOGI(TAG, "Voice Control Free Chat mode");
    } else if (index == 2) {
        ESP_LOGI(TAG, "Voice Control WeChat mode");
    } else {
        ESP_LOGE(TAG, "Execute_switch_mode: Invalid voice control command");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t execute_setting_light_color(int index)
{
    ESP_LOGI(TAG, "execute_setting_light_color, %d", index);
    switch (index) {
        case 0:
            ESP_LOGI(TAG, "Voice Control: Setting Color RED");
            mesh_light_color_set(COLOR_LIGHT_RED);
            break;
        case 1:
            ESP_LOGI(TAG, "Voice Control: Setting Color White");
            mesh_light_color_set(COLOR_LIGHT_WHITE);
            break;
        case 2:
            ESP_LOGI(TAG, "Voice Control: Setting Color Purple");
            mesh_light_color_set(COLOR_LIGHT_PURPLE);
            break;
        case 3:
            ESP_LOGI(TAG, "Voice Control: Setting Color Pink");
            mesh_light_color_set(COLOR_LIGHT_PINK);
            break;
        case 4:
            ESP_LOGI(TAG, "Voice Control: Setting Color Blue");
            mesh_light_color_set(COLOR_LIGHT_BLUE);
            break;
        case 5:
            ESP_LOGI(TAG, "Voice Control: Setting Color Yellow");
            mesh_light_color_set(COLOR_LIGHT_YELLOW);
            break;
        case 6:
            ESP_LOGI(TAG, "Voice Control: Setting Color Green");
            mesh_light_color_set(COLOR_LIGHT_GREEN);
            break;
        default:
            ESP_LOGE(TAG, "Execute_setting_light_color: Invalid voice control command");
            return ESP_FAIL;
    }
    return ESP_OK;
}

static duer_status_t voice_ctrl_point_cb(duer_context ctx, duer_msg_t *msg, duer_addr_t *addr)
{
    ESP_LOGE(TAG, "duer_app_test_control_point");

    if (msg) {
        duer_response(msg, DUER_MSG_RSP_INVALID, NULL, 0);
    } else {
        return DUER_ERR_FAILED;
    }
    if ((msg->payload == NULL) || (msg->payload_len == 0)) {
        ESP_LOGE(TAG, "duer_msg_t failed, func:%s, len:%d", msg->payload == NULL ? "NULL" : (char *)msg->payload, msg->payload_len);
        return DUER_ERR_FAILED;
    }
    baidu_json *result = baidu_json_Parse((char *)msg->payload);
    if (result == NULL) {
        ESP_LOGE(TAG, "Failed to parse payload, func:%s, %d", __func__, __LINE__);
        return DUER_ERR_FAILED;
    }

    baidu_json *value = baidu_json_GetObjectItem(result, "value");
    if (value == NULL) {
        ESP_LOGE(TAG, "Failed to parse value, func:%s, %d", __func__, __LINE__);
        return DUER_ERR_FAILED;
    }
    int index = 0;
    if (sscanf(value->valuestring, "%u", &index) == 0) {
        ESP_LOGE(TAG, "Failed to parse valuestring, func:%s, %d", __func__, __LINE__);
        return DUER_ERR_FAILED;
    }

    // Add name of identification
    if (msg->path && (strncmp((char *)msg->path, "Music_Mode", strlen("Music_Mode")) == 0)) {
        execute_switch_mode(index);
    }
    if (msg->path && (strncmp((char *)msg->path, "Light_Color", strlen("Light_Color")) == 0)) {
        execute_setting_light_color(index);
    }
    if (msg->path && (strncmp((char *)msg->path, "Control_Light", strlen("Control_Light")) == 0)) {
        execute_ctrl_light(index);
    }

    duer_u8_t *cachedToken = malloc(msg->token_len);
    memcpy(cachedToken, msg->token, msg->token_len);
    baidu_json *payload = baidu_json_CreateObject();
    baidu_json_AddStringToObject(payload, "result", "OK");
    baidu_json_AddNumberToObject(payload, "timestamp", duer_timestamp());

    duer_seperate_response((char *)cachedToken, msg->token_len, DUER_MSG_RSP_CONTENT, payload);

    free(cachedToken);
    return DUER_OK;
}

void duer_control_point_init()
{
    duer_res_t res[] = {
        {
            DUER_RES_MODE_DYNAMIC,
            DUER_RES_OP_PUT,
            "Control_Light",
            .res.f_res = voice_ctrl_point_cb
        },
        {
            DUER_RES_MODE_DYNAMIC,
            DUER_RES_OP_PUT,
            "Music_Mode",
            .res.f_res = voice_ctrl_point_cb
        },
        {
            DUER_RES_MODE_DYNAMIC,
            DUER_RES_OP_PUT,
            "Light_Color",
            .res.f_res = voice_ctrl_point_cb
        },

    };

    duer_add_resources(res, sizeof(res) / sizeof(res[0]));
    // mesh_light_ctrl_init();
}

void duer_control_point_deinit()
{
    mesh_light_ctrl_deinit();
}
