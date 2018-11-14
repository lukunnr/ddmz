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

#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "esp_types.h"

typedef enum {
    AudioCtlType_Url,
    AudioCtlType_Play,
    AudioCtlType_Vol,
    AudioCtlType_List,
} AudioCtlType;

typedef enum {
    MediaPlayStatus_Unknow,
    MediaPlayStatus_Playing,
    MediaPlayStatus_Start,
    MediaPlayStatus_Pause,
    MediaPlayStatus_Stop,
} MediaPlayStatus;

typedef struct {
    uint32_t type;
    uint32_t live;
    char *url;
    char *name;
} MsgUrlPkg;

typedef struct {
    uint32_t type;
    uint32_t value;             // volume and play status
    uint32_t reserve;
    uint32_t reserve2;
} MsgCtrlPkg;

typedef struct {
    uint32_t type;
    uint32_t live;
    uint32_t count;
    char *buf;
} MsgListPkg;


void user_devicefind_start(void);

int8_t user_devicefind_stop(void);

void user_webserver_start(xQueueHandle queue);

int8_t user_webserver_stop(void);
#endif

