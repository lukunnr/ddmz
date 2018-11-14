#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include "esp_wifi.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "esp_event_loop.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "lwip/sockets.h"
#include "lwip/dns.h"
#include "lwip/netdb.h"

#include "esp_log.h"
#include "mqtt_client.h"


#include "esp_log.h"
#include "mqtt_client.h"
#include "dd_mqtt.h"
#include "EspAudio.h"

static const char *TAG = "MQTT_SAMPLE";


int dev_volume;
extern void mp3_pause();
extern void mp3_play();
extern void mp3_stop();
extern void mp3_run(char *url);
extern void dd_set_volume(int vol);

static EventGroupHandle_t wifi_event_group;
const static int CONNECTED_BIT = BIT0;

static esp_err_t mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    esp_mqtt_client_handle_t client = event->client;
    int msg_id;
    // your_context_t *context = event->context;
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
			printf("MQTT_EVENT_CONNECTED\n");
            //msg_id = esp_mqtt_client_subscribe(client, CONFIG_EMITTER_CHANNEL_KEY"/topic/", 0);
            msg_id = esp_mqtt_client_subscribe(client, "/topic/+/event", 0);
			printf("sent subscribe successful,event msg_id=%d\n", msg_id);
			ESP_LOGI(TAG, "sent subscribe successful,event msg_id=%d", msg_id);
			msg_id = esp_mqtt_client_subscribe(client, "/topic/+/msg", 0);
			printf("sent subscribe successful,msg msg_id=%d\n", msg_id);
            ESP_LOGI(TAG, "sent subscribe successful,msg msg_id=%d", msg_id);

            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
			printf("MQTT_EVENT_DISCONNECTED\n");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
			printf("MQTT_EVENT_SUBSCRIBED msg_id=%d\n",event->msg_id);
            //msg_id = esp_mqtt_client_publish(client, CONFIG_EMITTER_CHANNEL_KEY"/topic/", "data", 0, 0, 0);
            msg_id = esp_mqtt_client_publish(client, "/topic/", "data", 0, 0, 0);
            ESP_LOGI(TAG, "sent publish successful, msg_id=%d", msg_id);
			printf("sent publish successful, msg_id=%d\n",msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
			printf("MQTT_EVENT_UNSUBSCRIBED, msg_id=%d\n",event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
			printf("MQTT_EVENT_PUBLISHED, msg_id=%d\n", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");

			if(!strncmp(event->data,"pause",strlen("pause")))
			{
				//mp3_pause();
				printf("player pause...\n");
			}else

			if(!strncmp(event->data,"play",strlen("play")))
			{
				printf("player play...\n");
				//mp3_play();
			}else

			if(!strncmp(event->data,"vol",strlen("vol")))
			{
				dev_volume=(dev_volume+10)%100;
				printf("player set vol=%d\n",dev_volume);
				//dd_set_volume(dev_volume);
			}
			char *newurl=malloc(128);
			memset(newurl,0,128);
			snprintf(newurl,128,"%.*s",event->data_len, event->data);
			printf("newurl=[%s]\n",newurl);

			EspAudioStop();
			EspAudioPlay(newurl,0);
			
			
/*
			//if(!strncmp(event->data,"url:",strlen("url:")))
			{
				
				//mp3_stop();
				//printf("play url->[%d][%s]\n",event->data_len,event->data);
				char *newurl=malloc(128);
				memset(newurl,0,128);
				snprintf(newurl,128,"%.*s",event->data_len, event->data);
				printf("newurl=[%s]\n",newurl);
				mp3_run(newurl);
			}
			*/

            printf("TOPIC=%.*s\r\n", event->topic_len, event->topic);
            printf("DATA=%.*s\r\n", event->data_len, event->data);
			
			printf("msgid=%d\n",event->msg_id);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
			printf("MQTT_EVENT_ERROR\n");
            break;
    }
    return ESP_OK;
}

void mqtt_app_start(void)
{
	esp_log_level_set("*", ESP_LOG_INFO);
    const esp_mqtt_client_config_t mqtt_cfg = {
        //.uri = "mqtts://api.emitter.io:443",    // for mqtt over ssl
          .uri = "mqtt://112.74.166.154:1883", //for mqtt over tcp
        // .uri = "ws://api.emitter.io:8080", //for mqtt over websocket
        // .uri = "wss://api.emitter.io:443", //for mqtt over websocket secure
        .event_handle = mqtt_event_handler,
    };

    ESP_LOGI(TAG, "[APP] Free memory: %d bytes", esp_get_free_heap_size());
    esp_mqtt_client_handle_t client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_start(client);
}


static void DeviceEvtNotifiedToMqtt(DeviceNotification* note)
{
    DdMqttService* service = (DdMqttService*) note->receiver;

    if (DEVICE_NOTIFY_TYPE_WIFI == note->type) {
        DeviceNotifyMsg msg = *((DeviceNotifyMsg*) note->data);
        switch (msg) {
            case DEVICE_NOTIFY_WIFI_GOT_IP:
				printf("mqtt start start\n");
                mqtt_app_start();
				printf("mqtt start done\n");
                break;

            case DEVICE_NOTIFY_WIFI_DISCONNECTED:
                break;

            default:
                break;
        }
    }
}

static void MqttServiceActive(DdMqttService *service)
{


}

static void MqttServiceDeactive(DdMqttService *service)
{
//    vQueueDelete(queRecData);
//    queRecData = NULL;
//    ESP_AUDIO_LOGI(RECOG_SERVICE_TAG, "recognitionDeactive\r\n");
}


DdMqttService *DdmqttServiceCreate()
{
    DdMqttService *ddmqtt = (DdMqttService *) EspAudioAlloc(1, sizeof(DdMqttService));
    ESP_ERROR_CHECK(!ddmqtt);
    ddmqtt->Based.deviceEvtNotified = DeviceEvtNotifiedToMqtt;
    ddmqtt->Based.serviceActive = MqttServiceActive;
    ddmqtt->Based.serviceDeactive = MqttServiceDeactive;
    return ddmqtt;
}


