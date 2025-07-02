#include "esp_log.h"
#include "../../scale/ble.h"
static const char *TAG = "scale_ctrl";

void tare_cb()
{
    ESP_LOGI(TAG, "TARE Pressed");
    scale_tare();
}
