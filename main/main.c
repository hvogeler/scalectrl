#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_lcd_panel_io.h"
#include "display/wavesharelcd2/display_init.h"
#include "lvgl/lvgl_init.h"
#include "ui/view01.h"

static const char *TAG = "scalectrl";
static int weight = 0;
static int seconds = 0;

void reset_cb()
{
    if (lvgl_lock(-1))
    {
        seconds = 0;
        set_timer(seconds);
        lvgl_unlock();
    }
}

void tare_cb()
{
    if (lvgl_lock(-1))
    {
        weight = 0;
        set_weight(weight);
        lvgl_unlock();
    }
}

void app_main(void)
{
    // Initialize hardware
    display_init();
    touch_init();

    // Initialize LVGL
    lvgl_init();

    // Setup backlight
    bsp_brightness_init();
    bsp_brightness_set_level(80);

    // Create UI
    if (lvgl_lock(-1))
    {
        ESP_LOGI(TAG, "Creating widgets demo");
        // lv_demo_widgets();
        make_widget_tree(reset_cb, tare_cb);
        lvgl_unlock();
    }

    // Start LVGL task
    xTaskCreatePinnedToCore(lvgl_task, "lvgl_task", 1024 * 24, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Setup complete");

    // Monitor loop
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        if (is_on())
        {
            if (lvgl_lock(1))
            {
                weight += 5;
                set_weight(weight);
                set_timer(++seconds);
                lvgl_unlock();
            }
        }
        // ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    }
}