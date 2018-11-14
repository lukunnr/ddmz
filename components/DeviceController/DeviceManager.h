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

/**
 * Interface
 */

#ifndef _DEVICE_MANAGER_H_
#define _DEVICE_MANAGER_H_

#include "esp_audio_log.h"

enum DeviceNotifyType;

struct DeviceController;

typedef struct DeviceManager DeviceManager;

/* Interface */
struct DeviceManager {
    void *instance;
    struct DeviceController *controller;  /* backward reference */
    char *tag;  //XXX: is this needed?? good for debug and visualization
    int (*active)(DeviceManager *manager);
    void (*deactive)(DeviceManager *manager);
    void (*notify)(struct DeviceController *controller, enum DeviceNotifyType type, void *data, int len);
    void (*pauseMedia)(DeviceManager *controller);
    void (*resumeMedia)(DeviceManager *controller);
};

void InitManager(DeviceManager *manager, struct DeviceController *controller);

#endif
