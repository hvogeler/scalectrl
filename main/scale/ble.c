/*
 * SPDX-FileCopyrightText: 2021-2024 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

/****************************************************************************
 *
 * This demo showcases BLE GATT client. It can scan BLE devices and connect to one device.
 * Run the gatt_server demo, the client demo will automatically connect to the gatt_server demo.
 * Client demo will enable gatt_server's notify after connection. The two devices will then exchange
 * data.
 *
 ****************************************************************************/

#include <stdint.h>
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"

#include "esp_bt.h"
#include "esp_gap_ble_api.h"
#include "esp_gattc_api.h"
#include "esp_gatt_defs.h"
#include "esp_bt_main.h"
#include "esp_gatt_common_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "ble.h"

#define SCALE_SERVICE_UUID 0xFFF0
#define SET_CHAR_UUID 0x36F5
#define NOTIFY_CHAR_UUID 0xFFF4
#define PROFILE_NUM 1
#define PROFILE_A_APP_ID 0
#define INVALID_HANDLE 0
#if CONFIG_EXAMPLE_INIT_DEINIT_LOOP
#define EXAMPLE_TEST_COUNT 50
#endif

static char remote_device_name[ESP_BLE_ADV_NAME_LEN_MAX] = "Decent Scale";
static bool connect = false;
static bool get_server = false;
static esp_gattc_char_elem_t *char_elem_result = NULL;
static esp_gattc_descr_elem_t *descr_elem_result = NULL;

static uint8_t tare[] = {0x03, 0x0f, 0x00, 0x00, 0x00, 0x00, 0x0c};
static uint8_t led_on_gram[] = {0x03, 0x0a, 0x01, 0x01, 0x00, 0x00, 0x09};
static uint8_t led_on_ounce[] = {0x03, 0x0a, 0x01, 0x01, 0x01, 0x00, 0x08};
static uint8_t led_off[] = {0x03, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x09};

static uint8_t power_off[] = {0x03, 0x0a, 0x02, 0x00, 0x00, 0x00, 0x0b};

static uint8_t timer_on[] = {0x03, 0x0b, 0x03, 0x00, 0x00, 0x00, 0x0b};
static uint8_t timer_off[] = {0x03, 0x0b, 0x00, 0x00, 0x00, 0x00, 0x08};
static uint8_t timer_reset[] = {0x03, 0x0b, 0x02, 0x00, 0x00, 0x00, 0x0a};

/* Declare static functions */
static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param);
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param);
void write_char(uint8_t write_data[]);

char *TAG = "ble";

// The UUID of the service containing characteristics: 0000fff0-0000-1000-8000-00805f9b34fb
// The UUID of the set chatacteristic:                 000036f5-0000-1000-8000-00805f9b34fb
// The UUID of the notify chatacteristic:              0000FFF3-0000-1000-8000-00805f9b34fb

static int16_t weight10 = 0;
static SemaphoreHandle_t weight_mutex = NULL;

static void set_weight10(int16_t value)
{
    if (weight_mutex == NULL)
    {
        weight_mutex = xSemaphoreCreateMutex();
    }
    if (xSemaphoreTake(weight_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        weight10 = value;
        xSemaphoreGive(weight_mutex);
    }
}

int16_t get_weight10()
{
    int16_t retval = 0;
    if (weight_mutex == NULL)
    {
        weight_mutex = xSemaphoreCreateMutex();
    }
    if (xSemaphoreTake(weight_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        retval = weight10;
        xSemaphoreGive(weight_mutex);
    }
    return retval;
}

static esp_bt_uuid_t set_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = SET_CHAR_UUID,
    },
};

static esp_bt_uuid_t notify_char_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = NOTIFY_CHAR_UUID,
    },
};

static esp_bt_uuid_t notify_descr_uuid = {
    .len = ESP_UUID_LEN_16,
    .uuid = {
        .uuid16 = ESP_GATT_UUID_CHAR_CLIENT_CONFIG,
    },
};

