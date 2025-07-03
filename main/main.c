#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "esp_err.h"
#include "display/wavesharelcd2/display_init.h"
#include "esp_lvgl_port.h"
#include "esp_sleep.h"
#include "ui/view01.h"
#include "ui/controller/battery_ctrl.h"
#include "ui/controller/timer_ctrl.h"
#include "ui/controller/scale_ctrl.h"

#define CST816D_INT_PIN 5

static const char *TAG = "scalectrl";

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
    make_widget_tree();
    battery_init();

    esp_sleep_wakeup_cause_t wakeup_cause = esp_sleep_get_wakeup_cause();
    ESP_LOGI(TAG, "Wakeup Cause: %d", wakeup_cause);

    // Start weight collection task
    xTaskCreatePinnedToCore(collect_weight_task, "collect_weight_task", 1024 * 2, NULL, 5, NULL, 1);

    // Start timer task
    xTaskCreatePinnedToCore(timer_task, "timer_task", 1024 * 3, NULL, 5, NULL, 1);

    // Start battery task
    xTaskCreatePinnedToCore(check_battery_task, "check_battery_task", 1024 * 2, NULL, 5, NULL, 1);

    ESP_LOGI(TAG, "Setup complete");

    // Monitor loop
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        ESP_LOGI(TAG, "Free heap: %lu bytes", esp_get_free_heap_size());
    }
}