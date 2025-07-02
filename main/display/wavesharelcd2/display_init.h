#pragma once
#include <stdint.h>
#include "lvgl.h"
#include "esp_err.h"



void display_init(void);
void touch_init(void);
esp_err_t lvgl_init(void);
void bsp_brightness_init(void);
void bsp_brightness_set_level(uint8_t);
void lcd_off();