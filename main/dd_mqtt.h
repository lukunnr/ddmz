#ifndef __DD_MQTT_H__
#define __DD_MQTT_H__
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "MediaService.h"
#include "rom/queue.h"
#include "DeviceCommon.h"
#include "EspAudioAlloc.h"

typedef struct DdMqttService {
    MediaService Based;

} DdMqttService;


DdMqttService *DdmqttServiceCreate();


#endif
