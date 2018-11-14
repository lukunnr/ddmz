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

#include "webserver.h"
#include "freertos/portmacro.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "string.h"
#include "cJSON.h"
#include "errno.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "tcpip_adapter.h"
#include "esp_err.h"
#include "esp_audio_log.h"
#include "httpd.h"
#include "httpd-platform.h"
#include "EspAudioAlloc.h"


#define WEBSERVER_TAG "WEBSERVER"

static xQueueHandle queWebCtrl;
#define ITEM_SIZE_QUE_WEB_CTRL   sizeof(MsgCtrlPkg)

int audio_httpd_cb(HttpdConnData *pValue);

HttpdBuiltInUrl builtInUrls[] = {
    {"/config", audio_httpd_cb, NULL},
    {NULL, NULL, NULL} //end marker
};

int audio_httpd_cb(HttpdConnData *pValue)
{
    cJSON *pJsonSub_status = NULL;
    int ret = 1;
    MsgCtrlPkg ctrlPkg;
    MsgUrlPkg  urlPkg;
    MsgListPkg listPkg;
    uint32_t recvLen = strlen(pValue->post->buff);
    char *inBuf = EspAudioAlloc(1, recvLen + 1);
    if (NULL == inBuf) {
        return -1;
    }
    strncpy(inBuf, pValue->post->buff, recvLen);
    memset(&ctrlPkg, 0, sizeof(ctrlPkg));
    memset(&urlPkg, 0, sizeof(urlPkg));
    memset(&listPkg, 0, sizeof(listPkg));

    cJSON *pJson = cJSON_Parse(inBuf);
    if (NULL != pJson) {
        pJsonSub_status = cJSON_GetObjectItem(pJson, "type");
    }
    if (NULL != pJsonSub_status) {
        switch (pJsonSub_status->valueint) {
        case AudioCtlType_Url:
            pJsonSub_status = cJSON_GetObjectItem(pJson, "live");
            if (NULL == pJsonSub_status) {
                ret = -1;
                break;
            }
            ESP_AUDIO_LOGD(WEBSERVER_TAG, "live=%d", pJsonSub_status->valueint);
            urlPkg.live = pJsonSub_status->valueint;

            pJsonSub_status = cJSON_GetObjectItem(pJson, "url");
            if (NULL == pJsonSub_status) {
                ret = -1;
                break;
            }
            ESP_AUDIO_LOGD(WEBSERVER_TAG, "url=%s", pJsonSub_status->valuestring);
            urlPkg.url = (char *)EspAudioAlloc(1, strlen(pJsonSub_status->valuestring) + 1);
            if(urlPkg.url == NULL){
                ESP_AUDIO_LOGE(WEBSERVER_TAG, "calloc failed(line %d)", __LINE__);
                ret = -1;
                break;;
            }
            strncpy(urlPkg.url, pJsonSub_status->valuestring, strlen(pJsonSub_status->valuestring));

            pJsonSub_status = cJSON_GetObjectItem(pJson, "name");
            if (NULL == pJsonSub_status) {
                free(urlPkg.url);
                ret  = -1;
                break;
            }
            ESP_AUDIO_LOGD(WEBSERVER_TAG, "name=%s", pJsonSub_status->valuestring);
            urlPkg.name = (char *)EspAudioAlloc(1, strlen(pJsonSub_status->valuestring) + 1);
            if(urlPkg.name == NULL){
                ESP_AUDIO_LOGE(WEBSERVER_TAG, "calloc failed(line %d)", __LINE__);
                free(urlPkg.url);
                ret = -1;
                break;;
            }
            strncpy(urlPkg.name, pJsonSub_status->valuestring, strlen(pJsonSub_status->valuestring));

            //// Queue send to ctrl task;
            urlPkg.type = AudioCtlType_Url;
            if (pdTRUE != xQueueSendToBack(queWebCtrl, &urlPkg, 0)) {
                free(urlPkg.url);
                free(urlPkg.name);
                ESP_AUDIO_LOGE(WEBSERVER_TAG, "AudioCtlType_Url queue send failed\n");
            } else {
                ESP_AUDIO_LOGD(WEBSERVER_TAG, "AudioCtlType_Url queue send success\n");
            }
            break;
        case AudioCtlType_Play:
            pJsonSub_status = cJSON_GetObjectItem(pJson, "play_status");
            if (NULL == pJsonSub_status) {
                ret  = -1;
                break;
            }
            if (1 == pJsonSub_status->valueint) {
                ESP_AUDIO_LOGD(WEBSERVER_TAG, "play_status->Start\n");
                ctrlPkg.type = AudioCtlType_Play;
                ctrlPkg.value = MediaPlayStatus_Start;


            } else {
                ctrlPkg.type = AudioCtlType_Play;
                ctrlPkg.value = MediaPlayStatus_Pause;
                ESP_AUDIO_LOGD(WEBSERVER_TAG, "play_status->Stop\n");
            }
            //// Queue send to ctrl task;
            if (pdTRUE != xQueueSendToBack(queWebCtrl, &ctrlPkg, 0)) {
                ESP_AUDIO_LOGE(WEBSERVER_TAG, "AudioCtlType_Play queue send failed\n");
            } else {
                ESP_AUDIO_LOGD(WEBSERVER_TAG, "AudioCtlType_Play queue send success\n");
            }

            break;
        case AudioCtlType_Vol:
            pJsonSub_status = cJSON_GetObjectItem(pJson, "volume");
            if (NULL == pJsonSub_status) {
                ret  = -1;
                break;
            }
            ESP_AUDIO_LOGD(WEBSERVER_TAG, "volume=%d\n", pJsonSub_status->valueint);
            ctrlPkg.type = AudioCtlType_Vol;
            ctrlPkg.value = pJsonSub_status->valueint;
            //// Queue send to ctrl task;
            if (pdTRUE != xQueueSendToBack(queWebCtrl, &ctrlPkg, 0)) {

                ESP_AUDIO_LOGE(WEBSERVER_TAG, "AudioCtlType_Vol queue send failed\n");
            } else {
                ESP_AUDIO_LOGD(WEBSERVER_TAG, "AudioCtlType_Vol queue send success\n");
            }
            break;
        case AudioCtlType_List:
            pJsonSub_status = cJSON_GetObjectItem(pJson, "live");
            if (NULL == pJsonSub_status) {
                ret = -1;
                break;
            }
            listPkg.type = AudioCtlType_List;
            listPkg.live = pJsonSub_status->valueint;
            char *buffer = EspAudioAlloc(1, pValue->post->buffLen);  //FIXME: too much waste??
            if(buffer == NULL){
                ESP_AUDIO_LOGE(WEBSERVER_TAG, "calloc failed(line %d)", __LINE__);
                ret = -1;
                break;
            }
            pJsonSub_status = cJSON_GetObjectItem(pJson, "push_data");
            if (NULL == pJsonSub_status) {
                free(buffer);
                ret  = -1;
                break;
            }
            if (pJsonSub_status->type == cJSON_Array) {
                listPkg.count = cJSON_GetArraySize(pJsonSub_status);
                ESP_AUDIO_LOGI(WEBSERVER_TAG, "List size = %d\n", listPkg.count);

                cJSON *node = pJsonSub_status->child;
                cJSON *temp;
                int ind = 0;
                while (node) {
                    temp = cJSON_GetObjectItem(node, "url");
                    if (temp) {
                        int len = strlen(temp->valuestring) + 1;
                        //ESP_AUDIO_LOGI(WEBSERVER_TAG, "%s", temp->valuestring);
                        strncpy(&buffer[ind], temp->valuestring, len);  //copy with the null terminator
                        ind += len;
                    }
                    node = node->next;
                }
            }
            listPkg.buf = buffer;
            if (pdTRUE != xQueueSendToBack(queWebCtrl, &listPkg, 0)) {  //free on receive
                free(buffer);
                ESP_AUDIO_LOGE(WEBSERVER_TAG, "AudioCtlType_Vol queue send failed\n");
            } else {
                ESP_AUDIO_LOGD(WEBSERVER_TAG, "AudioCtlType_Vol queue send success\n");
            }
            break;
        default:
            break;
        }
    }

    if (NULL != pJson)cJSON_Delete(pJson);
    free(inBuf);
    ESP_AUDIO_LOGD(WEBSERVER_TAG, "URL parse is ok\n");
    httpdStartResponse(pValue, 200);
    httpdEndHeaders(pValue);
    return HTTPD_CGI_DONE;

}

/******************************************************************************
 * FunctionName : user_webserver_start
 * Description  : start the web server task
 * Parameters   : noe
 * Returns      : none
*******************************************************************************/
void user_webserver_start(xQueueHandle queue)
{
    queWebCtrl = queue;
    httpdInit(80);
    httpdAddRouter(builtInUrls, NULL);
}

/******************************************************************************
 * FunctionName : user_webserver_stop
 * Description  : stop the task
 * Parameters   : void
 * Returns      : none
*******************************************************************************/
int8_t user_webserver_stop(void)
{
//  vQueueDelete(queWebCtrl);
//  queWebCtrl = NULL;
    // httpdPlatUninit();
    return pdPASS;
}
