#include <math.h>
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "ui/view01.h"

#define BATTERY_ADC_SIZE 15
#define EXAMPLE_BATTERY_ADC_CHANNEL ADC_CHANNEL_4
#define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_12

static const char *TAG = "battery_ctrl";

adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_chan0_handle = NULL;
bool do_calibration1_chan0;

// static SemaphoreHandle_t battery_mutex = NULL; // not used because there is no state in this controller

static bool example_adc_calibration_init(adc_unit_t unit, adc_channel_t channel, adc_atten_t atten, adc_cali_handle_t *out_handle)
{
    adc_cali_handle_t handle = NULL;
    esp_err_t ret = ESP_FAIL;
    bool calibrated = false;

#if ADC_CALI_SCHEME_CURVE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Curve Fitting");
        adc_cali_curve_fitting_config_t cali_config = {
            .unit_id = unit,
            .chan = channel,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_curve_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

#if ADC_CALI_SCHEME_LINE_FITTING_SUPPORTED
    if (!calibrated)
    {
        ESP_LOGI(TAG, "calibration scheme version is %s", "Line Fitting");
        adc_cali_line_fitting_config_t cali_config = {
            .unit_id = unit,
            .atten = atten,
            .bitwidth = ADC_BITWIDTH_DEFAULT,
        };
        ret = adc_cali_create_scheme_line_fitting(&cali_config, &handle);
        if (ret == ESP_OK)
        {
            calibrated = true;
        }
    }
#endif

    *out_handle = handle;
    if (ret == ESP_OK)
    {
        ESP_LOGI(TAG, "Calibration Success");
    }
    else if (ret == ESP_ERR_NOT_SUPPORTED || !calibrated)
    {
        ESP_LOGW(TAG, "eFuse not burnt, skip software calibration");
    }
    else
    {
        ESP_LOGE(TAG, "Invalid arg or no memory");
    }

    return calibrated;
}

void battery_init(void)
{
    // As long as there is not state in this controller no need for mutex (hopefully)
    // if (battery_mutex == NULL)
    // {
    //     battery_mutex = xSemaphoreCreateMutex();
    // }

    adc_oneshot_unit_init_cfg_t init_config1 = {
        .unit_id = ADC_UNIT_1,
    };
    ESP_ERROR_CHECK(adc_oneshot_new_unit(&init_config1, &adc1_handle));
    //-------------ADC1 Config---------------//
    adc_oneshot_chan_cfg_t config = {
        .bitwidth = ADC_BITWIDTH_DEFAULT,
        .atten = EXAMPLE_ADC_ATTEN,
    };
    ESP_ERROR_CHECK(adc_oneshot_config_channel(adc1_handle, EXAMPLE_BATTERY_ADC_CHANNEL, &config));

    //-------------ADC1 Calibration Init---------------//

    do_calibration1_chan0 = example_adc_calibration_init(ADC_UNIT_1, EXAMPLE_BATTERY_ADC_CHANNEL, EXAMPLE_ADC_ATTEN, &adc1_cali_chan0_handle);
}

// 4.2V = 100%
// 4.0V ≈ 85%
// 3.8V ≈ 65%
// 3.6V ≈ 40%
// 3.4V ≈ 20%
// 3.2V ≈ 5%
// 3.0V = 0%
uint8_t mapBatVolt2Pct(float v)
{
    if (v >= 4.2)
    {
        return 100;
    }
    if (v >= 4.0 && v < 4.2)
    {
        return 85;
    }
    if (v > 3.8 && v < 4.0)
    {
        return 65;
    }
    if (v >= 3.6 && v < 3.8)
    {
        return 40;
    }
    if (v >= 3.4 && v < 3.6)
    {
        return 20;
    }
    if (v >= 3.2 && v < 3.4)
    {
        return 5;
    }
    return 0;
}

/**
 * Convert lithium battery voltage to percentage using sigmoid function
 *
 * @param voltage: Battery voltage in volts (typically 3.0V to 4.2V)
 * @return: Battery percentage (0.0 to 100.0)
 */
float voltage_to_percentage_sigmoid(float voltage)
{
    const float V_MIN = 3.0f;      // Minimum voltage (0%)
    const float V_MAX = 4.2f;      // Maximum voltage (100%)
    const float V_MID = 3.6f;      // Midpoint voltage (50%)
    const float steepness = 12.0f; // Controls curve steepness

    // Sigmoid function: 1 / (1 + e^(-k*(x-x0)))
    float normalized = (voltage - V_MID) * steepness;
    float sigmoid = 1.0f / (1.0f + expf(-normalized));

    // Scale to 0-100 percentage
    float percentage = sigmoid * 100.0f;

    // Apply hard limits based on voltage range
    if (voltage <= V_MIN)
        percentage = 0.0f;
    if (voltage >= V_MAX)
        percentage = 100.0f;

    // Clamp to 0-100 range (safety check)
    if (percentage < 0.0f)
        percentage = 0.0f;
    if (percentage > 100.0f)
        percentage = 100.0f;

    return percentage;
}

void battery_get_voltage(float *voltage, uint16_t *adc_value)
{
    int adc_raw;
    int voltage_int;

    ESP_ERROR_CHECK(adc_oneshot_read(adc1_handle, EXAMPLE_BATTERY_ADC_CHANNEL, &adc_raw));

    // ESP_LOGI(TAG, "ADC%d Channel[%d] Raw Data: %d", ADC_UNIT_1 + 1, EXAMPLE_BATTERY_ADC_CHANNEL, adc_raw);
    if (do_calibration1_chan0)
    {
        ESP_ERROR_CHECK(adc_cali_raw_to_voltage(adc1_cali_chan0_handle, adc_raw, &voltage_int));
        *voltage = (voltage_int / 1000.0f) * 3.0;
        *adc_value = adc_raw;
    }
}

void check_battery_task(void *params)
{
    while (1)
    {
        vTaskDelay(pdMS_TO_TICKS(10000));
        float voltage;
        uint16_t adc_value;
        battery_get_voltage(&voltage, &adc_value);
        set_battery(voltage);
        // ESP_LOGI(TAG, "Battery: %.2f", voltage);
    }
}