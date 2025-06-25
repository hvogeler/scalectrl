#pragma once
#include <stdbool.h>
#include "lvgl.h"

void make_widget_tree(lv_event_cb_t, lv_event_cb_t);
void set_weight(int16_t);
void set_timer(int);
bool is_on();
void toggle_on();
