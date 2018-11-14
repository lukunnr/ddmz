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

#ifndef _MESH_CONTROL_H_
#define _MESH_CONTROL_H_

typedef enum {
    COLOR_LIGHT_WHITE,
    COLOR_LIGHT_YELLOW,
    COLOR_LIGHT_RED,
    COLOR_LIGHT_PURPLE,
    COLOR_LIGHT_GREEN,
    COLOR_LIGHT_BLUE,
    COLOR_LIGHT_PINK,
} color_light_t;

typedef enum {
    CTRL_LIGHT_OFF,
    CTRL_LIGHT_ON,
} ctrl_light_t;

typedef enum {
    MESH_CTRL_EVT_COLOR,
    MESH_CTRL_EVT_SWITCH,
    MESH_CTRL_EVT_QUIT,
}  mesh_ctrl_cmd_t;


typedef struct {
    mesh_ctrl_cmd_t     cmd;
    int                 data;
} mesh_ctrl_msg_t;

esp_err_t mesh_light_ctrl_init();
esp_err_t mesh_light_color_set(color_light_t color);
esp_err_t mesh_light_switch(ctrl_light_t ctrl);
esp_err_t mesh_light_ctrl_deinit();

#endif