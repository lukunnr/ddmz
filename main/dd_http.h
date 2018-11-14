#ifndef __DD_HTTP_H__
#define __DD_HTTP_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "MediaService.h"
#include "rom/queue.h"
#include "DeviceCommon.h"
#include "EspAudioAlloc.h"


typedef enum {
    DD_CMD_TTS_EVT,
} ddhttp_type_t;



typedef struct {
    uint32_t            *pdata;
    int                 index;
    int                 len;
	ddhttp_type_t		type;
} ddhttp_task_msg_t;


typedef struct DdHttpService {
    MediaService Based;
	void* que;
} DdHttpService;


DdHttpService *DdHttpServiceCreate();


void ddcloud_http_init();
void ddcloud_http_req(char *buffer,int size);
void ddcloud_http_data_post(char *buffer,int size);
void ddcloud_http_data_post_txt();
void ddcloud_http_xunfei_tts(char *buffer,int size);


#endif

