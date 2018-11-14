/* HTTP GET Example using plain POSIX sockets

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event_loop.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"
#include "EspAudio.h"

#include "soc/rtc.h"



#include "dd_http.h"
#include "esp_http_client.h"
#include "mbedtls/base64.h"
#include "mbedtls/md5.h"
#include "cJSON.h"
#include "utf8.h"

/* FreeRTOS event group to signal when we are connected & ready to make a request */
static EventGroupHandle_t wifi_event_group;
static DdHttpService     *localDdHttpService;

#define DDHTTP_SERVICE_TASK_PRIORITY     5
#define DDHTTP_SERVICE_TASK_SIZE         16*1024


/* The event group allows multiple bits for each event,
   but we only care about one event - are we connected
   to the AP with an IP? */
const int CONNECTED_BIT = BIT0;

/* Constants that aren't configurable in menuconfig */
#define WEB_SERVER "www.baidu.com"
#define WEB_PORT 80
#define WEB_URL "http://www.baidu.com/"

static const char *TAG = "httpc";

#define xf_appid 		"5b976677"
#define xf_asr_apikey 	"1f61390c3ffb48f9bb7b021c81070b69"
#define xf_tts_apikey	"0df05848029ed4e9fd3db36f485850df"

typedef enum
{
	tts_text_state_start,
	tts_text_state_run,
	tts_text_state_finish,
	tts_text_state_err
}_tts_text_state;

static char *tts_text_json=NULL;
static char *tts_text_iat_txt =NULL;
static char *tts_text_nlp_txt =NULL;
static _tts_text_state tts_text_state=tts_text_state_start;
static int 	tts_text_json_offset=0;
static esp_http_client_handle_t cur_tts_client=NULL;

extern void ddOSEvtSend(void *que, int type, void *data, int index, int len, int dir);


void tts_text_start()
{
	if(NULL == tts_text_json)
	{
		tts_text_json=malloc(64*1024);
		tts_text_iat_txt =malloc(256);
		tts_text_nlp_txt =malloc(2*1024);
	}
	tts_text_state=tts_text_state_start;
	memset(tts_text_json,0,64*1024);
	tts_text_json_offset=0;
	memset(tts_text_iat_txt,0,256);
	memset(tts_text_nlp_txt,0,2*1024);
}

void tts_text_push(char *info,int size)
{
	memcpy(tts_text_json+tts_text_json_offset,info,size);
	tts_text_json_offset+=size;
	tts_text_state=tts_text_state_run;
}

void tts_text_finish()
{
	int iatflag=0,nlpflag=0;
	tts_text_state=tts_text_state_finish;
	printf("\n\n%s\n\n",tts_text_json);

	cJSON *root = cJSON_Parse(tts_text_json);
	if(NULL==root)		
	{
		printf("json root analysis error\n");
		goto err;
	}


	cJSON *node_data = cJSON_GetObjectItem(root,"data");
	if(NULL==node_data)		
	{
		printf("json data analysis error\n");
		goto err;
	}

	int i,arrsize = cJSON_GetArraySize(node_data);

	//printf("arrsize=%d\n",arrsize);
	
	for(i=0;i<arrsize;i++)
	{
		cJSON *tmpchecknode = cJSON_GetArrayItem(node_data,i);

		//printf("\n%s\n",cJSON_Print(tmpchecknode));
		
		if(!iatflag && cJSON_GetObjectItem(tmpchecknode,"sub") && 
			!strcmp(cJSON_GetObjectItem(tmpchecknode,"sub")->valuestring,"iat")	&&
			cJSON_GetObjectItem(tmpchecknode,"text"))
		{
			snprintf(tts_text_iat_txt,256,"%s",cJSON_GetObjectItem(tmpchecknode,"text")->valuestring);
			iatflag=1;
		}

		if(!nlpflag && cJSON_GetObjectItem(tmpchecknode,"sub") && 
			!strcmp(cJSON_GetObjectItem(tmpchecknode,"sub")->valuestring,"nlp")	&&
			cJSON_GetObjectItem(tmpchecknode,"intent"))
		{
			cJSON *intentp = cJSON_GetObjectItem(tmpchecknode,"intent");
			cJSON *answerp = cJSON_GetObjectItem(intentp,"answer");
			if(answerp && cJSON_GetObjectItem(answerp,"text")&&(strlen(cJSON_GetObjectItem(answerp,"text")->valuestring)>0))
			{
				snprintf(tts_text_nlp_txt,2*1024,"%s",cJSON_GetObjectItem(answerp,"text")->valuestring);
				nlpflag=1;
			}
		}

		if(iatflag&&nlpflag)
		{
			if(root)	cJSON_free(root);
			printf(">>>>>>>iat:  [%s]\n",tts_text_iat_txt);
			printf(">>>>>>>nlp:  [%s]\n",tts_text_nlp_txt);
			ddOSEvtSend(localDdHttpService->que, DD_CMD_TTS_EVT, NULL, 0, 0, 0);
			return;
		}
	}

err:
	if(root)	cJSON_free(root);
	tts_text_state=tts_text_state_err;
	ddOSEvtSend(localDdHttpService->que, DD_CMD_TTS_EVT, NULL, 0, 0, 0);
}



esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            printf("HTTP_EVENT_ERROR\n");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            printf("HTTP_EVENT_ON_CONNECTED\n");
            break;
        case HTTP_EVENT_HEADER_SENT:
            printf("HTTP_EVENT_HEADER_SENT\n");
            break;
        case HTTP_EVENT_ON_HEADER:
            printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
            if (!esp_http_client_is_chunked_response(evt->client)) {
                // Write out data
                //printf("\n\n=============================================================================\n");
                //printf("%.*s", evt->data_len, (char*)evt->data);
				//printf("\n=============================================================================\n\n");
				tts_text_push((char*)evt->data,evt->data_len);
            }

            break;
        case HTTP_EVENT_ON_FINISH:
            printf( "HTTP_EVENT_ON_FINISH\n");
			tts_text_finish();
            break;
        case HTTP_EVENT_DISCONNECTED:
            printf("HTTP_EVENT_DISCONNECTED\n");
            break;
    }
    return ESP_OK;
}


static int tts_respon_audio_flag = 0;
esp_err_t _http_event_handler_tts(esp_http_client_event_t *evt)
{
	static FILE *file = NULL;
	static int filecnt=0;
	char filename[64]={0};

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGD(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            ESP_LOGD(TAG, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
			printf("\n\n===========================  HEAD  ==========================================\n");
            printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s\n", evt->header_key, evt->header_value);
			printf("\n=============================================================================\n\n");

			if(strstr(evt->header_key,"Content-Type")!=NULL)
			{
				if(strstr(evt->header_value,"audio")!=NULL)
				{
					localDdHttpService->Based.rawStop(localDdHttpService,TERMINATION_TYPE_NOW);
					tts_respon_audio_flag=1;
					//"raw://%d:%d@from.pcm/to.%s#i2s", 16000[sampleRate], 2[channel], "mp3"[music type] 
					localDdHttpService->Based.rawStart(localDdHttpService,"raw://from.mp3/to.mp3#i2s,16000,1,mp3");
					//EspAudioRawStart("raw://from.mp3/to.mp3#i2s,16000,1,mp3");
					//EspAudioSetDecoder("MP3");

					sprintf(filename,"/sdcard/xftts_%d.mp3",filecnt++);
					file = fopen(filename, "w");
					if (NULL == file) {
						printf("open %s failed,[%d]\n",filename, __LINE__);
						break;
					}
					
				}else
				if(strstr(evt->header_value,"text")!=NULL)
				{
					tts_respon_audio_flag=0;
				}
			}

			
            break;
        case HTTP_EVENT_ON_DATA:
            ESP_LOGD(TAG, "HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
			if(tts_respon_audio_flag==0)
			{
	            if (!esp_http_client_is_chunked_response(evt->client)) {
	                // Write out data
	                printf("\n\n===========================  BODY  ==========================================\n");
	                printf("%.*s", evt->data_len, (char*)evt->data);
					printf("\n=============================================================================\n\n");
	            }
			}else
	        {
	            if (!esp_http_client_is_chunked_response(evt->client)) {
	                // Write out data
	                int ret,write_len=0;
					ret=localDdHttpService->Based.rawWrite(localDdHttpService,evt->data,evt->data_len,&write_len);
					fwrite(evt->data, 1, evt->data_len, file);
					//ret=EspAudioRawWrite(evt->data,evt->data_len,&write_len);
					//printf("ret=%d,get size=%d, write size=%d\n",ret,evt->data_len,write_len);
	            }
			}
            break;
        case HTTP_EVENT_ON_FINISH:

			fclose(file);
			file=NULL;
		
            ESP_LOGD(TAG, "HTTP_EVENT_ON_FINISH");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGD(TAG, "HTTP_EVENT_DISCONNECTED");
            break;
    }
    return ESP_OK;
}



int URLEncode(const char* str, const int strSize, char* result, const int resultSize)
{  
    int i;  
    int j = 0;//for result index  
    char ch;  
  
    if ((str==NULL) || (result==NULL) || (strSize<=0) || (resultSize<=0)) {  
        return 0;  
    }  
  
    for ( i=0; (i<strSize)&&(j<resultSize); ++i) {  
        ch = str[i];  
        if (((ch>='A') && (ch<'Z')) ||  
            ((ch>='a') && (ch<'z')) ||  
            ((ch>='0') && (ch<'9'))) {  
            result[j++] = ch;  
        } else if (ch == ' ') {  
            result[j++] = '+';  
        } else if (ch == '.' || ch == '-' || ch == '_' || ch == '*') {  
            result[j++] = ch;  
        } else {  
            if (j+3 < resultSize) {  
                sprintf(result+j, "%%%02X", (unsigned char)ch);  
                j += 3;  
            } else {  
                return 0;  
            }  
        }  
    }  
  
    result[j] = '\0';  
    return j;  
}  


void ddcloud_http_xunfei_tts_stop()
{
	if(cur_tts_client)	esp_http_client_cleanup(cur_tts_client);
}


void ddcloud_http_xunfei_tts(char *buffer,int size)
{
	esp_http_client_config_t config = {
			.url = "http://api.xfyun.cn/v1/service/v1/tts",
			.event_handler = _http_event_handler_tts,
		};
			
		size_t base64size=0;
		char *headbase64Str=malloc(512);
		char *md5checkstr=malloc(512);
		char *timestr=malloc(16);
		char *md5charStr=malloc(33);
		unsigned char md5char[16];
		time_t now =0;
		
		memset(headbase64Str,0,512);
		memset(md5checkstr,0,512);
		memset(timestr,0,16);
		memset(md5charStr,0,33);
/*
	json def

		{
		    "auf": "audio/L16;rate=16000",
		    "aue": "lame",
		    "voice_name": "xiaoyan",
		    "speed": "50",
		    "volume": "50",
		    "pitch": "50",
		    "engine_type": "intp65",
		    "text_type": "text"
		}
*/
	snprintf(headbase64Str,512,"ew0KICAgICJhdWYiOiAiYXVkaW8vTDE2O3JhdGU9MTYwMDAiLA0KICAgICJhdWUiOiAibGFtZSIsDQogICAgInZvaWNlX25hbWUiOiAieGlhb3lhbiIsDQogICAgInNwZWVkIjogIjUwIiwNCiAgICAidm9sdW1lIjogIjUwIiwNCiAgICAicGl0Y2giOiAiNTAiLA0KICAgICJlbmdpbmVfdHlwZSI6ICJpbnRwNjUiLA0KICAgICJ0ZXh0X3R5cGUiOiAidGV4dCINCn0=");

	time(&now);
	sprintf(timestr,"%ld",now);
	//printf("====== timestr=%s\n",timestr);
	//printf("json_param=[%s]\n",json_param);
	sprintf(md5checkstr,"%s%ld%s",xf_tts_apikey,now,headbase64Str);
	mbedtls_md5((unsigned char*)md5checkstr,strlen(md5checkstr),md5char);

	int i=0;
	for(i=0;i<16;i++)
	{
		sprintf((md5charStr+i*2),"%02x",md5char[i]);
	}


	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_url(client, "http://api.xfyun.cn/v1/service/v1/tts");
	esp_http_client_set_header(client, "Content-Type", "application/x-www-form-urlencoded; charset=utf-8");

	esp_http_client_set_header(client,"X-Appid",xf_appid);
	esp_http_client_set_header(client,"X-CurTime",timestr);
	esp_http_client_set_header(client,"X-Param",(char *)headbase64Str);
	esp_http_client_set_header(client,"X-CheckSum",(char *)md5charStr);
	
    esp_http_client_set_method(client, HTTP_METHOD_POST);
/*
// ----- utf8 convert -----------
	printf("============= UTF8 ...................\n");
	int illegal = 0;
	size_t max = size + 1;
	ucs4_t *us = calloc(max, sizeof(ucs4_t));
	size_t n = u8decode(buffer, us, max, &illegal);

    for (i = 0; i < n; ++i)
    {
        printf("%#010x", us[i]);
        if (isuchiness(us[i]))
            printf("    chiness");
        printf("\n");
    }
	printf("============= UTF8 ...................\n");
*/
	char *urlbuff=malloc(1000);
	memset(urlbuff,0,1000);
	sprintf(urlbuff,"text=");
	URLEncode(buffer,size,urlbuff+5,1000-5);
    esp_http_client_set_post_field(client, urlbuff, strlen(urlbuff)+1);

	printf("urlbuff=[%s]\n",urlbuff);

	cur_tts_client = client;
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "HTTP POST request failed: %d", err);
    }

	free(headbase64Str);
	esp_http_client_cleanup(client);
	cur_tts_client = NULL;
}



