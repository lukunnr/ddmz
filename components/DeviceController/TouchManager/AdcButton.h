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

#ifndef _ADC_BUTTON_H_
#define _ADC_BUTTON_H_

typedef enum {
    BTN_STATE_IDLE,  // 0: idle
    BTN_STATE_CLICK, // 1: pressed
    BTN_STATE_PRESS, // 2: long pressed
    BTN_STATE_RELEASE, // 3: released
} BtnState;


typedef enum {
    USER_KEY_SET,
    USER_KEY_PLAY,
    USER_KEY_REC,
    USER_KEY_MODE,
    USER_KEY_VOL_DOWN,
    USER_KEY_VOL_UP,
    USER_KEY_MAX,
} UserKeyName;

typedef void (*ButtonCallback) (int id, BtnState state);

void adc_btn_test(void);
void adc_btn_init(ButtonCallback cb);

#endif
