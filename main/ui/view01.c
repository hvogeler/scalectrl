#include <stdio.h>
#include <string.h>
#include "view01.h"
#include "lvgl.h"
#include "esp_lvgl_port.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "roboto_bold_40.h"
#include "roboto_bold_60.h"
#include "roboto_regular_20.h"
#include "scale/ble.h"
#include "ui/controller/battery_ctrl.h"
#include "ui/controller/timer_ctrl.h"
#include "ui/controller/scale_ctrl.h"

static const char *TAG = "view01";
void bt_connect_scale(void);
void disconnect_from_scale(void);

static lv_obj_t *lbl_weight, *lbl_battery;
static lv_obj_t *lbl_unit, *lbl_timer, *lbl_seconds;
static lv_obj_t *btn_connect, *btn_tare, *btn_timer;
static lv_obj_t *lbl_connect, *lbl_tare, *lbl_reset;
static lv_obj_t *img_battery;
static lv_style_t style_pane, style_btn, style_weight;
static bool on = false;

bool is_on()
{
    return on;
}

void toggle_on()
{
    ESP_LOGI(TAG, "ON Pressed");

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

void make_widget_tree()
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

    // panestyle
    lv_style_init(&style_pane);
    lv_style_set_border_width(&style_pane, 0);
    lv_style_set_flex_grow(&style_pane, 0);
    lv_style_set_pad_all(&style_pane, 0);

    // button style
    lv_style_init(&style_btn);
    lv_style_set_pad_all(&style_btn, 10);
    lv_style_set_pad_gap(&style_btn, 10);
    lv_style_set_margin_all(&style_btn, 0);

    // panes
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
    lv_obj_set_width(data_pane, LV_PCT(100));
    lv_obj_set_height(data_pane, LV_PCT(100));
    lv_obj_add_style(data_pane, &style_pane, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_layout(data_pane, LV_LAYOUT_FLEX);
    lv_obj_set_flex_flow(data_pane, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(data_pane, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_END);
    lv_obj_set_flex_grow(data_pane, 6);
    lv_obj_set_style_bg_color(data_pane, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(data_pane, lv_color_white(), LV_PART_MAIN);
    lv_obj_set_style_pad_all(data_pane, 5, LV_PART_MAIN);

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

    btn_tare = lv_button_create(buttons_pane);
    lv_obj_add_event_cb(btn_tare, tare_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_set_width(btn_tare, LV_PCT(100));
    lbl_tare = lv_label_create(btn_tare);
    lv_label_set_text(lbl_tare, "TARE");
    lv_obj_align(lbl_tare, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_tare, &roboto_regular_20, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn_tare, 1);
    lv_obj_add_style(btn_tare, &style_btn, LV_PART_MAIN);

    btn_timer = lv_button_create(buttons_pane);
    lv_obj_set_width(btn_timer, LV_PCT(100));
    lv_obj_add_event_cb(btn_timer, timer_start_stop_cb, LV_EVENT_SHORT_CLICKED, NULL);
    lv_obj_add_event_cb(btn_timer, timer_reset_cb, LV_EVENT_LONG_PRESSED, NULL);
    lbl_reset = lv_label_create(btn_timer);
    lv_label_set_text(lbl_reset, "TIMER");
    lv_obj_align(lbl_reset, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_font(lbl_reset, &roboto_regular_20, LV_PART_MAIN);
    lv_obj_set_flex_grow(btn_timer, 1);

    // labels / fields
    // weight
    lbl_unit = lv_label_create(data_pane);
    lv_obj_add_flag(lbl_unit, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(lbl_unit, "Gram");
    lv_obj_set_style_text_font(lbl_unit, &roboto_regular_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_unit, lv_color_hex(0xbbbbbb), LV_PART_MAIN);

    lbl_weight = lv_label_create(data_pane);
    lv_obj_add_flag(lbl_weight, LV_OBJ_FLAG_CLICKABLE);
    lv_label_set_text(lbl_weight, "0.0");
    lv_obj_set_style_text_font(lbl_weight, &roboto_bold_60, LV_PART_MAIN);
    lv_obj_set_style_margin_bottom(lbl_weight, 10, LV_PART_MAIN);

    // timer
    lbl_seconds = lv_label_create(data_pane);
    lv_label_set_text(lbl_seconds, "Seconds");
    lv_obj_set_style_text_font(lbl_seconds, &roboto_regular_20, LV_PART_MAIN);
    lv_obj_set_style_text_color(lbl_seconds, lv_color_hex(0xbbbbbb), LV_PART_MAIN);

    lbl_timer = lv_label_create(data_pane);
    lv_label_set_text(lbl_timer, "0");
    lv_obj_set_style_text_font(lbl_timer, &roboto_bold_40, LV_PART_MAIN);
    lv_obj_set_style_margin_bottom(lbl_timer, 10, LV_PART_MAIN);

    // battery
    lv_obj_t *bat_widget = lv_obj_create(data_pane);
    lv_obj_set_flex_flow(bat_widget, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bat_widget, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_bg_color(bat_widget, lv_color_black(), LV_PART_MAIN);
    lv_obj_set_style_text_color(bat_widget, lv_color_hex(0xbbbbbb), LV_PART_MAIN);
    lv_obj_set_style_border_width(bat_widget, 0, LV_PART_MAIN);

    lbl_battery = lv_label_create(bat_widget);
    lv_label_set_text(lbl_battery, "V0.9.0");
    lv_obj_set_style_text_font(lbl_battery, &roboto_regular_20, LV_PART_MAIN);
    img_battery = lv_image_create(bat_widget);
    lv_image_set_src(img_battery, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_size(bat_widget, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_pad_all(bat_widget, 0, 0);

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

int round_to_int(float x)
{
    return (int)(x + (x >= 0 ? 0.5f : -0.5f));
}

void set_battery(float voltage)
{
    // char str_voltage[20];
    // snprintf(str_voltage, sizeof(str_voltage), "%.1f", voltage);
    char bat_percent[20];
    snprintf(bat_percent, sizeof(bat_percent), "%d %%", round_to_int(voltage_to_percentage_sigmoid(voltage)));
    // ESP_LOGI(TAG, "Set Battery percent to %s", percent);
    if (lbl_battery != NULL)
    {
        lv_image_set_src(img_battery, LV_SYMBOL_BATTERY_FULL);
        int bat_percent = round_to_int(voltage_to_percentage_sigmoid(voltage));
        lv_label_set_text_fmt(lbl_battery, "%d%%", bat_percent);
        uint32_t color = 0x00aa00; // green
        if (bat_percent < 40)
        {
            color = 0x00aaaa;
        }
        if (bat_percent < 15)
        {
            color = 0xaa0000;
        }
        lv_obj_set_style_text_color(img_battery, lv_color_hex(color), LV_PART_MAIN);
    }
}

void set_timer(int seconds)
{
    if (lvgl_port_lock(1000))
    {
        // ESP_LOGI(TAG, "Set Timer to %d", seconds);
        if (lbl_timer != NULL)
        {
            lv_label_set_text_fmt(lbl_timer, "%d", seconds);
        }
        lvgl_port_unlock();
    }
}