#pragma once
#include <stdint.h>
#include "lvgl.h"
#include "esp_err.h"

// #define LCD_H_RES 240
// #define LCD_V_RES 320
#define LCD_H_RES 320
#define LCD_V_RES 240

void display_init(void);
void touch_init(void);
esp_err_t lvgl_init(void);
void bsp_brightness_init(void);
void bsp_brightness_set_level(uint8_t);
// void lvgl_flush_cb(lv_display_t *, const lv_area_t *, uint8_t *);
// void lvgl_touch_cb(lv_indev_t *, lv_indev_data_t *);