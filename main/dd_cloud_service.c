#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_audio_log.h"

#include "DeviceCommon.h"
#include "dd_cloud_service.h"
#include "esp_audio_device_info.h"

#include "apps/sntp/sntp.h"

#include "EspAudioAlloc.h"
#include "Recorder.h"
#include "toneuri.h"
#include "recorder_engine.h"
#include "sdkconfig.h"
#include "apps/sntp/sntp.h"
#include "ESCodec_common.h"
#include "ES8388_interface.h"
#include "dd_http.h"



static char *TAG = "DDCLOUD";
const static int CONNECT_BIT = BIT0;

typedef enum {
    DD_CMD_UNKNOWN,
    DD_CMD_CONNECTED,
    DD_CMD_START,
    DD_CMD_STOP,
    DD_CMD_QUIT,
    DD_CMD_WAKEUP,
} ddcloud_type_t;



typedef struct {
    uint32_t            *pdata;
    int                 index;
    int                 len;
	ddcloud_type_t		type;
} ddcloud_task_msg_t;


static DdCloudService          *localDdCloudService;
static EventGroupHandle_t       reprog_evt_group;


static void dd_rec_engine_cb(rec_event_type_t type);
static esp_err_t dd_recorder_open(void **handle);
static esp_err_t dd_recorder_read(void *handle, char *data, int data_size);
static esp_err_t dd_recorder_close(void *handle);


#define DDCLOUD_SERVICE_TASK_PRIORITY     5
#define DDCLOUD_SERVICE_TASK_SIZE         16*1024
#define RECORD_SAMPLE_RATE              (16000)
#define RECOGNITION_TRIGGER_URI         "i2s://16000:1@record.pcm#raw"


static bool mic_mute_flag=false;



void ddOSEvtSend(void *que, int type, void *data, int index, int len, int dir)
{
    ddcloud_task_msg_t evt = {0};
    evt.type = type;
    evt.pdata = data;
    evt.index = index;
    evt.len = len;
    if (dir) {
        xQueueSendToFront(que, &evt, 0) ;
    } else {
        xQueueSend(que, &evt, 0);
    }
}



static void wait_for_sntp(void)
{
    time_t now = 0;
    struct tm timeinfo = { 0 };
    int retry = 0;

    printf("Initializing SNTP\n");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_init();

    const int retry_count = 5;
    while (timeinfo.tm_year < (2016 - 1900) && ++retry < retry_count) {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        time(&now);
        localtime_r(&now, &timeinfo);
    }
}

