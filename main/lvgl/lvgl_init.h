#pragma once

void lvgl_init();
void lvgl_task(void *);
bool lvgl_lock(int);
void lvgl_unlock(void);