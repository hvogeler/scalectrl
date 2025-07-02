#pragma once
#include <stdbool.h>
#include "lvgl.h"

void make_widget_tree(void);
void set_weight(int16_t);
void set_timer(int);
void set_battery(float voltage);
bool is_on();
void toggle_on();
