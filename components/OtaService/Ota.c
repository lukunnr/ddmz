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
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_audio_log.h"

#include "nvs_flash.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

#include "Ota.h"

#define OTA_TAG  "OTA_HANDLE"
/*an ota data write buffer ready to write to the flash*/
static char ota_write_data[BUFFSIZE + 1] = { 0 };
/*an packet receive buffer*/
static char text[BUFFSIZE + 1] = { 0 };
/* an image total length*/
static int binary_file_length = 0;
/*socket id*/
static int socket_id = -1;
static char http_request[128] = {0};

/* operate handle : uninitialized value is zero ,every ota begin would exponential growth*/
static audio_handle out_handle;

static audio_partition operate_partition;

int read_until(char *buffer, char delim, int len);
bool resolve_pkg(char text[], int total_len, audio_handle out_handle, int in_ota_select);
bool connect_to_http_server();
bool ota_init();
bool ota_data_from_server(int in_ota_select);

/*read buffer by byte still delim ,return read bytes counts*/
int read_until(char *buffer, char delim, int len)
{
//  /*TODO: delim check,buffer check,further: do an buffer length limited*/
    int i = 0;
    while (buffer[i] != delim && i < len) {
        ++i;
    }
    return i + 1;
}

/* resolve a packet from http socket
 * return true if packet including \r\n\r\n that means http packet header finished,start to receive packet body
 * otherwise return false
 * */
bool resolve_pkg(char text[], int total_len, audio_handle out_handle, int in_ota_select)
{
    /* i means current position */
    int i = 0, i_read_len = 0;
    while (text[i] != 0 && i < total_len) {
        i_read_len = read_until(&text[i], '\n', total_len);
        // if we resolve \r\n line,we think packet header is finished
        if (i_read_len == 2) {
            int i_write_len = total_len - (i + 2);
            memset(ota_write_data, 0, BUFFSIZE);
            /*copy first http packet body to write buffer*/
            memcpy(ota_write_data, &(text[i + 2]), i_write_len);
            /*check write packet header first byte:0xE9 second byte:0x08 */
            if (ota_write_data[0] == 0xE9 && i_write_len >= 2) {
                ESP_AUDIO_LOGI(OTA_TAG, "Info: Write Header format Check OK! first byte is %02x ,second byte is %02x", ota_write_data[0], ota_write_data[1]);
            } else {
                ESP_AUDIO_LOGE(OTA_TAG, "Error: Write Header format Check Failed! first byte is %02x ,second byte is %02x", ota_write_data[0], ota_write_data[1]);
                return false;
            }

            if (esp_ota_audio_write( out_handle, (const void *)ota_write_data, i_write_len, in_ota_select) != ESP_OK) {
                ESP_AUDIO_LOGE(OTA_TAG, "Error: esp_ota_write first body failed!");
                return false;
            } else {
                ESP_AUDIO_LOGI(OTA_TAG, "Info: esp_ota_write header OK!");
                binary_file_length += i_write_len;
            }
            return true;
        }
        i += i_read_len;
    }
    return false;
}

bool connect_to_http_server()
{
    ESP_AUDIO_LOGI(OTA_TAG, "Server IP: %s Server Port:%s", EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
    int  connect_id = -1;
    struct sockaddr_in sock_info;
    socket_id = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_id == -1) {
        ESP_AUDIO_LOGE(OTA_TAG, "Error: create socket failed!");
        return false;
    } else {
        ESP_AUDIO_LOGI(OTA_TAG, "Info: create socket success!");
    }

    // set connect info
    memset(&sock_info, 0, sizeof(struct sockaddr_in));
    sock_info.sin_family = AF_INET;

    inet_aton(EXAMPLE_SERVER_IP, &sock_info.sin_addr);
    sock_info.sin_port = htons(atoi(EXAMPLE_SERVER_PORT));
    ESP_AUDIO_LOGI(OTA_TAG, "\r\nconnecting to server IP:%s,Port:%d...\r\n",
           ipaddr_ntoa((const ip_addr_t*)&sock_info.sin_addr.s_addr), sock_info.sin_port);
    // connect to http server
    connect_id = connect(socket_id, (struct sockaddr *)&sock_info, sizeof(sock_info));
    if (connect_id == -1) {
        ESP_AUDIO_LOGE(OTA_TAG, "Error: connect to server failed!");
        return false;
    } else {
        ESP_AUDIO_LOGI(OTA_TAG, "Info: connect to server success!");
        return true;
    }
    return false;
}