void ddcloud_http_data_post(char *buffer,int size)
{
	esp_http_client_config_t config = {
			.url = "http://openapi.xfyun.cn/v2/aiui",
			.event_handler = _http_event_handler,
		};

	size_t base64size=0;
	char *headbase64Str=malloc(512);
	char *md5checkstr=malloc(512);
	char *timestr=malloc(16);
	char *md5charStr=malloc(33);
	unsigned char md5char[16];
	time_t now =0;
	tts_text_start();
	memset(headbase64Str,0,512);
	memset(md5checkstr,0,512);
	memset(timestr,0,16);
	memset(md5charStr,0,33);
	//mbedtls_base64_encode(headbase64Str,512,&base64size,json_param,strlen((char *)json_param)+1);
/*
	json def
	
	{"scene":"main","aue":"raw","sample_rate":"16000","pers_param":"{\"auth_id\":\"6649a9975b8245ccacc8f062097a6577\"}","data_type":"audio","auth_id":"6649a9975b8245ccacc8f062097a6577"}
*/
	snprintf(headbase64Str,512,"eyJzY2VuZSI6Im1haW4iLCJhdWUiOiJyYXciLCJzYW1wbGVfcmF0ZSI6IjE2MDAwIiwicGVyc19wYXJhbSI6IntcImF1dGhfaWRcIjpcIjY2NDlhOTk3NWI4MjQ1Y2NhY2M4ZjA2MjA5N2E2NTc3XCJ9IiwiZGF0YV90eXBlIjoiYXVkaW8iLCJhdXRoX2lkIjoiNjY0OWE5OTc1YjgyNDVjY2FjYzhmMDYyMDk3YTY1NzcifQ==");
	time(&now);
	sprintf(timestr,"%ld",now);
	//printf("====== timestr=%s\n",timestr);
	//printf("json_param=[%s]\n",json_param);
	sprintf(md5checkstr,"%s%ld%s",xf_asr_apikey,now,headbase64Str);
	mbedtls_md5((unsigned char*)md5checkstr,strlen(md5checkstr),md5char);
	
	int i=0;
	for(i=0;i<16;i++)
	{
		sprintf((md5charStr+i*2),"%02x",md5char[i]);
	}
	
	//printf("MD5 [%s] \n>\n [%s]\n\n",md5checkstr,md5charStr);
	esp_http_client_handle_t client = esp_http_client_init(&config);
    esp_http_client_set_url(client, "http://openapi.xfyun.cn/v2/aiui");
	esp_http_client_set_header(client,"X-Appid",xf_appid);
	esp_http_client_set_header(client,"X-CurTime",timestr);
	esp_http_client_set_header(client,"X-Param",(char *)headbase64Str);
	esp_http_client_set_header(client,"X-CheckSum",(char *)md5charStr);

    esp_http_client_set_method(client, HTTP_METHOD_POST);
    esp_http_client_set_post_field(client, buffer, size);
    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %d",
                esp_http_client_get_status_code(client),
                esp_http_client_get_content_length(client));
    } else {
		//err process
        ESP_LOGE(TAG, "HTTP POST request failed: %d", err);
    }

	free(headbase64Str);
	free(md5checkstr);
	free(timestr);
	free(md5charStr);
	esp_http_client_cleanup(client);
}

