#pragma once

enum unit_t
{
    GRAM,
    OUNCE,
};

void bt_connect_scale(void);
void scale_tare();
void scale_led_on_gram();
void scale_led_on_ounce();
void scale_led_off();
void scale_power_off();
void scale_timer_on();
void scale_timer_off();
void scale_timer_reset();
int16_t get_weight10();
void set_unit(enum unit_t unit);