static esp_ble_scan_params_t ble_scan_params = {
    .scan_type = BLE_SCAN_TYPE_ACTIVE,
    .own_addr_type = BLE_ADDR_TYPE_PUBLIC,
    .scan_filter_policy = BLE_SCAN_FILTER_ALLOW_ALL,
    .scan_interval = 0x50,
    .scan_window = 0x30,
    .scan_duplicate = BLE_SCAN_DUPLICATE_DISABLE};

struct gattc_profile_inst
{
    esp_gattc_cb_t gattc_cb;
    uint16_t gattc_if;
    uint16_t app_id;
    uint16_t conn_id;
    uint16_t service_start_handle;
    uint16_t service_end_handle;
    uint16_t set_char_handle;
    uint16_t notify_char_handle;
    esp_bd_addr_t remote_bda;
};

/* One gatt-based profile one app_id and one gattc_if, this array will store the gattc_if returned by ESP_GATTS_REG_EVT */
static struct gattc_profile_inst gl_profile_tab[PROFILE_NUM] = {
    [PROFILE_A_APP_ID] = {
        .gattc_cb = gattc_profile_event_handler,
        .gattc_if = ESP_GATT_IF_NONE, /* Not get the gatt_if, so initial is ESP_GATT_IF_NONE */
    },
};

// Decent Scale requires to be sent a few commands before it
// starts delivering weight notifications
void init_scale()
{
    vTaskDelay(pdMS_TO_TICKS(100));
    scale_tare();
    vTaskDelay(pdMS_TO_TICKS(100));
    scale_led_on_gram();
    vTaskDelay(pdMS_TO_TICKS(100));
    scale_tare();
}

void uuid128_to_string(const uint8_t *uuid128, char *str_buf, size_t buf_size)
{
    // UUID128 format: xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx
    // ESP-IDF stores UUID128 in little-endian format
    snprintf(str_buf, buf_size,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             uuid128[15], uuid128[14], uuid128[13], uuid128[12],                      // bytes 15-12
             uuid128[11], uuid128[10],                                                // bytes 11-10
             uuid128[9], uuid128[8],                                                  // bytes 9-8
             uuid128[7], uuid128[6],                                                  // bytes 7-6
             uuid128[5], uuid128[4], uuid128[3], uuid128[2], uuid128[1], uuid128[0]); // bytes 5-0
}

