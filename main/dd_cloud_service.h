#ifndef __dd_cloud_service_h__
#define __dd_cloud_service_h__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "MediaService.h"
#include "rom/queue.h"
#include "DeviceCommon.h"
#include "EspAudioAlloc.h"

typedef struct DdCloudService {
    MediaService Based;
	void* que;
} DdCloudService;

DdCloudService *DdCloudServiceCreate();

#endif
