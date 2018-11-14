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

#include "alarmparser.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_system.h"
#include "esp_audio_log.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include "AlarmService.h"

int remove_TESP_OKens_ABS(char * source)
{
    char* i = source;
    char* j = source;
    int cnt=0;  //counter for chars
    while(*j != 0){
    *i = *j++;
    if((*i == '-') || (*i ==':')){
            *i=' ';
        }
    i++;
    cnt++;
    if(cnt==ABS_FORMAT_LEN){
            while(*j != 0){
                *i = *j++;
                i++;
            }
            break;
        }
    }
  *i = '\0';

    return E_ALARM_OK;
}
int remove_TESP_OKens_PER(char * source)
{

    char* i = source;
    char* j = source;
    int cnt=0;  //counter for chars
    while(*j != 0){
    *i = *j++;
    if((*i == '-') || (*i ==':')){
            *i=' ';
        }
    i++;
    cnt++;
    if(cnt==PER_FORMAT_LEN){
        while(*j != 0){
            *i = *j++;
            i++;
            }
            break;
      }
    }
  *i = '\0';
    return E_ALARM_OK;
}
void chopN(char *str, size_t n)
{

    size_t len = strlen(str);
    if (n > len)
        return;

    memmove(str, str+n, len - n + 1);
}
int get_Type(char * source,Alarm_t ** alarm)
{
    /** ABS+'\0'= 4 **/
    short typeLen=4;
    char type[typeLen];

    strncpy(type,source,typeLen-1);
    type[typeLen-1]='\0';
    if(strcmp(type,"ABS")==0){
        (*alarm)->AlarmType_t=ABS;
        chopN(source,typeLen);
    }
    else if(strcmp(type,"PER")==0){
       (*alarm)->AlarmType_t=PER;
       chopN(source,typeLen);
    }
    else{
        /**Error: Wrong type**/
        return E_ALARM_INVALID_TYPE;
        }
return E_ALARM_OK;

}
int get_Date(char * source, Alarm_t ** alarm)
{
    /**format is const of length 20**/
    short dateLen=20;
    if(6!=sscanf(source,"%d %d %d %d %d %d",&(*alarm)->date_t.y,&(*alarm)->date_t.m,&(*alarm)->date_t.d,&(*alarm)->date_t.h,&(*alarm)->date_t.mi,&(*alarm)->date_t.s))
    {
        printf("Data missing from date.\n");
        return E_ALARM_MISSING_DATA;
    }
    if((*alarm)->date_t.y<2016)
    {
        printf("Error: Year must be greater than 2016.\n");
        return E_ALARM_YEAR;
    }
    if((*alarm)->date_t.m<0 || (*alarm)->date_t.m>12 )
    {
        printf("Error: Month must be from 1 to 12.\n");
        return E_ALARM_MONTH;
    }
    if((*alarm)->date_t.d<0 || (*alarm)->date_t.d>31)
    {
        printf("Error: Days must be from 0 - 31.\n");
        return E_ALARM_DAY;
    }
    if((*alarm)->date_t.h<0 || (*alarm)->date_t.h>24)
    {
        printf("Error: Hours must be from 0 - 24.\n");
        return E_ALARM_HOUR;
    }
    if((*alarm)->date_t.mi<0 || (*alarm)->date_t.mi>59)
    {
        printf("Error: Minutes must be from 0 - 59.\n");
        return E_ALARM_MINUTE;
    }
    if((*alarm)->date_t.s<0 || (*alarm)->date_t.s>59)
    {
        printf("Error: Seconds must be from 0 - 59\n");
        return E_ALARM_SECOND;
    }

    chopN(source,dateLen);

    return E_ALARM_OK;
}