// GATT Profile Specific Handler
static void gattc_profile_event_handler(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    esp_ble_gattc_cb_param_t *p_data = (esp_ble_gattc_cb_param_t *)param;

    switch (event)
    {
    case ESP_GATTC_REG_EVT:
        ESP_LOGI(TAG, "GATT client register, status %d, app_id %d, gattc_if %d", param->reg.status, param->reg.app_id, gattc_if);
        esp_err_t scan_ret = esp_ble_gap_set_scan_params(&ble_scan_params);
        if (scan_ret)
        {
            ESP_LOGE(TAG, "set scan params error, error code = %x", scan_ret);
        }
        break;
    case ESP_GATTC_CONNECT_EVT:
    {
        ESP_LOGI(TAG, "Connected, conn_id %d, remote " ESP_BD_ADDR_STR "", p_data->connect.conn_id,
                 ESP_BD_ADDR_HEX(p_data->connect.remote_bda));
        gl_profile_tab[PROFILE_A_APP_ID].conn_id = p_data->connect.conn_id;
        memcpy(gl_profile_tab[PROFILE_A_APP_ID].remote_bda, p_data->connect.remote_bda, sizeof(esp_bd_addr_t));
        esp_err_t mtu_ret = esp_ble_gattc_send_mtu_req(gattc_if, p_data->connect.conn_id);
        if (mtu_ret)
        {
            ESP_LOGE(TAG, "Config MTU error, error code = %x", mtu_ret);
        }

        break;
    }
    case ESP_GATTC_OPEN_EVT:
        if (param->open.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Open failed, status %d", p_data->open.status);
            break;
        }
        ESP_LOGI(TAG, "Open successfully, MTU %u", p_data->open.mtu);
        break;
    case ESP_GATTC_DIS_SRVC_CMPL_EVT:
        if (param->dis_srvc_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Service discover failed, status %d", param->dis_srvc_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Service discover complete, conn_id %d", param->dis_srvc_cmpl.conn_id);
        esp_ble_gattc_search_service(gattc_if, param->dis_srvc_cmpl.conn_id, NULL);
        break;
    case ESP_GATTC_CFG_MTU_EVT:
        ESP_LOGI(TAG, "MTU exchange, status %d, MTU %d", param->cfg_mtu.status, param->cfg_mtu.mtu);
        break;
    case ESP_GATTC_SEARCH_RES_EVT:
    {
        ESP_LOGI(TAG, "Service search result, conn_id = %x, is primary service %d", p_data->search_res.conn_id, p_data->search_res.is_primary);
        ESP_LOGI(TAG, "start handle %d, end handle %d, current handle value %d", p_data->search_res.start_handle, p_data->search_res.end_handle, p_data->search_res.srvc_id.inst_id);
        esp_bt_uuid_t *uuid = &p_data->search_res.srvc_id.uuid;

        if (uuid->len == ESP_UUID_LEN_16)
        {
            ESP_LOGI(TAG, "UUID16: 0x%04x", uuid->uuid.uuid16);
        }

        if (uuid->len == ESP_UUID_LEN_32)
        {
            ESP_LOGI(TAG, "UUID32: 0x%08lx", uuid->uuid.uuid32);
        }

        if (uuid->len == ESP_UUID_LEN_128)
        {
            char uuid128_str[37] = {0};
            uuid128_to_string(uuid->uuid.uuid128, uuid128_str, sizeof(uuid128_str));
            ESP_LOGI(TAG, "UUID128 (LE): %s", uuid128_str);
        }
        if (p_data->search_res.srvc_id.uuid.len == ESP_UUID_LEN_16 && p_data->search_res.srvc_id.uuid.uuid.uuid16 == SCALE_SERVICE_UUID)
        {
            ESP_LOGI(TAG, "*** Service found ***");
            get_server = true;
            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle = p_data->search_res.start_handle;
            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle = p_data->search_res.end_handle;
            ESP_LOGI(TAG, "UUID16: %x", p_data->search_res.srvc_id.uuid.uuid.uuid16);

            // Get all characteristics
        }
        break;
    }

    case ESP_GATTC_SEARCH_CMPL_EVT:
        if (p_data->search_cmpl.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Service search failed, status %x", p_data->search_cmpl.status);
            break;
        }
        if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_REMOTE_DEVICE)
        {
            ESP_LOGI(TAG, "Get service information from remote device");
        }
        else if (p_data->search_cmpl.searched_service_source == ESP_GATT_SERVICE_FROM_NVS_FLASH)
        {
            ESP_LOGI(TAG, "Get service information from flash");
        }
        else
        {
            ESP_LOGI(TAG, "Unknown service source");
        }
        ESP_LOGI(TAG, "Service search complete");
        if (get_server)
        {
            uint16_t count = 0;
            esp_gatt_status_t status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                    p_data->search_res.conn_id,
                                                                    ESP_GATT_DB_CHARACTERISTIC,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                    INVALID_HANDLE,
                                                                    &count);
            if (status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
                break;
            }

            if (count > 1)
            {
                ESP_LOGI(TAG, "Found %d characteristics", count);
                char_elem_result = (esp_gattc_char_elem_t *)malloc(sizeof(esp_gattc_char_elem_t) * count);
                if (!char_elem_result)
                {
                    ESP_LOGE(TAG, "gattc no mem");
                    break;
                }
                else
                {
                    // Get Set Characteristic (allows to set Tare)
                    status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                            p_data->search_cmpl.conn_id,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                            set_char_uuid,
                                                            char_elem_result,
                                                            &count);
                    if (status != ESP_GATT_OK)
                    {
                        ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                        free(char_elem_result);
                        char_elem_result = NULL;
                        break;
                    }
                    gl_profile_tab[PROFILE_A_APP_ID].set_char_handle = char_elem_result[0].char_handle;
                    ESP_LOGI(TAG, "set charac handle: %d, gattc_if: %d", gl_profile_tab[PROFILE_A_APP_ID].set_char_handle, gl_profile_tab[PROFILE_A_APP_ID].gattc_if);

                    // Get Notification Characteristic (that sends the weight)
                    status = esp_ble_gattc_get_char_by_uuid(gattc_if,
                                                            p_data->search_cmpl.conn_id,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                            gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                            notify_char_uuid,
                                                            char_elem_result,
                                                            &count);
                    if (status != ESP_GATT_OK)
                    {
                        ESP_LOGE(TAG, "esp_ble_gattc_get_char_by_uuid error");
                        free(char_elem_result);
                        char_elem_result = NULL;
                        break;
                    }
                    gl_profile_tab[PROFILE_A_APP_ID].notify_char_handle = char_elem_result[0].char_handle;
                    status = esp_ble_gattc_register_for_notify(
                        gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
                        gl_profile_tab[PROFILE_A_APP_ID].remote_bda,
                        gl_profile_tab[PROFILE_A_APP_ID].notify_char_handle);

                    ESP_LOGI(TAG, "notify charac handle: %d, gattc_if: %d", gl_profile_tab[PROFILE_A_APP_ID].notify_char_handle, gl_profile_tab[PROFILE_A_APP_ID].gattc_if);

                    init_scale();
                    /*  Every service have only one char in our 'ESP_GATTS_DEMO' demo, so we used first 'char_elem_result' */
                    // if (count > 0 && (char_elem_result[0].properties & ESP_GATT_CHAR_PROP_BIT_NOTIFY))
                    // {
                    //     gl_profile_tab[PROFILE_A_APP_ID].char_handle = char_elem_result[0].char_handle;
                    //     esp_ble_gattc_register_for_notify(gattc_if, gl_profile_tab[PROFILE_A_APP_ID].remote_bda, char_elem_result[0].char_handle);
                    // }
                }
                /* free char_elem_result */
                free(char_elem_result);
            }
            else
            {
                ESP_LOGE(TAG, "no char found");
            }
        }
        break;
    case ESP_GATTC_REG_FOR_NOTIFY_EVT:
    {
        if (p_data->reg_for_notify.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Notification register failed, status %d", p_data->reg_for_notify.status);
        }
        else
        {
            ESP_LOGI(TAG, "Notification register successfully");
            uint16_t count = 0;
            uint16_t notify_en = 1;
            esp_gatt_status_t ret_status = esp_ble_gattc_get_attr_count(gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        ESP_GATT_DB_DESCRIPTOR,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].service_start_handle,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].service_end_handle,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].notify_char_handle,
                                                                        &count);
            if (ret_status != ESP_GATT_OK)
            {
                ESP_LOGE(TAG, "esp_ble_gattc_get_attr_count error");
                break;
            }
            if (count > 0)
            {
                descr_elem_result = malloc(sizeof(esp_gattc_descr_elem_t) * count);
                if (!descr_elem_result)
                {
                    ESP_LOGE(TAG, "malloc error, gattc no mem");
                    break;
                }
                else
                {
                    ret_status = esp_ble_gattc_get_descr_by_char_handle(gattc_if,
                                                                        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                        p_data->reg_for_notify.handle,
                                                                        notify_descr_uuid,
                                                                        descr_elem_result,
                                                                        &count);
                    if (ret_status != ESP_GATT_OK)
                    {
                        ESP_LOGE(TAG, "esp_ble_gattc_get_descr_by_char_handle error");
                        free(descr_elem_result);
                        descr_elem_result = NULL;
                        break;
                    }
                    /* Every char has only one descriptor in our 'ESP_GATTS_DEMO' demo, so we used first 'descr_elem_result' */
                    if (count > 0 && descr_elem_result[0].uuid.len == ESP_UUID_LEN_16 && descr_elem_result[0].uuid.uuid.uuid16 == ESP_GATT_UUID_CHAR_CLIENT_CONFIG)
                    {
                        ret_status = esp_ble_gattc_write_char_descr(gattc_if,
                                                                    gl_profile_tab[PROFILE_A_APP_ID].conn_id,
                                                                    descr_elem_result[0].handle,
                                                                    sizeof(notify_en),
                                                                    (uint8_t *)&notify_en,
                                                                    ESP_GATT_WRITE_TYPE_RSP,
                                                                    ESP_GATT_AUTH_REQ_NONE);
                    }

                    if (ret_status != ESP_GATT_OK)
                    {
                        ESP_LOGE(TAG, "esp_ble_gattc_write_char_descr error");
                    }

                    /* free descr_elem_result */
                    free(descr_elem_result);
                }
            }
            else
            {
                ESP_LOGE(TAG, "decsr not found");
            }
        }
        break;
    }
    case ESP_GATTC_NOTIFY_EVT:
        uint8_t *buf = p_data->notify.value;
        if (p_data->notify.value_len > 3 && buf[1] != 0xaa)
        {
            uint16_t highByte = (uint16_t)buf[2];
            uint16_t lowByte = (uint16_t)buf[3];
            int16_t weight10 = (highByte << 8) + lowByte;
            // ESP_LOGI(TAG, "Weight: %d", weight10);
            set_weight10(weight10);
        }

        break;
    // case ESP_GATTC_WRITE_DESCR_EVT:
    //     if (p_data->write.status != ESP_GATT_OK)
    //     {
    //         ESP_LOGE(TAG, "Descriptor write failed, status %x", p_data->write.status);
    //         break;
    //     }
    //     ESP_LOGI(TAG, "Descriptor write successfully");
    //     uint8_t write_char_data[35];
    //     for (int i = 0; i < sizeof(write_char_data); ++i)
    //     {
    //         write_char_data[i] = i % 256;
    //     }
    //     esp_ble_gattc_write_char(gattc_if,
    //                              gl_profile_tab[PROFILE_A_APP_ID].conn_id,
    //                              gl_profile_tab[PROFILE_A_APP_ID].char_handle,
    //                              sizeof(write_char_data),
    //                              write_char_data,
    //                              ESP_GATT_WRITE_TYPE_RSP,
    //                              ESP_GATT_AUTH_REQ_NONE);
    //     break;
    case ESP_GATTC_SRVC_CHG_EVT:
    {
        esp_bd_addr_t bda;
        memcpy(bda, p_data->srvc_chg.remote_bda, sizeof(esp_bd_addr_t));
        ESP_LOGI(TAG, "Service change from " ESP_BD_ADDR_STR "", ESP_BD_ADDR_HEX(bda));
        break;
    }
    case ESP_GATTC_WRITE_CHAR_EVT:
        if (p_data->write.status != ESP_GATT_OK)
        {
            ESP_LOGE(TAG, "Characteristic write failed, status %x)", p_data->write.status);
            break;
        }
        ESP_LOGI(TAG, "Characteristic write successfully with handle: %d", gl_profile_tab[PROFILE_A_APP_ID].set_char_handle);
        break;
    case ESP_GATTC_DISCONNECT_EVT:
        connect = false;
        get_server = false;
        ESP_LOGI(TAG, "Disconnected, remote " ESP_BD_ADDR_STR ", reason 0x%02x",
                 ESP_BD_ADDR_HEX(p_data->disconnect.remote_bda), p_data->disconnect.reason);
        break;
    default:
        break;
    }
}

