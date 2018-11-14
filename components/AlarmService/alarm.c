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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"
#include "rom/queue.h"
#include "esp_system.h"
#include "esp_audio_log.h"
#include "alarm.h"
#include "alarmparser.h"

int computeTimeDifference(esp_alarm_info *node)
{   /*creating time structures*/
    if(node==NULL){
        return E_ALARM_LIST_EAMPTY;
    }
    struct tm tm;
    time_t t1;
    time(&t1);
    double diff;
    /*update from alarm*/
    tm.tm_year=node->alarm_info.date_t.y-1900;
    tm.tm_mon=node->alarm_info.date_t.m-1;
    tm.tm_mday=node->alarm_info.date_t.d;
    tm.tm_hour=node->alarm_info.date_t.h;
    tm.tm_min=node->alarm_info.date_t.mi;
    tm.tm_sec=node->alarm_info.date_t.s-1;
    tm.tm_isdst=-1;
    /*comparing*/
    diff=difftime(t1,mktime(&tm));
    printf("handle %d Difference is: %lf\n",node->handle,diff);
    return diff;
}
int alarmTrigger(esp_alarm_info *node, int *p){

    if((computeTimeDifference(node)>0) && (node->alarm_info.AlarmType_t==ABS)){
         if(computeTimeDifference(node)<3){
                struct tm * timeinfo;
                time_t now;
                time (&now);
                timeinfo = localtime ( &now );
                printf("ABS Alarm triggered!\n");
                printf ( "Alarm went off at: %s", asctime (timeinfo));
                printf("\n");
                *p=getNodeKey(node);
                return ABS_SUCC;
         }

    } else if(computeTimeDifference(node)>0 && node->alarm_info.AlarmType_t==PER){
        if(computeTimeDifference(node)<3){
            struct tm * timeinfo;
            time_t now;
            time (&now);
            timeinfo = localtime ( &now );
            printf("PER Alarm triggered!\n");
            printf ("Alarm went off at: %s", asctime (timeinfo));
            updatePerAlarm(node);
            return PER_TRIG;
            }

    }
    return ESP_OK;

}
int updatePerAlarm(esp_alarm_info * node){

    node->alarm_info.date_t.y = node->alarm_info.date_t.y + node->alarm_info.pdate_t.y;
    node->alarm_info.date_t.m = node->alarm_info.date_t.m + node->alarm_info.pdate_t.m;
    node->alarm_info.date_t.d = node->alarm_info.date_t.d + node->alarm_info.pdate_t.d;
    node->alarm_info.date_t.h = node->alarm_info.date_t.h + node->alarm_info.pdate_t.h;
    node->alarm_info.date_t.mi = node->alarm_info.date_t.mi + node->alarm_info.pdate_t.mi;
    node->alarm_info.date_t.s = node->alarm_info.date_t.s + node->alarm_info.pdate_t.s;

    return ESP_OK;

}
 int getNodeKey(esp_alarm_info * node){

    if(node == NULL)
        return E_ALARM_ARG;
    int key;
    key = node->handle;
    return key;

 }