// /*switch between wifi bin and bt bin*/
// void esp_ota_audio_switch_wifi_bt()
// {
//     const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
//     if (esp_current_partition->type != ESP_PARTITION_TYPE_APP) {
//         ESP_AUDIO_LOGE(OTA_TAG, "Error esp_current_partition->type != ESP_PARTITION_TYPE_APP");
//         return exit_task();
//     }

//     esp_partition_t switch_partition;
//     switch (esp_current_partition->subtype) {
//     case ESP_PARTITION_SUBTYPE_APP_OTA_0:
//         switch_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
//         break;
//     case  ESP_PARTITION_SUBTYPE_APP_OTA_1:
//         switch_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
//         break;
//     case ESP_PARTITION_SUBTYPE_APP_OTA_2:
//         switch_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_3;
//         break;
//     case ESP_PARTITION_SUBTYPE_APP_OTA_3:
//         switch_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_2;
//         break;
//     default:
//         return;
//     }
//     switch_partition.type = ESP_PARTITION_TYPE_APP;
//     const esp_partition_t *find_partition = esp_partition_find_first(switch_partition.type, switch_partition.subtype, NULL);
//     assert(find_partition);
//     if (esp_ota_set_boot_partition(find_partition) != ESP_OK) {
//         ESP_AUDIO_LOGE(OTA_TAG, "Error: set boot wifi  failed");
//         return exit_task();
//     }
//     system_restart();
// }

esp_err_t esp_ota_audio_begin(audio_partition oper_partition, audio_size image_size, audio_handle *out_handle)
{
    if (oper_partition.wifi_flag) {
        if (esp_ota_begin(&(oper_partition.wifi_partition), 0x0, &(out_handle->out_wifi_handle)) != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: ota begin wifi failed");
            return ESP_FAIL;
        }
    }
    if (oper_partition.bt_flag) {
        if (esp_ota_begin(&(oper_partition.bt_partition), 0x0, &(out_handle->out_bt_handle)) != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: ota begin bt failed");
            return ESP_FAIL;
        }
    }
    return ESP_OK;
}

