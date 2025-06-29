#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_log.h"

#define BATTERY_ADC_SIZE 15
#define EXAMPLE_BATTERY_ADC_CHANNEL ADC_CHANNEL_4
#define EXAMPLE_ADC_ATTEN ADC_ATTEN_DB_12

static const char *TAG = "battery";

adc_oneshot_unit_handle_t adc1_handle;
adc_cali_handle_t adc1_cali_chan0_handle = NULL;
bool do_calibration1_chan0;

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

// lv_obj_t *label_adc_raw;
// lv_obj_t *label_voltage;

// lv_timer_t *battery_timer = NULL;

// static void battery_timer_callback(lv_timer_t *timer)
// {
//     char str_buffer[20];
//     float voltage;
//     uint16_t adc_value;
//     bsp_battery_get_voltage(&voltage, &adc_value);
//     lv_label_set_text_fmt(label_adc_raw, "%d", adc_value);
//     sprintf(str_buffer, "%.1f", voltage);
//     lv_label_set_text(label_voltage, str_buffer);
// }

// void lvgl_battery_ui_init(lv_obj_t *parent)
// {
//     lv_obj_t *list = lv_list_create(parent);
//     lv_obj_set_size(list, lv_pct(100), lv_pct(100));

//     lv_obj_t *list_item = lv_list_add_btn(list, NULL, "adc_value");
//     label_adc_raw = lv_label_create(list_item);
//     lv_label_set_text(label_adc_raw, "0");

//     list_item = lv_list_add_btn(list, NULL, "voltage");
//     label_voltage = lv_label_create(list_item);
//     lv_label_set_text(label_voltage, "0.0");
//     battery_timer = lv_timer_create(battery_timer_callback, 1000, NULL);
// }
