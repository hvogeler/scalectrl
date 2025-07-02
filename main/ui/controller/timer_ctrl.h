#pragma once
typedef enum {
    RUNNING,
    STOPPED,
} timer_state_t;

void timer_start_stop_cb();
void timer_reset_cb();
timer_state_t get_timer_state();
void timer_task(void *params);