esp_err_t esp_ota_audio_write(audio_handle handle, const void *data, size_t size, int in_ota_select)
{
    switch (in_ota_select) {
    case 0:
        if (esp_ota_write(handle.out_wifi_handle, data, size) != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: write wifi handle failed");
            return ESP_FAIL;
        }
        break;
    case 1:
        if (esp_ota_write(handle.out_bt_handle, data, size) != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: write bt handle failed");
            return ESP_FAIL;
        }
        break;
    default:
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t esp_ota_audio_end(audio_handle handle)
{
    if (operate_partition.wifi_flag) {
        if (esp_ota_end(handle.out_wifi_handle) != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: end wifi handle failed");
            return ESP_FAIL;
        }
    }

    if (operate_partition.bt_flag) {
        if (esp_ota_end(handle.out_bt_handle) != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: end bt handle failed");
            return ESP_FAIL;
        }
    }

    return ESP_OK;
}

esp_err_t esp_ota_audio_set_boot_partition(audio_partition oper_partition)
{
    if (oper_partition.wifi_flag == 1) {
        if (esp_ota_set_boot_partition(&(oper_partition.wifi_partition)) != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: set boot wifi  failed");
            return -1;
        }
    } else if (oper_partition.bt_flag == 1) {
        if (esp_ota_set_boot_partition(&(oper_partition.bt_partition))  != ESP_OK) {
            ESP_AUDIO_LOGE(OTA_TAG, "Error: set boot bt  failed");
            return -1;
        }
    } else {
        return -1;
    }
    return ESP_OK;
}

bool ota_init(void)
{
    const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
    if ((esp_current_partition == NULL) || (esp_current_partition->type != ESP_PARTITION_TYPE_APP)) {
        ESP_AUDIO_LOGE(OTA_TAG, "Error esp_current_partition->type != ESP_PARTITION_TYPE_APP");
        return false;
    }
    ESP_AUDIO_LOGI("esp_current_partition:", "current type[%x]", esp_current_partition->type);
    ESP_AUDIO_LOGI("esp_current_partition:", "current subtype[%x]", esp_current_partition->subtype);
    ESP_AUDIO_LOGI("esp_current_partition:", "current address:0x%x", esp_current_partition->address);
    ESP_AUDIO_LOGI("esp_current_partition:", "current size:0x%x", esp_current_partition->size);
    ESP_AUDIO_LOGI("esp_current_partition:", "current labe:%s", esp_current_partition->label);

    esp_partition_t find_wifi_partition;
    esp_partition_t find_bt_partition;
    memset(&operate_partition, 0, sizeof(audio_partition));
    memset(&find_wifi_partition, 0, sizeof(esp_partition_t));
    memset(&find_bt_partition, 0, sizeof(esp_partition_t));
    /*choose which OTA_TAG audio image should we write to*/
    switch (esp_current_partition->subtype) {
    case ESP_PARTITION_SUBTYPE_APP_OTA_0:
        find_wifi_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_2;
        find_bt_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_3;
        break;
    case  ESP_PARTITION_SUBTYPE_APP_OTA_1:
        find_wifi_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_2;
        find_bt_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_3;
        break;
    case ESP_PARTITION_SUBTYPE_APP_OTA_2:
        find_wifi_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        find_bt_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        break;
    case ESP_PARTITION_SUBTYPE_APP_OTA_3:
        find_wifi_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        find_bt_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        break;
    default:
        break;
    }
    find_wifi_partition.type = ESP_PARTITION_TYPE_APP;
    find_bt_partition.type = ESP_PARTITION_TYPE_APP;

    const esp_partition_t *wifi_partition = NULL;
    wifi_partition = esp_partition_find_first(find_wifi_partition.type, find_wifi_partition.subtype, NULL);
    const esp_partition_t *bt_partition = NULL;
    bt_partition = esp_partition_find_first(find_bt_partition.type, find_bt_partition.subtype, NULL);
    assert(wifi_partition != NULL && bt_partition != NULL);
    memcpy(&(operate_partition.wifi_partition), wifi_partition, sizeof(esp_partition_t));
    memcpy(&(operate_partition.bt_partition), bt_partition, sizeof(esp_partition_t));
    /*actual : we would not assign size for it would assign by default*/
    ESP_AUDIO_LOGI("wifi_partition", "%d: type[%x]", __LINE__, wifi_partition->type);
    ESP_AUDIO_LOGI("wifi_partition", "%d: subtype[%x]", __LINE__, wifi_partition->subtype);
    ESP_AUDIO_LOGI("wifi_partition", "%d: address:0x%x", __LINE__, wifi_partition->address);
    ESP_AUDIO_LOGI("wifi_partition", "%d: size:0x%x", __LINE__, wifi_partition->size);
    ESP_AUDIO_LOGI("wifi_partition", "%d: labe:%s", __LINE__,  wifi_partition->label);

    ESP_AUDIO_LOGI("bt_partition", "%d: type[%x]", __LINE__, bt_partition->type);
    ESP_AUDIO_LOGI("bt_partition", "%d: subtype[%x]", __LINE__, bt_partition->subtype);
    ESP_AUDIO_LOGI("bt_partition", "%d: address:0x%x", __LINE__, bt_partition->address);
    ESP_AUDIO_LOGI("bt_partition", "%d: size:0x%x", __LINE__, bt_partition->size);
    ESP_AUDIO_LOGI("bt_partition", "%d: labe:%s", __LINE__,  bt_partition->label);
    audio_size image_size;
    if (esp_ota_audio_begin(operate_partition, image_size, &out_handle) != ESP_OK) {
        ESP_AUDIO_LOGE(OTA_TAG, "Error: esp_ota_begin failed!");
        return false;
    } else {
        ESP_AUDIO_LOGI(OTA_TAG, "Info: esp_ota_begin init OK!");
        return true;
    }
    return false;
}

bool ota_data_from_server(int in_ota_select)
{
    /*connect to http server*/
    if (connect_to_http_server()) {
        ESP_AUDIO_LOGI(OTA_TAG, "Info: connect to http server success!");
    } else {
        ESP_AUDIO_LOGE(OTA_TAG, "Error: connect to http server failed!");
        return false;
    }

    int res = -1;
    /*send GET request to http server*/
    switch (in_ota_select) {
    case 0:
        sprintf(http_request, "GET /ota_audio_wifi.bin HTTP/1.1\r\nHost: %s:%s\r\n\r\n", EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
        ESP_AUDIO_LOGI(OTA_TAG, "-------------- prepare to download wifi bin ------------");
        break;
    case 1:
        sprintf(http_request, "GET /ota_audio_bt.bin HTTP/1.1\r\nHost: %s:%s\r\n\r\n", EXAMPLE_SERVER_IP, EXAMPLE_SERVER_PORT);
        ESP_AUDIO_LOGI(OTA_TAG, "-------------- prepare to download bt bin ------------");
        break;
    default:
        return false;
    }

    res = send(socket_id, http_request, strlen(http_request), 0);
    if (res == -1) {
        ESP_AUDIO_LOGE(OTA_TAG, "Error: send GET request to server failed");
        return false;
    } else {
        ESP_AUDIO_LOGI(OTA_TAG, "Info: send GET request to server success!");
    }
    bool pkg_body_start = false, flag = true;
    /*deal with all receive packet*/
    while (flag) {
        memset(text, 0, TEXT_BUFFSIZE);
        memset(ota_write_data, 0, BUFFSIZE);
        int buff_len = recv(socket_id, text, TEXT_BUFFSIZE, 0);
        if (buff_len < 0) { /*receive error*/
            ESP_AUDIO_LOGE(OTA_TAG, "Error: receive data error!");
            close(socket_id);
            return false;
        } else if (buff_len > 0 && !pkg_body_start) { /*deal with packet header*/
            memcpy(ota_write_data, text, buff_len);
            pkg_body_start = resolve_pkg(text, buff_len, out_handle, in_ota_select);
        } else if (buff_len > 0 && pkg_body_start) { /*deal with packet body*/
            memcpy(ota_write_data, text, buff_len);
            if (esp_ota_audio_write( out_handle, (const void *)ota_write_data, buff_len, in_ota_select) != ESP_OK) {
                ESP_AUDIO_LOGE(OTA_TAG, "Error: esp_ota_write audio failed!");
                close(socket_id);
                return false;
            }
            binary_file_length += buff_len;
            ESP_AUDIO_LOGI(OTA_TAG, "Info: had written image length %d", binary_file_length);
        } else if (buff_len == 0) {  /*packet over*/
            flag = false;
            close(socket_id);
            ESP_AUDIO_LOGI(OTA_TAG, "Info: receive all packet over!");
        } else {
            ESP_AUDIO_LOGI(OTA_TAG, "Warning: uncontolled event!");
            close(socket_id);
            return false;
        }
    }
    return true;
}

esp_err_t OtaServiceStart(void)
{
    esp_err_t ret = ESP_FAIL;
    memset(&operate_partition, 0, sizeof(audio_partition));
    /*select which ota or both bt and wifi*/
    // TODO, Need to check which bin will be update;
    operate_partition.bt_flag = 1;
    operate_partition.wifi_flag = 1;

    /*begin ota*/
    if ( ota_init() ) {
        ESP_AUDIO_LOGI(OTA_TAG, "Info: OTA Init success!");
    } else {
        ESP_AUDIO_LOGE(OTA_TAG, "Error: OTA Init failed");
        return ret;
    }
    /*if wifi ota exist,would ota it*/
    if (operate_partition.wifi_flag) {
        binary_file_length = 0;
        if (!ota_data_from_server(0)) {
            return ESP_FAIL;
        }
        ESP_AUDIO_LOGI(OTA_TAG, "Info: Total Write wifi binary data length : %d", binary_file_length);
    }
    /*if bt ota exist,would ota it*/
    if (operate_partition.bt_flag) {
        binary_file_length = 0;
        if (!ota_data_from_server(1)) {
            return ESP_FAIL;
        }
        ESP_AUDIO_LOGI(OTA_TAG, "Info: Total Write bt binary data length : %d", binary_file_length);
    }
    /*ota end*/
    if (esp_ota_audio_end(out_handle) != ESP_OK) {
        ESP_AUDIO_LOGE(OTA_TAG, "Error : esp_ota_end failed!");
        return ret;
    }
    /*set boot*/
    if (esp_ota_audio_set_boot_partition(operate_partition) != ESP_OK) {
        ESP_AUDIO_LOGE(OTA_TAG, "Error : esp_ota_set_boot_partition failed!");
        return ret;
    }
    ESP_AUDIO_LOGI(OTA_TAG, "Prepare to restart system!");
    return ESP_OK;
}

esp_err_t QueryBootPartition(void)
{
    const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
    if ((esp_current_partition == NULL) || (esp_current_partition->type != ESP_PARTITION_TYPE_APP)) {
        ESP_AUDIO_LOGE(OTA_TAG, "func:%s,line:%d,Error type != ESP_PARTITION_TYPE_APP", __func__, __LINE__);
        return ESP_FAIL;
    }
    ESP_AUDIO_LOGI("booting_partition:", "subtype[%x]", esp_current_partition->subtype);
    ESP_AUDIO_LOGI("booting_partition:", "address:0x%x", esp_current_partition->address);
    ESP_AUDIO_LOGI("booting_partition:", "size:0x%x", esp_current_partition->size);
    ESP_AUDIO_LOGI("booting_partition:", "labe:%s", esp_current_partition->label);
    return ESP_OK;
}

esp_err_t SwitchBootPartition(void)
{
    const esp_partition_t *esp_current_partition = esp_ota_get_boot_partition();
    if ((esp_current_partition == NULL) || (esp_current_partition->type != ESP_PARTITION_TYPE_APP)) {
        ESP_AUDIO_LOGE(OTA_TAG, "func:%s,line:%d,Error esp_current_partition->type != ESP_PARTITION_TYPE_APP", __func__, __LINE__);
        return ESP_FAIL;
    }
    ESP_AUDIO_LOGI("switch_boot_partition:", "current type[%x]", esp_current_partition->type);
    ESP_AUDIO_LOGI("switch_boot_partition:", "current subtype[%x]", esp_current_partition->subtype);
    ESP_AUDIO_LOGI("switch_boot_partition:", "current address:0x%x", esp_current_partition->address);
    ESP_AUDIO_LOGI("switch_boot_partition:", "current size:0x%x", esp_current_partition->size);
    ESP_AUDIO_LOGI("switch_boot_partition:", "current labe:%s", esp_current_partition->label);

    esp_partition_t find_partition;
    memset(&find_partition, 0, sizeof(find_partition));
    /*choose which OTA_TAG audio image should we write to*/
    switch (esp_current_partition->subtype) {
    case ESP_PARTITION_SUBTYPE_APP_OTA_0:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        break;
    case  ESP_PARTITION_SUBTYPE_APP_OTA_1:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    case ESP_PARTITION_SUBTYPE_APP_OTA_2:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_3;
        break;
    case ESP_PARTITION_SUBTYPE_APP_OTA_3:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_2;
        break;
    default:
        find_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        break;
    }
    find_partition.type = ESP_PARTITION_TYPE_APP;
    const esp_partition_t *wifi_partition = esp_partition_find_first(find_partition.type, find_partition.subtype, NULL);

    assert(wifi_partition != NULL);
    esp_partition_t next_partition;
    memcpy(&next_partition, wifi_partition, sizeof(next_partition));
//    next_partition.address = 0x10000;
//    next_partition.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
//    next_partition.address = 0x10000;
//    strncpy(next_partition.label, "ota_0", strlen("ota_0"));
    ESP_AUDIO_LOGI("switch_boot_partition:", "Next address:0x%x", next_partition.address);
    ESP_AUDIO_LOGI("switch_boot_partition:", "Next size:0x%x", next_partition.size);
    ESP_AUDIO_LOGI("switch_boot_partition:", "Next labe:%s", next_partition.label);
    if (esp_ota_set_boot_partition(&next_partition)  != ESP_OK) {
        ESP_AUDIO_LOGE(OTA_TAG, "func:%s,line:%d,Error:set boot failed", __func__, __LINE__);
        return ESP_FAIL;
    }

    ESP_AUDIO_LOGI(OTA_TAG, "func:%s,line:%d,Boot part switch ok [%d]", __func__, __LINE__, find_partition.subtype);
    return ESP_OK;
}
