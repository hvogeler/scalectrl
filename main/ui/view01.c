#include <stdio.h>
#include <string.h>
#include "view01.h"
#include "lvgl.h"
#include "../lvgl/lvgl_init.h"
#include "esp_lvgl_port.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "roboto_bold_40.h"
#include "roboto_bold_60.h"
#include "roboto_regular_20.h"
#include "scale/ble.h"

static const char *TAG = "view01";
void bt_connect_scale(void);
void disconnect_from_scale(void);
void toggle_unit();

static lv_obj_t *lbl_weight;
static lv_obj_t *lbl_unit, *lbl_timer, *lbl_seconds;
static lv_obj_t *btn_connect, *btn_tare, *btn_reset;
static lv_obj_t *lbl_connect, *lbl_tare, *lbl_reset;
static lv_style_t style_pane;
static bool on = false;

bool is_on()
{
    return on;
}

void toggle_on()
{
    on = !on;
    if (lvgl_port_lock(1000))
    {
        lv_label_set_text(lbl_connect, is_on() ? "OFF" : "ON");
        lvgl_port_unlock();
    }
    if (on)
    {
        bt_connect_scale();
    }
    else
    {
        disconnect_from_scale();
    }
}

void power_off()
{
    scale_power_off();
}

void default_cb()
{
    ESP_LOGI(TAG, "Default CB");
}

