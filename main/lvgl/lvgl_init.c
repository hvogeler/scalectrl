#include <string.h>
#include "lvgl.h"
#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "display/wavesharelcd2/display_init.h"

#define LVGL_TICK_PERIOD_MS 2
#define LVGL_TASK_MAX_DELAY_MS 500
#define LVGL_TASK_MIN_DELAY_MS 1


// Buffer size - use 1/8 of screen for better stability
#define LVGL_BUFFER_SIZE (LCD_H_RES * LCD_V_RES / 8)

static SemaphoreHandle_t lvgl_api_mux = NULL;
static lv_display_t *display = NULL;
static lv_indev_t *indev_touchpad = NULL;

// Display buffers - make them global to avoid stack issues
static lv_color_t *disp_buf1 = NULL;
static lv_color_t *disp_buf2 = NULL;

static const char *TAG = "lvglinit";

void lv_port_disp_init(void);
void lv_port_indev_init(void);
void lvgl_tick_timer_init(uint32_t);

void lvgl_init()
{
    ESP_LOGI(TAG, "Starting LVGL %d.%d.%d on Waveshare Touch LCD 2", lv_version_major(), lv_version_minor(), lv_version_patch());

    // Create LVGL mutex
    lvgl_api_mux = xSemaphoreCreateRecursiveMutex();
    if (lvgl_api_mux == NULL)
    {
        ESP_LOGE(TAG, "Failed to create LVGL mutex");
        return;
    }

    // Initialize LVGL
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    lvgl_tick_timer_init(LVGL_TICK_PERIOD_MS);
    ESP_LOGI(TAG, "LVGL %d.%d.%d initialized", lv_version_major(), lv_version_minor(), lv_version_patch());
}

void lv_port_disp_init(void)
{
    ESP_LOGI(TAG, "Initialize LVGL 9.3 display");
    
    // Create display
    display = lv_display_create(LCD_H_RES, LCD_V_RES);
    if (display == NULL) {
        ESP_LOGE(TAG, "Failed to create LVGL display");
        return;
    }
    
    // Allocate buffers in DMA-capable memory
    size_t buffer_size = LVGL_BUFFER_SIZE * sizeof(lv_color_t);
    
    disp_buf1 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (disp_buf1 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate display buffer 1 in DMA memory");
        return;
    }
    
    disp_buf2 = heap_caps_malloc(buffer_size, MALLOC_CAP_DMA | MALLOC_CAP_INTERNAL);
    if (disp_buf2 == NULL) {
        ESP_LOGE(TAG, "Failed to allocate display buffer 2 in DMA memory");
        heap_caps_free(disp_buf1);
        return;
    }
    
    // Clear the buffers
    memset(disp_buf1, 0, buffer_size);
    memset(disp_buf2, 0, buffer_size);
    
    // Set the buffers
    lv_display_set_buffers(display, disp_buf1, disp_buf2, buffer_size, LV_DISPLAY_RENDER_MODE_PARTIAL);
    
    // Set the flush callback
    lv_display_set_flush_cb(display, lvgl_flush_cb);
    
    ESP_LOGI(TAG, "LVGL display initialized (buffer size: %zu bytes each)", buffer_size);
}

void lv_port_indev_init(void)
{
    ESP_LOGI(TAG, "Initialize LVGL input device");
    
    indev_touchpad = lv_indev_create();
    if (indev_touchpad == NULL) {
        ESP_LOGE(TAG, "Failed to create input device");
        return;
    }
    
    lv_indev_set_type(indev_touchpad, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev_touchpad, lvgl_touch_cb);
    
    ESP_LOGI(TAG, "LVGL input device initialized");
}

static void increase_lvgl_tick(void *arg)
{
    lv_tick_inc(LVGL_TICK_PERIOD_MS);
}

void lvgl_tick_timer_init(uint32_t ms)
{
    ESP_LOGI(TAG, "Initialize LVGL tick timer");
    
    const esp_timer_create_args_t lvgl_tick_timer_args = {
        .callback = &increase_lvgl_tick,
        .name = "lvgl_tick"
    };
    esp_timer_handle_t lvgl_tick_timer = NULL;
    ESP_ERROR_CHECK(esp_timer_create(&lvgl_tick_timer_args, &lvgl_tick_timer));
    ESP_ERROR_CHECK(esp_timer_start_periodic(lvgl_tick_timer, ms * 1000));
}


// Mutex functions
bool lvgl_lock(int timeout_ms)
{
    const TickType_t timeout_ticks = (timeout_ms == -1) ? portMAX_DELAY : pdMS_TO_TICKS(timeout_ms);
    return xSemaphoreTakeRecursive(lvgl_api_mux, timeout_ticks) == pdTRUE;
}

void lvgl_unlock(void)
{
    xSemaphoreGiveRecursive(lvgl_api_mux);
}

void lvgl_task(void *param)
{
    ESP_LOGI(TAG, "LVGL task started");

    while (1)
    {
        uint32_t task_delay_ms = LVGL_TASK_MAX_DELAY_MS;

        if (lvgl_lock(-1))
        {
            task_delay_ms = lv_timer_handler();
            lvgl_unlock();
        }

        if (task_delay_ms > LVGL_TASK_MAX_DELAY_MS)
        {
            task_delay_ms = LVGL_TASK_MAX_DELAY_MS;
        }
        else if (task_delay_ms < LVGL_TASK_MIN_DELAY_MS)
        {
            task_delay_ms = LVGL_TASK_MIN_DELAY_MS;
        }

        vTaskDelay(pdMS_TO_TICKS(task_delay_ms));
    }
}