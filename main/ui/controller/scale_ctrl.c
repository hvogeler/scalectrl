#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "../../scale/ble.h"
#include "../view01.h"
#include "../../display/wavesharelcd2/display_init.h"
#include <esp_err.h>
#include "esp_sleep.h"
#include "driver/gpio.h"

static const char *TAG = "scale_ctrl";

void tare_cb()
{
    ESP_LOGI(TAG, "TARE Pressed");
    scale_tare();
}

void collect_weight_task(void *params)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(100));
        if (is_on())
        {
            int16_t weight10 = get_weight10();
            set_weight(weight10);
        }
    }
}

void enter_deep_sleep(void)
{
    ESP_LOGI(TAG, "Preparing for deep sleep");

    // Turn off backlight
    gpio_set_level(5, 1);
    lcd_off();
    vTaskDelay(pdMS_TO_TICKS(1000));
    esp_deep_sleep_start();
}
