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
#include "lwip/sockets.h"
#include "mesh_ctrl.h"

static const char *TAG              = "MESH_CTRL";

const char *light_control_head =
    "POST /device_request HTTP/1.1\r\n"
    "Mesh-Node-Num: 5\r\n"
    "Mesh-Node-Mac: 240ac48b96e0,240ac48b9d64,240ac48b9e60,240ac48b9528,240ac48b9834\r\n"
    "Content-Type: application/json\r\n"
    "root-response: false\r\n"
    "Connection: close\r\n"
    "Content-Length: %d\r\n\r\n";

const char *light_control_body = "{\"request\":\"set_status\",\"characteristics\":[{\"cid\":1,\"value\":%d},{\"cid\":2,\"value\":%d},{\"cid\":3,\"value\":%d},{\"cid\":0,\"value\":%d}]}";


#define MESH_UDP_BROADCAST_PORT (1025)
#define MESH_HTTP_PORT          (80)

#if 0
"white" - "0.0000,0.0000,1.0000" - "0,0,100"
"yellow" - "0.1667,1.0000,1.0000" - "60,100,100"
"red" - "0.0000,1.0000,1.0000" - "0,100,100"
"Purple" - "0.7692,0.8667,0.9412" - "277,87,94"
"Green" - "0.3333,1.0000,1.0000" - "120,100,100"
"Blue" - "0.6667,1.0000,1.0000" - "240,100,100"
"Pink" -  "0.9709,0.2471,1.0000" - "349,25,100"
#endif

static  xQueueHandle mesh_ctrl_event = NULL;
static  TaskHandle_t mesh_foud_handle = NULL;

static uint8_t light_hvs[][3] = {
    {0,     0,      100}, // WHITE
    {60,    100,    100}, // YELLOW
    {0,     100,    100}, // RED
    {277,   87,     94},  // PURPLE
    {120,   100,    100}, // GREEN
    {240,   100,    100}, // BLUE
    {349,   25,     100}, // PINK
};

int mesh_udp_client_create(void)
{
    esp_err_t ret = ESP_OK;
    int opt_val = 1;
    int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        ESP_LOGE(TAG, "socket create:%s", strerror(errno));
        goto UDP_ERR_EXIT;
    }
    struct timeval tv_out;
    tv_out.tv_sec = 1; //Wait 1 seconds.
    tv_out.tv_usec = 0;
    setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &tv_out, sizeof(tv_out));

    ret = setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &opt_val, sizeof(int));
    if (ret < 0) {
        ESP_LOGE(TAG, "socket setsockopt:%s", strerror(errno));
        goto UDP_ERR_EXIT;
    }
    ret = setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(int));
    if (ret < 0) {
        ESP_LOGE(TAG, "socket setsockopt:%s", strerror(errno));
        goto UDP_ERR_EXIT;
    }
    ESP_LOGD(TAG, "create udp multicast, sockfd: %d", sockfd);
    return sockfd;

UDP_ERR_EXIT:

    if (sockfd != -1) {
        close(sockfd);
    }

    return -1;
}

static int mesh_tcp_client_create(const char *ip, uint16_t port)
{
    esp_err_t ret = ESP_OK;
    int sockfd = -1;
    struct sockaddr_in server_addr = {
        .sin_family = AF_INET,
        .sin_port = htons(port),
        .sin_addr.s_addr = inet_addr(ip),
    };

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        ESP_LOGE(TAG, "socket create: sockfd: %d", sockfd);
        goto TCP_ERR_EXIT;
    }
    ret = connect(sockfd, (struct sockaddr *)&server_addr, sizeof(struct sockaddr_in));
    if (ret < 0) {
        ESP_LOGE(TAG, "socket connect, ret: %d, ip: %s, port: %d", ret, ip, port);
        goto TCP_ERR_EXIT;
    }

    ESP_LOGI(TAG, "connect tcp server ip: %s, port: %d, sockfd: %d", ip, port, sockfd);

    return sockfd;
