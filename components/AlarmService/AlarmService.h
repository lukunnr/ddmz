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

#ifndef _ALARM_SERVICE_H_
#define _ALARM_SERVICE_H_
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "MediaService.h"
#include "AlarmService.h"
#include "rom/queue.h"

/******************************************************************************/
/**Size for ABS format parsing**/
#define ABS_FORMAT_LEN 20
/**Size for PER format parsing**/
#define PER_FORMAT_LEN 38
/**Size for MAX format parsing**/
#define MAX_FORMAT_LEN 42

typedef uint32_t esp_alarm_handle;

typedef void(*alarm_trigger_func)(void *arg, esp_alarm_handle handle);

enum trigger_type{
    ABS_SUCC=100,
    PER_TRIG=101,
};
typedef enum Alarm_Code_t{
    E_ALARM_OK = 0,
    E_ALARM_ARG= -19000,
    E_ALARM_NOREADY,
    E_ALARM_NOMEM,
    E_ALARM_NOTFOUND,
    E_ALARM_INVALID_TYPE,
    E_ALARM_MISSING_DATA,
    E_ALARM_YEAR,
    E_ALARM_MONTH,
    E_ALARM_DAY,
    E_ALARM_HOUR,
    E_ALARM_MINUTE,
    E_ALARM_SECOND,
    E_ALARM_LIST_EAMPTY,
    E_ALARM_PER_ZERO,
}Alarm_Code;

typedef enum AlarmStatus_t{
    A_idle,
    A_running,
    A_disabled,
    A_deleted,
}AlarmStatus;

typedef struct _alarm_t{

enum{
    ABS=0,
    PER=1
}AlarmType_t;

struct _date_t{

int y,m,d,h,mi,s;
}date_t;

struct p_date_t{

int y,m,d,h,mi,s;
}pdate_t;

}Alarm_t;

typedef struct esp_alarm_info_t {
    int diff;
    esp_alarm_handle handle;
    AlarmStatus status;
    Alarm_t alarm_info;
    void *trigger_arg;
    alarm_trigger_func trigger_func;
    LIST_ENTRY(esp_alarm_info_t) entries;
} esp_alarm_info;

typedef struct AlarmService {
    MediaService Based;

} AlarmService;

/**
 * @brief  add alarm
 *
 * This function will create a alarm, if register the trigger function,it will be triggered if timeout
 * @param char* alarm_buf: timer parser bug
 * for example:
 *  "ABS 2017-12-25 10:10:27 "Message for alarm at 2017-12-25 10:10:27"
 *  "PER 2017-12-16 11:10:07 00-00-00 00:01:03" Periodic message that stared on 2017-12-16 11:10:07 and repeats every 63 seconds
 * @param handle: pointer to return timer handle.
 *
 * @return
 *        E_ALARM_OK on success, or another error code in enum Alarm_Code_t
 */
Alarm_Code esp_alarm_add(char* alarm_buf,esp_alarm_handle* handle);

/**
 * @brief  delete an alarm as input alarm handle
 *
 * This function will delete an alarm,if not find the timer in timer list will return error
 * @param handle: timer handle.
 *
 * @return
 *        E_ALARM_OK on success, or another error code in enum Alarm_Code_t
 */
Alarm_Code esp_alarm_delete(esp_alarm_handle handle);

/**
 * @brief  edit an alarm as input alarm handle and edit info
 *
 * This function will edit an alarm,if not find the timer in timer list will return error
 * @param alarm_info: edit alarm info
 * @param handle: timer handle.
 *
 * @return
 *        E_ALARM_OK on success, or another error code in enum Alarm_Code_t
 */
Alarm_Code esp_alarm_edit(esp_alarm_info* alarm_info, esp_alarm_handle handle);

/**
 * @brief  get an alarm's info as input alarm handle
 *
 * This function will get an alarm's info ,if not find the timer in timer list will return error
 * @param alarm_info: edit alarm info
 * @param handle: timer handle.
 *
 * @return
 *        E_ALARM_OK on success, or another error code in enum Alarm_Code_t
 */
Alarm_Code esp_get_alarm_info(esp_alarm_handle handle,esp_alarm_info* alarm_info);

/**
 * @brief register the timer trigger function as input timer handle
 *
 * @param arg: callback arg
 * @param handle: timer handle.
 * @param func: trigger function of alarm handle.
 *
 * @return
 *        E_ALARM_OK on success, or another error code in enum Alarm_Code_t
 */
Alarm_Code esp_alarm_register_trigger_func(void * arg,esp_alarm_handle handle,alarm_trigger_func func);

/**
 * @brief create the alarm service of Audio Project
 *
 * @return
 *        alarm service
 */
AlarmService *AlarmServiceCreate();

#endif
