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

#ifndef ALARM_PARSER_H_INCLUDED
#define ALARM_PARSER_H_INCLUDED

#include "AlarmService.h"

/**
 * @brief   parse Alarm struct
 *
 * This function takes the input string, parses it and fills up the alarm structure
 * Functions used for this purpose are: -int Get_Type(char * source,alarm_t ** alarm);
 *                                                    -int Get_Date(char * source, alarm_t ** alarm);
 *                                                    -int Get_Period_Date(char * source, alarm_t ** alarm);
 * @param source takes the line.
 * @param _sData pointer to data structure.
 *
 * @return
 *        OK on success, E_BAD if alarm is bad,
 */
int parseAlarm(struct _alarm_t * _sData,char *source);

/**
 * @brief  romove ABS timer token
 *
 * This functions remove tokens for respective alarms.
*               Tokens removed: '-' and ':'.
*               20 chars parsed for ABS.
*
 * @param source takes the line.
 *
 * @return
 *        OK on success
 */
int remove_Tokens_ABS(char * source);

/**
 * @brief  romove PER timer token
 *
 * This functions remove tokens for respective alarms.
 *               Tokens removed: '-' and ':'.
 *               38 chars parsed for PER.
 *
 * @param source takes the line.
 *
 * @return
 *        OK on success
 */
int remove_Tokens_PER(char * source);

/**
 * @brief get timer type
 *
 * This function takes the string and checks for alarm type.
 *              On success it writes the given type into structure.
 * @param source: takes the line.
 * @param alarm_t ** alarm:  double pointer to data structure.
 *
 * @return
 *        OK on success, E_INVALID_ALARM_TYPE if alarm is not ABS or PER.
 */
int get_Type(char * source,Alarm_t ** alarm);

/**
 * Function Name 	: get_Date
 *
 * Description : This function takes the input string and checks for date.
 *               If date is in correct format function writes data into alarm structure.
 *
 *
 * Side effects  	: None
 *
 * Comment       	:
 *
 * Parameters 		: input: char *source- takes the line.
 *                     output: alarm_t ** alarm) - double pointer to data structure.
 * Returns 		    : OK on success, E_MISSING_DATA - if some data is missing.
 *                                    E_YEAR - if year is incorrect.
 *                                    E_MONTH - if month is incorrect.
 *                                    E_DAY - if day is incorrect.
 *                                    E_HOUR - if hour is incorrect.
 *                                    E_MINUTE - if minute is incorrect.
 *                                    E_SECOND - if second is incorrect.
 *
 *
*/
int get_Date(char * source, Alarm_t ** alarm);

/**
 * Function Name 	: get_Period_Date
 *
 * Description : This function takes the input string and checks for period date.
 *               If date is in correct format function writes data into alarm structure.
 *
 *
 * Side effects  	: None
 *
 * Comment       	:
 *
 * Parameters 		: input: char *source- takes the line.
 *                     output: alarm_t ** alarm) - double pointer to data structure.
 * Returns 		    : OK on success, E_MISSING_DATA - if some data is missing.
 *
 *
*/
int get_Period_Date(char * source, Alarm_t ** alarm);

/**
 * Function Name 	: get_Message
 *
 * Description : This function takes the input string and checks for message.
 *               Message is than copied for 250 chars into the alarm structure.
 *
 * Side effects  	: None
 *
 * Comment       	:
 *
 * Parameters 		: input: char *source- takes the line.
 *                     output: alarm_t ** alarm) - double pointer to data structure.
 * Returns 		    : OK on success,
 *
 *
*/
int get_Message(char * source, Alarm_t ** alarm);

/**
 * Function Name 	: chopN
 *
 * Description : Simple function that cuts first n chars from a string thus making a new
 *               string without first n characters.
 *
 * Side effects  	: None
 *
 * Comment       	: len variable stores the length of a string without the terminating character.
 *
 * Parameters 		: input: char *source- takes the line.
 *                     size_t n - number of first chars cut
 * Returns 		    : void.
 *
 *
*/
void chopN(char *str, size_t n);

/**
 * Function Name 	: printAlarmStructure
 *
 * Description : Simple function that prints the content of an alarm structure.
 *
 * Side effects  	: None
 *
 * Comment       	:
 *
 * Parameters 		: input: alarm_t * alarm
 *
 * Returns 		    : void
 *
 *
*/
void printAlarmStructure(Alarm_t * alarm);

/**
 * Function Name 	: diffTime
 *
 * Description : Simple function calculates time difference between current time and alarm time.
 *
 * Side effects  	: None
 *
 * Comment       	:
 *
 * Parameters 		: input: alarm_t * alarm
 *
 * Returns 		    : int diff, value of difference
 *
 *
 */
int diffTime(Alarm_t * alarm);
#endif