TCP_ERR_EXIT:

    if (sockfd != -1) {
        close(sockfd);
    }
    return -1;
}

static void mesh_ctrl_task(void *arg)
{
    struct sockaddr_in *dest_ip = (struct sockaddr_in *)arg;
    char *body = calloc(1, 256);
    char *http_msg = calloc(1, 1024);
    if (body == NULL) {
        ESP_LOGE(TAG, "mesh_ctrl_task[%d], calloc failed", __LINE__);
        goto mesh_task_err;
    }
    if (http_msg == NULL) {
        ESP_LOGE(TAG, "mesh_ctrl_task[%d], calloc failed", __LINE__);
        goto mesh_task_err;
    }
    int task_run = 1;
    mesh_ctrl_msg_t msg = {0};
    uint8_t light_sw = CTRL_LIGHT_OFF;
    uint16_t light_c_h = light_hvs[COLOR_LIGHT_WHITE][0];
    uint8_t light_c_s = light_hvs[COLOR_LIGHT_WHITE][1];
    uint8_t light_c_v = light_hvs[COLOR_LIGHT_WHITE][2];

    ESP_LOGI(TAG, "mesh_ctrl_task running... port: %d, ip: %s",
             ntohs(dest_ip->sin_port), inet_ntoa(dest_ip->sin_addr));
    mesh_ctrl_event =  xQueueCreate(3, sizeof(mesh_ctrl_msg_t));
    if (mesh_ctrl_event == NULL) {
        ESP_LOGE(TAG, "mesh_ctrl_task[%d], xQueueCreate failed", __LINE__);
        goto mesh_task_err;
    }
    while (task_run) {
        if (xQueueReceive(mesh_ctrl_event, &msg, portMAX_DELAY) == pdPASS) {
            if (msg.cmd == MESH_CTRL_EVT_QUIT) {
                task_run = 0;
                continue;
            } else {
                ESP_LOGE(TAG, "Mesh control command is %d", msg.cmd);
                int sok = mesh_tcp_client_create(inet_ntoa(dest_ip->sin_addr), MESH_HTTP_PORT);
                if (sok == -1) {
                    ESP_LOGE(TAG, "Create socket to connect mesh failed");
                    continue;
                }
                if (msg.cmd == MESH_CTRL_EVT_COLOR) {
                    light_c_h = light_hvs[msg.data][0];
                    light_c_s = light_hvs[msg.data][1];
                    light_c_v = light_hvs[msg.data][2];
                    light_sw = CTRL_LIGHT_ON;
                }
                if (msg.cmd == MESH_CTRL_EVT_SWITCH) {
                    light_sw = msg.data;
                }
                sprintf(body, light_control_body, light_c_h, light_c_s, light_c_v, light_sw);
                ESP_LOGI(TAG, "Mesh control body length:%d, %s", strlen(body), body);

                sprintf(http_msg, light_control_head, strlen(body));
                strcat(http_msg, body);
                int len =  strlen(http_msg);
                ESP_LOGI(TAG, "control_msg_len: %d, control_msg:\n%s", len, http_msg);
                send(sok, http_msg, len, 0);
                vTaskDelay(100 / portTICK_RATE_MS);
                recv(sok, http_msg, 1024, 0);
                vTaskDelay(500 / portTICK_RATE_MS);
                close(sok);
                memset(body, 0, 256);
                memset(body, 0, 1024);
                sok = -1;
            }
        }
    }

mesh_task_err:
    if (body) {
        free(body);
    }
    if (http_msg) {
        free(http_msg);
    }
    vTaskDelete(NULL);
}