static void esp_gap_cb(esp_gap_ble_cb_event_t event, esp_ble_gap_cb_param_t *param)
{
    uint8_t *adv_name = NULL;
    uint8_t adv_name_len = 0;
    switch (event)
    {
    case ESP_GAP_BLE_SCAN_PARAM_SET_COMPLETE_EVT:
    {
        // The unit of duration is seconds.
        // If duration is set to 0, scanning will continue indefinitely
        // until esp_ble_gap_stop_scanning is explicitly called.
        uint32_t duration = 30;
        esp_ble_gap_start_scanning(0);
        break;
    }
    case ESP_GAP_BLE_SCAN_START_COMPLETE_EVT:
        // scan start complete event to indicate scan start successfully or failed
        if (param->scan_start_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Scanning start failed, status %x", param->scan_start_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Scanning start successfully");

        break;
    case ESP_GAP_BLE_SCAN_RESULT_EVT:
    {
        esp_ble_gap_cb_param_t *scan_result = (esp_ble_gap_cb_param_t *)param;
        switch (scan_result->scan_rst.search_evt)
        {
        case ESP_GAP_SEARCH_INQ_RES_EVT:
            adv_name = esp_ble_resolve_adv_data_by_type(scan_result->scan_rst.ble_adv,
                                                        scan_result->scan_rst.adv_data_len + scan_result->scan_rst.scan_rsp_len,
                                                        ESP_BLE_AD_TYPE_NAME_CMPL,
                                                        &adv_name_len);
            ESP_LOGI(TAG, "Scan result, device " ESP_BD_ADDR_STR ", name len %u", ESP_BD_ADDR_HEX(scan_result->scan_rst.bda), adv_name_len);
            ESP_LOG_BUFFER_CHAR(TAG, adv_name, adv_name_len);

#if CONFIG_EXAMPLE_DUMP_ADV_DATA_AND_SCAN_RESP
            if (scan_result->scan_rst.adv_data_len > 0)
            {
                ESP_LOGI(TAG, "adv data:");
                ESP_LOG_BUFFER_HEX(TAG, &scan_result->scan_rst.ble_adv[0], scan_result->scan_rst.adv_data_len);
            }
            if (scan_result->scan_rst.scan_rsp_len > 0)
            {
                ESP_LOGI(TAG, "scan resp:");
                ESP_LOG_BUFFER_HEX(TAG, &scan_result->scan_rst.ble_adv[scan_result->scan_rst.adv_data_len], scan_result->scan_rst.scan_rsp_len);
            }
#endif

            if (adv_name != NULL)
            {
                if (strlen(remote_device_name) == adv_name_len && strncmp((char *)adv_name, remote_device_name, adv_name_len) == 0)
                {
                    // Note: If there are multiple devices with the same device name, the device may connect to an unintended one.
                    // It is recommended to change the default device name to ensure it is unique.
                    if (connect == false)
                    {
                        ESP_LOGI(TAG, "Device found %s", remote_device_name);
                        connect = true;
                        ESP_LOGI(TAG, "Connect to the remote device");
                        esp_ble_gap_stop_scanning();
                        esp_ble_gatt_creat_conn_params_t creat_conn_params = {0};
                        memcpy(&creat_conn_params.remote_bda, scan_result->scan_rst.bda, ESP_BD_ADDR_LEN);
                        creat_conn_params.remote_addr_type = scan_result->scan_rst.ble_addr_type;
                        creat_conn_params.own_addr_type = BLE_ADDR_TYPE_PUBLIC;
                        creat_conn_params.is_direct = true;
                        creat_conn_params.is_aux = false;
                        creat_conn_params.phy_mask = 0x0;
                        esp_ble_gattc_enh_open(gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
                                               &creat_conn_params);
                    }
                }
            }
            break;
        case ESP_GAP_SEARCH_INQ_CMPL_EVT:
            break;
        default:
            break;
        }
        break;
    }

    case ESP_GAP_BLE_SCAN_STOP_COMPLETE_EVT:
        if (param->scan_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Scanning stop failed, status %x", param->scan_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Scanning stop successfully");
        break;

    case ESP_GAP_BLE_ADV_STOP_COMPLETE_EVT:
        if (param->adv_stop_cmpl.status != ESP_BT_STATUS_SUCCESS)
        {
            ESP_LOGE(TAG, "Advertising stop failed, status %x", param->adv_stop_cmpl.status);
            break;
        }
        ESP_LOGI(TAG, "Advertising stop successfully");
        break;
    case ESP_GAP_BLE_UPDATE_CONN_PARAMS_EVT:
        ESP_LOGI(TAG, "Connection params update, status %d, conn_int %d, latency %d, timeout %d",
                 param->update_conn_params.status,
                 param->update_conn_params.conn_int,
                 param->update_conn_params.latency,
                 param->update_conn_params.timeout);
        break;
    case ESP_GAP_BLE_SET_PKT_LENGTH_COMPLETE_EVT:
        ESP_LOGI(TAG, "Packet length update, status %d, rx %d, tx %d",
                 param->pkt_data_length_cmpl.status,
                 param->pkt_data_length_cmpl.params.rx_len,
                 param->pkt_data_length_cmpl.params.tx_len);
        break;
    default:
        break;
    }
}