static void ddcloud_task(void *pvParameters)
{
    DdCloudService *service =  (DdCloudService *)pvParameters;

    uint8_t *voiceData = EspAudioAlloc(1, REC_ONE_BLOCK_SIZE);
    if (NULL == voiceData) {
        ESP_AUDIO_LOGE(TAG, "Func:%s, Line:%d, Malloc failed", __func__, __LINE__);
        return;
    }
	
    static ddcloud_task_msg_t ddcloud_msg;
    xEventGroupWaitBits(reprog_evt_group, CONNECT_BIT, false, true, 5000 / portTICK_PERIOD_MS);
	wait_for_sntp();
    rec_config_t eng = DEFAULT_REC_ENGINE_CONFIG();
    eng.vad_off_delay_ms = 1000;
    eng.wakeup_time_ms = 4000;
    eng.evt_cb = dd_rec_engine_cb;
    eng.open = dd_recorder_open;
    eng.close = dd_recorder_close;
    eng.fetch = dd_recorder_read;
    rec_engine_create(&eng);

    FILE *file = NULL;
	char filename[64]={0};

    int task_run = 1;
	int filecnt=0;
	int sendcnt=0;

	#define MIC_1S_BUFFSIZE (16000*2)
	#define MIC_MAX_SAVE_TIME 10
	#define MIC_SAVE_BUFFER_SIZE (MIC_1S_BUFFSIZE*MIC_MAX_SAVE_TIME)
	
	char *mic_save_buffer=malloc(MIC_SAVE_BUFFER_SIZE);	//最大允许10秒
	int   mic_save_buffer_size=0;
	memset(mic_save_buffer,0,MIC_SAVE_BUFFER_SIZE);
	
	while (task_run) 
	{
		if (xQueueReceive(service->que, &ddcloud_msg, portMAX_DELAY)) 
		{
			if (ddcloud_msg.type == DD_CMD_WAKEUP) 
			{
				while(1)
				{
					int ret = rec_engine_data_read(voiceData, REC_ONE_BLOCK_SIZE, 110 / portTICK_PERIOD_MS);
					if ((ret == 0) || (ret == -1)) {
						printf("rec_engine_data_read ret = %d\n",ret);
						fclose(file);
						file=NULL;

						if(mic_save_buffer_size>(MIC_1S_BUFFSIZE/2))	//录音至少超过0.5秒
						{
							printf("post data size = %dk\n",mic_save_buffer_size/1024);
							//ddcloud_http_xunfei_tts("hello , what your name",strlen("hello , what your name"));
							ddcloud_http_data_post(mic_save_buffer,mic_save_buffer_size);
						}

						mic_save_buffer_size=0;
						memset(mic_save_buffer,0,MIC_SAVE_BUFFER_SIZE);
						break;
					}else
					{
						if(file==NULL)
						{
							//ddcloud_http_init();
							sendcnt=0;
							sprintf(filename,"/sdcard/rec_debug_%d.wav",filecnt++);
							printf("open file %s\n",filename);
							file = fopen(filename, "w");
							if (NULL == file) {
								ESP_AUDIO_LOGW(TAG, "open %s failed,[%d]",filename, __LINE__);
								break;
							}
						}

						if((mic_save_buffer_size+REC_ONE_BLOCK_SIZE)<=MIC_SAVE_BUFFER_SIZE)
						{
							mic_save_buffer_size+=REC_ONE_BLOCK_SIZE;
							memcpy(mic_save_buffer+mic_save_buffer_size,voiceData,REC_ONE_BLOCK_SIZE);
						}else
						{
							printf("mic save timeout\n");
						}
						fwrite(voiceData, 1, REC_ONE_BLOCK_SIZE, file);
					}
				}			
			}
		}
	} 
	
    free(voiceData);
    vTaskDelete(NULL);
}

static void ddcloudEvtNotif(DeviceNotification *note)
{
    DdCloudService *service = (DdCloudService *) note->receiver;
    if (DEVICE_NOTIFY_TYPE_TOUCH == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg *) note->data);
        switch (msg) {
			case DEVICE_NOTIFY_KEY_UNDEFINED:
				printf("ddcloud DEVICE_NOTIFY_KEY_UNDEFINED\n");
				break;
			case DEVICE_NOTIFY_KEY_VOL_UP:
				printf("ddcloud DEVICE_NOTIFY_KEY_VOL_UP\n");
				break;
			case DEVICE_NOTIFY_KEY_PREV:
				printf("ddcloud DEVICE_NOTIFY_KEY_PREV\n");
				break;
			case DEVICE_NOTIFY_KEY_VOL_DOWN:
				printf("ddcloud DEVICE_NOTIFY_KEY_VOL_DOWN\n");
				break;
			case DEVICE_NOTIFY_KEY_NEXT:
				printf("ddcloud DEVICE_NOTIFY_KEY_NEXT\n");
				break;
			case DEVICE_NOTIFY_KEY_WIFI_SET:
				printf("ddcloud DEVICE_NOTIFY_KEY_WIFI_SET\n");
				break;
			case DEVICE_NOTIFY_KEY_PLAY:
				printf("ddcloud DEVICE_NOTIFY_KEY_PLAY\n");
				break;
			case DEVICE_NOTIFY_KEY_REC:
				printf("ddcloud DEVICE_NOTIFY_KEY_REC\n");
				rec_engine_trigger_start();
				break;
			case DEVICE_NOTIFY_KEY_REC_QUIT:
				printf("ddcloud DEVICE_NOTIFY_KEY_REC_QUIT\n");
				rec_engine_trigger_stop();
				break;
			case DEVICE_NOTIFY_KEY_MODE:
				printf("ddcloud DEVICE_NOTIFY_KEY_MODE\n");
				break;
			case DEVICE_NOTIFY_KEY_BT_WIFI_SWITCH:
				printf("ddcloud DEVICE_NOTIFY_KEY_BT_WIFI_SWITCH\n");
				break;
			case DEVICE_NOTIFY_KEY_AIWIFI_WECHAT_SWITCH:
				printf("ddcloud DEVICE_NOTIFY_KEY_AIWIFI_WECHAT_SWITCH\n");
				break;
            default:
                break;
        }
    } 
	else if (DEVICE_NOTIFY_TYPE_WIFI == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg *) note->data);
        switch (msg) {
            case DEVICE_NOTIFY_WIFI_GOT_IP:
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_WIFI_GOT_IP");
                xEventGroupSetBits(reprog_evt_group, CONNECT_BIT);
                break;
            case DEVICE_NOTIFY_WIFI_DISCONNECTED:
                ESP_AUDIO_LOGW(TAG, "DEVICE_NOTIFY_WIFI_DISCONNECTED");
                break;
            case DEVICE_NOTIFY_WIFI_SETTING_TIMEOUT:
                ESP_AUDIO_LOGI(TAG, "DEVICE_NOTIFY_WIFI_SETTING_TIMEOUT");
                break;
            default:
                break;
        }
    }
}


