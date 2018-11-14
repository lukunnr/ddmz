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

#ifndef _WIFI_MANAGER_H_
#define _WIFI_MANAGER_H_
#include "DeviceManager.h"

typedef enum {
    SMARTCONFIG_NO,
    SMARTCONFIG_YES
} SmartconfigStatus;

typedef enum {
    WifiState_Unknow,
    WifiState_Config_Timeout,
    WifiState_Connecting,
    WifiState_Connected,
    WifiState_Disconnected,
    WifiState_ConnectFailed,
    WifiState_GotIp,
    WifiState_SC_Disconnected,//smart config Disconnected
    WifiState_BLE_Disconnected,
    WifiState_BLE_Stop,
} WifiState;

struct DeviceController;

typedef struct WifiManager WifiManager;

/* Implements DeviceManager */
struct WifiManager {
    /*extend*/
    DeviceManager Based;
    void (*smartConfig)(WifiManager* manager);
    void (*bleConfig)(WifiManager* manager);
    void (*bleConfigStop)(WifiManager* manager);
    /* private */
};

WifiManager* WifiConfigCreate(struct DeviceController* controller);

#endif