int get_Period_Date(char * source, Alarm_t ** alarm)
{
    short dateLen=18;
    if(6!=sscanf(source,"%d %d %d %d %d %d",&(*alarm)->pdate_t.y,&(*alarm)->pdate_t.m,&(*alarm)->pdate_t.d,&(*alarm)->pdate_t.h,&(*alarm)->pdate_t.mi,&(*alarm)->pdate_t.s))
    {
        printf("Data missing from date.\n");
        return E_ALARM_MISSING_DATA;
    }
    int sum_per;
    /**checking if period is zero**/
    sum_per = (*alarm)->pdate_t.y+(*alarm)->pdate_t.m+(*alarm)->pdate_t.d+(*alarm)->pdate_t.h+(*alarm)->pdate_t.mi+(*alarm)->pdate_t.s;
    if(sum_per==0)
    {
        return E_ALARM_PER_ZERO;
    }
    chopN(source,dateLen);
    return E_ALARM_OK;
}

void printAlarmStructure(Alarm_t * alarm)
{
    printf("Type of alarm :%d \n",(*alarm).AlarmType_t);
    printf("Year of alarm :%d \n",(*alarm).date_t.y);
    printf("Month of alarm :%d \n",(*alarm).date_t.m);
    printf("Day of alarm :%d \n",(*alarm).date_t.d);
    printf("Hour of alarm :%d \n",(*alarm).date_t.h);
    printf("Minute of alarm :%d \n",(*alarm).date_t.mi);
    printf("Second of alarm :%d \n",(*alarm).date_t.s);

    printf("Type  alarm :%d \n",alarm->AlarmType_t);
    printf("Year of alarm :%d \n",alarm->pdate_t.y);
    printf("Month of alarm :%d \n",(*alarm).pdate_t.m);
    printf("Day of alarm :%d \n",(*alarm).pdate_t.d);
    printf("Hour of alarm :%d \n",(*alarm).pdate_t.h);
    printf("Minute of alarm :%d \n",(*alarm).pdate_t.mi);
    printf("Second of alarm :%d \n",(*alarm).pdate_t.s);
}

int diffTime(Alarm_t * alarm)
{   /*creating time structures*/

    struct tm tm;
    time_t t1;
    time(&t1);
    double diff;
    tm.tm_year=alarm->date_t.y-1900;
    tm.tm_mon=alarm->date_t.m-1;
    tm.tm_mday=alarm->date_t.d;
    tm.tm_hour=alarm->date_t.h;
    tm.tm_min=alarm->date_t.mi;
    tm.tm_sec=alarm->date_t.s-1;
    tm.tm_isdst=-1;
    /*comparing*/
    diff=difftime(t1,mktime(&tm));
    return diff;
}

int parseAlarm(Alarm_t * alarm,char *source)
{
    if(source==NULL || alarm == NULL){
        printf("Error: line empty\n");
        return E_ALARM_ARG;
    }

    if(E_ALARM_INVALID_TYPE==get_Type(source,&alarm)){
        printf("Error: Invalid alarm type\n");
        return E_ALARM_ARG;
        }

    if(alarm->AlarmType_t==ABS){
       remove_TESP_OKens_ABS(source);
        if(get_Date(source,&alarm) != ESP_OK || diffTime(alarm)>0){
              printf("Error: Invalid alarm data\n");
              return E_ALARM_ARG;
          }
       }
    else{
       remove_TESP_OKens_PER(source);
       if(get_Date(source,&alarm) != 0){
              return E_ALARM_ARG;
          }
       if(get_Period_Date(source,&alarm) != ESP_OK){
            printf("Correct your period.\n");
        return E_ALARM_ARG;
       }
       while(diffTime(alarm)>0){
        alarm->date_t.y = alarm->date_t.y + alarm->pdate_t.y;
        alarm->date_t.m = alarm->date_t.m + alarm->pdate_t.m;
        alarm->date_t.d = alarm->date_t.d + alarm->pdate_t.d;
        alarm->date_t.h = alarm->date_t.h + alarm->pdate_t.h;
        alarm->date_t.mi = alarm->date_t.mi + alarm->pdate_t.mi;
        alarm->date_t.s = alarm->date_t.s + alarm->pdate_t.s;

       }
    }

  return ESP_OK;
}