static void dd_rec_engine_cb(rec_event_type_t type)
{
    if (REC_EVENT_WAKEUP_START == type) {
		printf("dd_rec_engine_cb REC_EVENT_WAKEUP_START\n");
		ddOSEvtSend(localDdCloudService->que, DD_CMD_WAKEUP, NULL, 0, 0, 0);
    } else if (REC_EVENT_VAD_START == type) {
   	 	printf("dd_rec_engine_cb REC_EVENT_VAD_START\n");
    } else if (REC_EVENT_VAD_STOP == type) {
		printf("dd_rec_engine_cb REC_EVENT_VAD_STOP\n");
    } else if (REC_EVENT_WAKEUP_END == type) {
		printf("dd_rec_engine_cb REC_EVENT_WAKEUP_END\n");
		
    } else {

    }
}


static esp_err_t dd_recorder_open(void **handle)
{
    int ret = EspAudioRecorderStart(RECOGNITION_TRIGGER_URI, 10 * 1024, 0);
    return ret;
}

static esp_err_t dd_recorder_read(void *handle, char *data, int data_size)
{
    int len = 0;
    int ret = 0;
    if (mic_mute_flag == true) {
        memset(data, 0, data_size);
        len = data_size;
        vTaskDelay(10 / portTICK_PERIOD_MS);
    } else {
        ret = EspAudioRecorderRead((uint8_t *)data, data_size, &len);
    }
    if (ret == 0) {
        return len;
    } else {
        return -1;
    }
}

static esp_err_t dd_recorder_close(void *handle)
{
    int ret = EspAudioRecorderStop(TERMINATION_TYPE_NOW);
    return ret;
}


static void ddcloudAppActive(DdCloudService *service)
{
    if (xTaskCreate(ddcloud_task, "ddcloud_task", DDCLOUD_SERVICE_TASK_SIZE, service,
                    DDCLOUD_SERVICE_TASK_PRIORITY, NULL) != pdPASS) {
        ESP_AUDIO_LOGE(TAG, "Error create ddcloud_task");
        return;
    }
    ESP_AUDIO_LOGI(TAG, "ddcloudAppActive");
}

static void ddcloudAppDeactive(DdCloudService *service)
{
    vQueueDelete(service->que);
    service->que = NULL;
    ESP_AUDIO_LOGI(TAG, "ddcloudAppDeactive");
}


DdCloudService *DdCloudServiceCreate()
{
    DdCloudService *recprog = (DdCloudService *) EspAudioAlloc(1, sizeof(DdCloudService));
    ESP_ERROR_CHECK(!recprog);
    recprog->Based.deviceEvtNotified = ddcloudEvtNotif;
    recprog->Based.serviceActive = ddcloudAppActive;
    recprog->Based.serviceDeactive = ddcloudAppDeactive;
    reprog_evt_group = xEventGroupCreate();
    configASSERT(reprog_evt_group);
    recprog->que = xQueueCreate(5, sizeof(ddcloud_task_msg_t));
    configASSERT(recprog->que);
    ESP_AUDIO_LOGI(TAG, "ddcloudService has create");
    localDdCloudService = recprog;
    return recprog;
}








