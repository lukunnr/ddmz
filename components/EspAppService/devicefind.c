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
#include "freertos/queue.h"
#include "stdint.h"

#include "lwip/sockets.h"
#include "lwip/err.h"
#include "lwip/sys.h"

#include "lwip/netbuf.h"
#include "lwip/sys.h"
#include "lwip/ip_addr.h"
#include "tcpip_adapter.h"

#include "lwip/err.h"
#include "esp_err.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_audio_log.h"

#define DEVICE_TASK_PRIO  19
#define DEVICEFIND_TAG "DEVICE_FIND"

//#define DEBUG

#ifdef DEBUG
#define DF_DEBUG printf
#else
#define DF_DEBUG
#endif

static xQueueHandle queDeviceFindStop = NULL;

#define UDF_SERVER_PORT     1025

const char *device_find_request = "Are You Espressif IOT Smart Device?";
const char *device_find_response_ok = "I'm Soundbox.";


#define len_udp_msg 70
#define len_mac_msg 70
#define len_ip_rsp  50
static TaskHandle_t deviceFindHandle;
static int HighWaterLine = 0;
static wifi_mode_t  wifi_mode;
/*---------------------------------------------------------------------------*/
static int32_t sock_fd;

/******************************************************************************
 * FunctionName : user_devicefind_data_process
 * Description  : Processing the received data from the host
 * Parameters   : arg -- Additional argument to pass to the callback function
 *                pusrdata -- The received data (or NULL when the connection has been closed!)
 *                length -- The length of received data
 * Returns      : none
*******************************************************************************/
static void
user_devicefind_data_process(char *pusrdata, unsigned short length, struct sockaddr_in *premote_addr)
{
    char *DeviceBuffer;//40
    char *Device_mac_buffer;//60
    uint8_t hwaddr[6];
    int len;
    int err;
    tcpip_adapter_ip_info_t ipconfig;
    tcpip_adapter_ip_info_t ipconfig_r;

    if (pusrdata == NULL) {
        ESP_AUDIO_LOGE(DEVICEFIND_TAG, "pusrdata is NULL\r\n");
        return;
    }

    if (wifi_mode != WIFI_MODE_STA) {
        tcpip_adapter_ip_info_t info;
        memset(&info, 0x00, sizeof(tcpip_adapter_ip_info_t));
        tcpip_adapter_get_ip_info(WIFI_IF_AP, &info);
        esp_wifi_get_mac(WIFI_IF_AP, hwaddr);
        inet_addr_to_ipaddr(&ipconfig_r.ip, &premote_addr->sin_addr);
        if (!ip4_addr_netcmp(&ipconfig_r.ip, &ipconfig.ip, &ipconfig.netmask)) {
            ESP_AUDIO_LOGI(DEVICEFIND_TAG, "udpclient connect with sta\n");
            tcpip_adapter_get_ip_info(WIFI_IF_STA, &ipconfig);
            esp_wifi_get_mac(WIFI_IF_STA, hwaddr);
        }
    } else {
        tcpip_adapter_get_ip_info(WIFI_IF_STA, &ipconfig);
        esp_wifi_get_mac(WIFI_IF_STA, hwaddr);
    }
    DeviceBuffer = (char *)malloc(len_ip_rsp);
    memset(DeviceBuffer, 0, len_ip_rsp);
    int req_len = strlen(device_find_request);
    if (length == req_len &&
        strncmp(pusrdata, device_find_request, req_len) == 0) {
        sprintf(DeviceBuffer, "%s" MACSTR " " IPSTR, device_find_response_ok,
                MAC2STR(hwaddr), IP2STR(&ipconfig.ip));

        length = strlen(DeviceBuffer);
        sendto(sock_fd, DeviceBuffer, length, 0, (struct sockaddr *)premote_addr, sizeof(struct sockaddr_in));

    } else if (length == (req_len + 18)) {

        Device_mac_buffer = (char *)malloc(len_mac_msg);
        memset(Device_mac_buffer, 0, len_mac_msg);
        sprintf(Device_mac_buffer, "%s " MACSTR , device_find_request, MAC2STR(hwaddr));

        if (strncmp(Device_mac_buffer, pusrdata, req_len + 18) == 0) {

            sprintf(DeviceBuffer, "%s" MACSTR " " IPSTR, device_find_response_ok,
                    MAC2STR(hwaddr), IP2STR(&ipconfig.ip));

            length = strlen(DeviceBuffer);
            //DF_DEBUG("%s %d\n", DeviceBuffer, length);

            sendto(sock_fd, DeviceBuffer, length, 0, (struct sockaddr *)premote_addr, sizeof(struct sockaddr_in));
        }

        if (Device_mac_buffer)free(Device_mac_buffer);
    }

    if (DeviceBuffer)free(DeviceBuffer);
}