// Main GATT handler
static void esp_gattc_cb(esp_gattc_cb_event_t event, esp_gatt_if_t gattc_if, esp_ble_gattc_cb_param_t *param)
{
    // ESP_LOGI(TAG, "esp_gattc_cb() called");
    /* If event is register event, store the gattc_if for each profile */
    if (event == ESP_GATTC_REG_EVT)
    {
        if (param->reg.status == ESP_GATT_OK)
        {
            gl_profile_tab[param->reg.app_id].gattc_if = gattc_if;
        }
        else
        {
            ESP_LOGI(TAG, "reg app failed, app_id %04x, status %d",
                     param->reg.app_id,
                     param->reg.status);
            return;
        }
    }

    /* If the gattc_if equal to profile A, call profile A cb handler,
     * so here call each profile's callback */
    do
    {
        int idx;
        for (idx = 0; idx < PROFILE_NUM; idx++)
        {
            if (gattc_if == ESP_GATT_IF_NONE || /* ESP_GATT_IF_NONE, not specify a certain gatt_if, need to call every profile cb function */
                gattc_if == gl_profile_tab[idx].gattc_if)
            {
                if (gl_profile_tab[idx].gattc_cb)
                {
                    gl_profile_tab[idx].gattc_cb(event, gattc_if, param);
                }
            }
        }
    } while (0);
}