void make_widget_tree(lv_event_cb_t reset_cb, lv_event_cb_t tare_cb)
{
    // setup screen
    lv_obj_t *screen = lv_scr_act();

    /* Task lock */
    lvgl_port_lock(1000);

    lv_obj_set_scrollbar_mode(screen, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_layout(screen, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(screen, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(screen, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_style_text_color(lv_screen_active(), lv_color_hex(0xffffff), LV_PART_MAIN);

    // panes
    lv_style_init(&style_pane);
    lv_style_set_border_width(&style_pane, 0);

    lv_obj_t *buttons_pane = lv_obj_create(screen);
    lv_obj_add_style(buttons_pane, &style_pane, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_height(buttons_pane, LV_PCT(100));
    lv_obj_set_layout(buttons_pane, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(buttons_pane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(buttons_pane, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
    lv_obj_set_flex_grow(buttons_pane, 3);
    lv_obj_set_style_bg_color(buttons_pane, lv_color_black(), LV_PART_MAIN);

    lv_obj_t *data_pane = lv_obj_create(screen);
    lv_obj_set_scrollbar_mode(data_pane, LV_SCROLLBAR_MODE_OFF);
    lv_obj_set_height(data_pane, LV_PCT(100));
    lv_obj_add_style(data_pane, &style_pane, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_layout(data_pane, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(data_pane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_grow(data_pane, 6);
    lv_obj_set_style_bg_color(data_pane, lv_color_black(), LV_PART_MAIN);

    // lv_style_init(&style_datapane);
    // lv_style_set_pad_bottom(&style_datapane, 45);
    // lv_obj_add_style(data_pane, &style_datapane, LV_PART_MAIN | LV_STATE_DEFAULT);

    lv_obj_t *weight_pane = lv_obj_create(data_pane);
    lv_obj_set_width(weight_pane, LV_PCT(100));
    lv_obj_add_style(weight_pane, &style_pane, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_layout(weight_pane, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(weight_pane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(weight_pane, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_bg_color(weight_pane, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(weight_pane, lv_color_white(), LV_PART_MAIN);

    lv_obj_t *timer_pane = lv_obj_create(data_pane);
    lv_obj_set_width(timer_pane, LV_PCT(100));
    lv_obj_add_style(timer_pane, &style_pane, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_layout(timer_pane, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(timer_pane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(timer_pane, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_style_bg_color(timer_pane, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(timer_pane, lv_color_white(), LV_PART_MAIN);

    // buttons
    btn_connect = lv_button_create(buttons_pane);
    lv_obj_add_event_cb(btn_connect, toggle_on, LV_EVENT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_connect, power_off, LV_EVENT_LONG_PRESSED, NULL);
    lv_obj_set_width(btn_connect, LV_PCT(100));
    lbl_connect = lv_label_create(btn_connect);
    lv_label_set_text(lbl_connect, is_on() ? "OFF" : "ON");
    lv_obj_align(lbl_connect, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_connect, &roboto_regular_20, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn_connect, 1);
    // lv_obj_set_style_border_width(btn_connect, 0, LV_PART_MAIN);
    // lv_obj_set_style_bg_color(btn_connect, lv_palette_main(LV_PALETTE_RED), LV_PART_MAIN | LV_STATE_DEFAULT);

    btn_tare = lv_button_create(buttons_pane);
    lv_obj_add_event_cb(btn_tare, tare_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_width(btn_tare, LV_PCT(100));
    lbl_tare = lv_label_create(btn_tare);
    lv_label_set_text(lbl_tare, "TARE");
    lv_obj_align(lbl_tare, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_tare, &roboto_regular_20, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn_tare, 1);

    btn_reset = lv_button_create(buttons_pane);
    lv_obj_set_width(btn_reset, LV_PCT(100));
    lv_obj_add_event_cb(btn_reset, reset_cb, LV_EVENT_CLICKED, NULL);
    lbl_reset = lv_label_create(btn_reset);
    lv_label_set_text(lbl_reset, "TIMER");
    lv_obj_align(lbl_reset, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_reset, &roboto_regular_20, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn_reset, 1);

    // labels / fields
    // weight
    lbl_unit = lv_label_create(weight_pane);
    lv_obj_add_flag(lbl_unit, LV_OBJ_FLAG_CLICKABLE);
    // lv_obj_add_event_cb(lbl_unit, toggle_unit, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lbl_unit, "Gram");
    lv_obj_align(lbl_unit, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_unit, &roboto_regular_20, LV_PART_MAIN);

    lbl_weight = lv_label_create(weight_pane);
    lv_obj_add_flag(lbl_weight, LV_OBJ_FLAG_CLICKABLE);
    // lv_obj_add_event_cb(lbl_weight, toggle_unit, LV_EVENT_CLICKED, NULL);
    lv_label_set_text(lbl_weight, "0.0");
    lv_obj_align(lbl_weight, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_weight, &roboto_bold_60, LV_PART_MAIN);

    // timer
    lbl_seconds = lv_label_create(timer_pane);
    lv_label_set_text(lbl_seconds, "Seconds");
    lv_obj_align(lbl_seconds, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_seconds, &roboto_regular_20, LV_PART_MAIN);

    lbl_timer = lv_label_create(timer_pane);
    lv_label_set_text(lbl_timer, "0");
    lv_obj_align(lbl_timer, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_font(lbl_timer, &roboto_bold_40, LV_PART_MAIN);

    lvgl_port_unlock();
}

void set_weight(int16_t weight)
{
    float w = (float)weight / 10;

    char str_weight[20];
    snprintf(str_weight, sizeof(str_weight), "%.1f", w);
    // ESP_LOGI(TAG, "Set Weight to %s", str_weight);
    if (lbl_weight != NULL)
    {
        lv_label_set_text_fmt(lbl_weight, "%s", str_weight);
    }
}

void set_timer(int seconds)
{
    // ESP_LOGI(TAG, "Set Timer to %d", seconds);
    if (lbl_timer != NULL)
    {
        lv_label_set_text_fmt(lbl_timer, "%d", seconds);
    }
}

// void toggle_unit()
// {
//     char *current_unit = lv_label_get_text(lbl_unit);
//     if (strcmp(current_unit, "Gram") == 0)
//     {
//         lv_label_set_text(lbl_unit, "Ounce");
//         set_unit(OUNCE);
//     }
//     else
//     {
//         lv_label_set_text(lbl_unit, "Gram");
//         set_unit(GRAM);
//     }
// }