/******************************************************************************
 * FunctionName : user_devicefind_init
 * Description  : the espconn struct parame init
 * Parameters   : none
 * Returns      : none
*******************************************************************************/
static void
user_devicefind_task(void *pvParameters)
{
    struct sockaddr_in server_addr;
    int32_t ret;
    int err = 0;
    socklen_t len = sizeof(err);

    struct sockaddr_in from;
    socklen_t fromlen;
    tcpip_adapter_ip_info_t ipconfig;

    char *udp_msg = NULL;
    bool ValueFromReceive = false;
    portBASE_TYPE xStatus;

    int nNetTimeout = 1000; // 1 Sec

    memset(&ipconfig, 0, sizeof(ipconfig));
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(UDF_SERVER_PORT);
    server_addr.sin_len = sizeof(server_addr);


    if (udp_msg)
        free(udp_msg);
    if (sock_fd > 0)
        close(sock_fd);

    udp_msg = (char *)malloc(len_udp_msg);
    do {
        sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
        if (sock_fd == -1) {
            ESP_AUDIO_LOGE(DEVICEFIND_TAG, "ERROR:devicefind failed to create sock!\n");
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } while (sock_fd == -1);

    struct timeval tv_out;
    tv_out.tv_sec = 10; //Wait 10 seconds.
    tv_out.tv_usec = 0;
    setsockopt(sock_fd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));
    do {
        ret = bind(sock_fd, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (ret != 0) {
            ESP_AUDIO_LOGE(DEVICEFIND_TAG, "ERROR:devicefind failed to bind sock!ret=%d\n", ret);
            vTaskDelay(1000 / portTICK_RATE_MS);
        }
    } while (ret != 0);

    ESP_AUDIO_LOGI(DEVICEFIND_TAG, "user_devicefind_task is running\r\n");
    while (1) {

        xStatus = xQueueReceive(queDeviceFindStop, &ValueFromReceive, 0);
        if ((pdPASS == xStatus) && (true == ValueFromReceive)) {
            ESP_AUDIO_LOGI(DEVICEFIND_TAG, "user_devicefind_task rcv exit signal!\n");
            break;
        }

        memset(udp_msg, 0, len_udp_msg);
        memset(&from, 0, sizeof(from));

        fromlen = sizeof(struct sockaddr_in);
        ret = recvfrom(sock_fd, (uint8_t *)udp_msg, len_udp_msg, 0, (struct sockaddr *)&from, (socklen_t *)&fromlen);
        if (ret > 0) {
            ESP_AUDIO_LOGI(DEVICEFIND_TAG, "recvfrom->port %d  %s\n", ntohs(from.sin_port), inet_ntoa(from.sin_addr));
            user_devicefind_data_process(udp_msg, ret, &from);
        } else if (ret == 0){
            vTaskDelay(50 / portTICK_RATE_MS);
        } else {
            ret = getsockopt(sock_fd, SOL_SOCKET, SO_ERROR, &err, &len);
            if (err == EAGAIN){
                vTaskDelay(1000 / portTICK_RATE_MS);
            } else if (err == EINTR){
                vTaskDelay(5 / portTICK_RATE_MS);
            } else {
                ESP_AUDIO_LOGE(DEVICEFIND_TAG, "recvfrom ret=%d,err = %d id %d, %s %d", ret, err, sock_fd, strerror(errno), errno);
                vTaskDelay(1000 / portTICK_RATE_MS);
            }
        }
#if EN_STACK_TRACKER
        if(uxTaskGetStackHighWaterMark(deviceFindHandle) > HighWaterLine){
            HighWaterLine = uxTaskGetStackHighWaterMark(deviceFindHandle);
            ESP_AUDIO_LOGI("STACK", "%s %d\n\n\n", __func__, HighWaterLine);
        }
#endif
    }

    if (udp_msg)free(udp_msg);

    close(sock_fd);
    sock_fd = 0;

    vQueueDelete(queDeviceFindStop);
    queDeviceFindStop = NULL;
    deviceFindHandle = NULL;

    ESP_AUDIO_LOGI(DEVICEFIND_TAG, "user_devicefind_task is exited");
    vTaskDelete(NULL);

}
void user_devicefind_start(void)
{

    int err = esp_wifi_get_mode(&wifi_mode);//Remove to other place
    if (err != ESP_OK) {
        ESP_AUDIO_LOGI(DEVICEFIND_TAG, "esp_wifi_get_mode fail, ret=%d\n", err);
    }

    if (queDeviceFindStop == NULL) {
        queDeviceFindStop = xQueueCreate(1, 1);
    }
    if (deviceFindHandle == NULL) {
        xTaskCreate(user_devicefind_task, "user_devicefind_task", 2048 + 512, NULL, DEVICE_TASK_PRIO, &deviceFindHandle);
    }else{
        ESP_AUDIO_LOGI(DEVICEFIND_TAG, "user_devicefind_start already started!\n");
    }
    ESP_AUDIO_LOGI(DEVICEFIND_TAG, "user_devicefind_start\n");
}

int8_t user_devicefind_stop(void)
{
    bool ValueToSend = true;
    portBASE_TYPE xStatus;
    ESP_AUDIO_LOGI(DEVICEFIND_TAG, "Stop the device find task");
    if (queDeviceFindStop == NULL) {
        ESP_AUDIO_LOGI(DEVICEFIND_TAG, "queDeviceFindStop has been NULL!");
        return pdFAIL;
    }
    xStatus = xQueueSend(queDeviceFindStop, &ValueToSend, 0);
    if (xStatus != pdPASS) {
        ESP_AUDIO_LOGI(DEVICEFIND_TAG, "Could not send to the queue!");
        return pdFAIL;
    } else {
        taskYIELD();
    }
    return pdPASS;
}