void bt_connect_scale(void)
{
    if (gl_profile_tab[0].gattc_if != ESP_GATT_IF_NONE)
    {
        esp_err_t ret = esp_ble_gattc_open(
            gl_profile_tab[0].gattc_if,
            gl_profile_tab[0].remote_bda,
            BLE_ADDR_TYPE_PUBLIC,
            true);
        if (ret == ESP_OK)
        {
            connect = true;
        }
        return;
    }

    // Initialize NVS.
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
    ESP_ERROR_CHECK(esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT));

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    ret = esp_bt_controller_init(&bt_cfg);
    if (ret)
    {
        ESP_LOGE(TAG, "%s initialize controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret)
    {
        ESP_LOGE(TAG, "%s enable controller failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_init();
    if (ret)
    {
        ESP_LOGE(TAG, "%s init bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    ret = esp_bluedroid_enable();
    if (ret)
    {
        ESP_LOGE(TAG, "%s enable bluetooth failed: %s", __func__, esp_err_to_name(ret));
        return;
    }

    // Note: Avoid performing time-consuming operations within callback functions.
    // Register the callback function to the gap module
    ret = esp_ble_gap_register_callback(esp_gap_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "%s gap register failed, error code = %x", __func__, ret);
        return;
    }

    // Register the callback function to the gattc module
    ret = esp_ble_gattc_register_callback(esp_gattc_cb);
    if (ret)
    {
        ESP_LOGE(TAG, "%s gattc register failed, error code = %x", __func__, ret);
        return;
    }

    ret = esp_ble_gattc_app_register(PROFILE_A_APP_ID);
    if (ret)
    {
        ESP_LOGE(TAG, "%s gattc app register failed, error code = %x", __func__, ret);
    }

    esp_err_t local_mtu_ret = esp_ble_gatt_set_local_mtu(500);
    if (local_mtu_ret)
    {
        ESP_LOGE(TAG, "set local  MTU failed, error code = %x", local_mtu_ret);
    }

    ret = esp_ble_gap_set_scan_params(&ble_scan_params);
    if (ret)
    {
        ESP_LOGE(TAG, "Set scan params failed: %s", esp_err_to_name(ret));
        return;
    }

    /*
     * This code is intended for debugging and prints all HCI data.
     * To enable it, turn on the "BT_HCI_LOG_DEBUG_EN" configuration option.
     * The output HCI data can be parsed using the script:
     * esp-idf/tools/bt/bt_hci_to_btsnoop.py.
     * For detailed instructions, refer to esp-idf/tools/bt/README.md.
     */

    /*
    while (1) {
        extern void bt_hci_log_hci_data_show(void);
        extern void bt_hci_log_hci_adv_show(void);
        bt_hci_log_hci_data_show();
        bt_hci_log_hci_adv_show();
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
    */
}

void disconnect_from_scale(void)
{
    if (connect)
    {
        ESP_LOGI(TAG, "Disconnecting from device...");

        esp_err_t ret = esp_ble_gattc_close(
            gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
            gl_profile_tab[PROFILE_A_APP_ID].conn_id);

        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "Disconnect failed: %s", esp_err_to_name(ret));
        }
        else
        {
            ESP_LOGI(TAG, "Disconnect request sent");
        }
    }
    else
    {
        ESP_LOGW(TAG, "Not connected to any device");
    }
}

void write_char(uint8_t write_data[])
{
    ESP_LOGI(TAG, "sizeof scale commands: %d", sizeof(write_data));
    esp_err_t ret = esp_ble_gattc_write_char(
        gl_profile_tab[PROFILE_A_APP_ID].gattc_if,
        gl_profile_tab[PROFILE_A_APP_ID].conn_id,
        gl_profile_tab[PROFILE_A_APP_ID].set_char_handle,
        7, // don't use sizeof(write_data) here because it will be 4 (sizeof pointer type)
        write_data,
        ESP_GATT_WRITE_TYPE_RSP,
        ESP_GATT_AUTH_REQ_NONE);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "Write to set charact with handle %d failed", gl_profile_tab[PROFILE_A_APP_ID].set_char_handle);
    }
}