static void DeviceEvtNotifiedToHttp(DeviceNotification* note)
{
    DdHttpService* service = (DdHttpService*) note->receiver;

    if (DEVICE_NOTIFY_TYPE_WIFI == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg*) note->data);
        switch (msg) {
            case DEVICE_NOTIFY_WIFI_GOT_IP:
                break;

            case DEVICE_NOTIFY_WIFI_DISCONNECTED:
                break;
            default:
                break;
        }
    }
}


static void ddhttp_task(void *pvParameters)
{
    DdHttpService *service =  (DdHttpService *)pvParameters;
    static ddhttp_task_msg_t ddhttp_msg;



    int task_run = 1;
	while (task_run) 
	{
		if (xQueueReceive(service->que, &ddhttp_msg, portMAX_DELAY)) 
		{
			if (ddhttp_msg.type == DD_CMD_TTS_EVT) 
			{
				if(tts_text_state==tts_text_state_finish)
				{
					ddcloud_http_xunfei_tts(tts_text_nlp_txt,strlen(tts_text_nlp_txt));
				}
			}
		}
	} 
    vTaskDelete(NULL);
}


static void DdHttpServiceActive(DdHttpService *service)
{
	if (xTaskCreate(ddhttp_task, "ddhttp_task", DDHTTP_SERVICE_TASK_SIZE, service,
		DDHTTP_SERVICE_TASK_PRIORITY, NULL) != pdPASS) {
		//ESP_AUDIO_LOGE(TAG, "Error create ddcloud_task");
		return;
	}	

}

static void DdHttpServiceDeactive(DdHttpService *service)
{
//    vQueueDelete(queRecData);
//    queRecData = NULL;
//    ESP_AUDIO_LOGI(RECOG_SERVICE_TAG, "recognitionDeactive\r\n");
}




DdHttpService *DdHttpServiceCreate()
{
    DdHttpService *ddhttp = (DdHttpService *) EspAudioAlloc(1, sizeof(DdHttpService));
    ESP_ERROR_CHECK(!ddhttp);
    ddhttp->Based.deviceEvtNotified = DeviceEvtNotifiedToHttp;
    ddhttp->Based.serviceActive = DdHttpServiceActive;
    ddhttp->Based.serviceDeactive = DdHttpServiceDeactive;
	ddhttp->que = xQueueCreate(5, sizeof(ddhttp_task_msg_t));
	localDdHttpService = ddhttp;
    return ddhttp;
}


