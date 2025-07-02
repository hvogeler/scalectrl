#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "../../scale/ble.h"
#include "../view01.h"

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