void scale_tare()
{
    ESP_LOGI(TAG, "Sending TARE to scale");
    write_char(tare);
}

void scale_led_on_gram()
{
    ESP_LOGI(TAG, "Sending LED_ON_GRAM to scale");
    write_char(led_on_gram);
}

void scale_led_on_ounce()
{
    ESP_LOGI(TAG, "Sending LED_ON_OUNCE to scale");
    write_char(led_on_ounce);
}

void scale_led_off()
{
    ESP_LOGI(TAG, "Sending LED_OFF to scale");
    write_char(led_off);
}

void scale_power_off()
{
    ESP_LOGI(TAG, "Sending POWER_OFF to scale");
    write_char(power_off);
}

void scale_timer_on()
{
    ESP_LOGI(TAG, "Sending TIMER ON to scale");
    write_char(timer_on);
}

void scale_timer_off()
{
    ESP_LOGI(TAG, "Sending TIMER OFF to scale");
    write_char(timer_off);
}

void scale_timer_reset()
{
    ESP_LOGI(TAG, "Sending TIMER RESET to scale");
    write_char(timer_reset);
}

void set_unit(enum unit_t unit)
{
    if (unit == GRAM)
    {
        scale_led_on_gram();
    }
    else
    {
        scale_led_on_ounce();
    }
}