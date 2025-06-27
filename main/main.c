#include <stdio.h>
#include <string.h>
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "display/wavesharelcd2/display_init.h"
#include "esp_lvgl_port.h"
#include "ui/view01.h"
#include "scale/ble.h"

static const char *TAG = "scalectrl";
static int seconds = 0;

void reset_cb()
{
    ESP_LOGI(TAG, "TIMER Pressed");
    if (lvgl_port_lock(0))
    {
        seconds = 0;
        set_timer(seconds);
        scale_timer_reset();
        scale_timer_on();
        lvgl_port_unlock();
    }
}

void tare_cb()
{
    ESP_LOGI(TAG, "TARE Pressed");
    if (lvgl_port_lock(0))
    {
        scale_tare();
        lvgl_port_unlock();
    }
}

void collect_weight_task(void *params)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (is_on())
        {
            if (lvgl_port_lock(1))
            {
                int16_t weight10 = get_weight10();
                set_weight(weight10);
                lvgl_port_unlock();
            }
        }
    }
}

void timer_task(void *params)
{
    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1000ms = 1 second

    // Initialize the xLastWakeTime variable with the current time
    xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        // Increment counter
        seconds++;
        if (is_on())
        {
            if (lvgl_port_lock(1000))
            {
                set_timer(seconds);
                lvgl_port_unlock();
            }
        }
        // Wait for the next cycle (1 second)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}

void app_main(void)
{
    // Initialize hardware
    display_init();
    touch_init();
    ESP_ERROR_CHECK(lvgl_init());

    // Setup backlight
    bsp_brightness_init();
    bsp_brightness_set_level(80);

    // Create UI
    make_widget_tree(reset_cb, tare_cb);

    // Start weight collection task
    xTaskCreatePinnedToCore(collect_weight_task, "collect_weight_task", 1024 * 2, NULL, 5, NULL, 1);

    // Start timer task
    xTaskCreatePinnedToCore(timer_task, "timer_task", 1024 * 2, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Setup complete");

    // Monitor loop
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    }
}