static void mesh_found(void *arg)
{
    esp_err_t ret = ESP_OK;
    int udp_client = -1;
    const char *discover_str = "Are You Espressif IOT Smart Device?";
    struct sockaddr_in broadcast_addr = {
        .sin_family      = AF_INET,
        .sin_port        = htons(MESH_UDP_BROADCAST_PORT),
        .sin_addr.s_addr = htonl(INADDR_BROADCAST),
    };
    char recv_buf[64] = {0};
    struct sockaddr_in from_addr = {0};
    socklen_t from_addr_len = sizeof(struct sockaddr_in);
    udp_client = mesh_udp_client_create();
    if (udp_client == NULL) {
        vTaskDelete(NULL);
        return;
    }
    while (1) {
        /**< send and recv udp data */
        ESP_LOGD(TAG, "sendto %s, ret: %d", discover_str, ret);
        for (int i = 0; i < 100; i++) {
            ret = sendto(udp_client, discover_str, strlen(discover_str), 0,
                         (struct sockaddr *)&broadcast_addr, sizeof(struct sockaddr));
            vTaskDelay(200 / portTICK_RATE_MS);
            ESP_LOGD(TAG, "send times %d", i);
            ret = recvfrom(udp_client, recv_buf, sizeof(recv_buf),
                           0, (struct sockaddr *)&from_addr, (socklen_t *)&from_addr_len);
            if (ret <= 0) {
                continue;
            } else {
                ESP_LOGW(TAG, "Mesh found timeout i=%d", i);
                break;
            }
        }
        ESP_LOGD(TAG, "udp recvfrom, sockfd: %d, port: %d, ip: %s, recv_buf: %s",
                 udp_client, ntohs(((struct sockaddr_in *)&from_addr)->sin_port),
                 inet_ntoa(((struct sockaddr_in *)&from_addr)->sin_addr), recv_buf);
        struct sockaddr_in *sock_addr = malloc(sizeof(struct sockaddr_in));
        memcpy(sock_addr, &from_addr, sizeof(struct sockaddr_in));
        xTaskCreate(mesh_ctrl_task, "mesh_ctrl_task", (1024 * 2), sock_addr, 3, NULL);
        break;
    }
    mesh_foud_handle = NULL;
    vTaskDelete(NULL);
}

esp_err_t mesh_light_ctrl_init()
{
    if (mesh_foud_handle == NULL) {
        if (xTaskCreate(mesh_found, "mesh_found",  (1024 * 2 + 512), NULL, 2, &mesh_foud_handle)) {
            return ESP_OK;
        }
    }
    return ESP_FAIL;
}

esp_err_t mesh_light_color_set(color_light_t color)
{
    if (mesh_ctrl_event == NULL) {
        ESP_LOGE(TAG, "mesh_ctrl_event is NULL, line:%d", __LINE__);
        return ESP_FAIL;
    }
    mesh_ctrl_msg_t msg = {MESH_CTRL_EVT_COLOR, color};
    if (xQueueSend(mesh_ctrl_event, &msg, 100 / portTICK_RATE_MS) != pdPASS) {
        ESP_LOGE(TAG, "mesh_light_color_set queue failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t mesh_light_switch(ctrl_light_t ctrl)
{
    if (mesh_ctrl_event == NULL) {
        ESP_LOGE(TAG, "mesh_ctrl_event is NULL, line:%d", __LINE__);
        return ESP_FAIL;
    }
    mesh_ctrl_msg_t msg = {MESH_CTRL_EVT_SWITCH, ctrl};
    if (xQueueSend(mesh_ctrl_event, &msg, 100 / portTICK_RATE_MS) != pdPASS) {
        ESP_LOGE(TAG, "mesh_light_switch queue failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}

esp_err_t mesh_light_ctrl_deinit()
{
    if (mesh_ctrl_event == NULL) {
        ESP_LOGE(TAG, "mesh_ctrl_event is NULL, line:%d", __LINE__);
        return ESP_FAIL;
    }
    mesh_ctrl_msg_t msg = {MESH_CTRL_EVT_QUIT, 0};
    if (xQueueSend(mesh_ctrl_event, &msg, 100 / portTICK_RATE_MS) != pdPASS) {
        ESP_LOGE(TAG, "MESH_CTRL_EVT_QUIT queue failed");
        return ESP_FAIL;
    }
    return ESP_OK;
}
