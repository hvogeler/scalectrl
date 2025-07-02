#include "freertos/FreeRTOS.h"
#include "esp_log.h"
#include "timer_ctrl.h"
#include "scale/ble.h"
#include "../view01.h"

static const char *TAG = "timer_ctrl";

static int seconds = 0;
static timer_state_t timer_state = STOPPED;
static SemaphoreHandle_t timer_mutex = NULL;

void init_timer()
{
    if (timer_mutex == NULL)
    {
        timer_mutex = xSemaphoreCreateMutex();
    }
}

timer_state_t get_timer_state()
{
    timer_state_t retval = STOPPED;

    init_timer();
    if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        retval = timer_state;
        xSemaphoreGive(timer_mutex);
    }
    return retval;
}

void timer_reset_cb()
{
    init_timer();
    if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        if (timer_state == STOPPED)
        {
            ESP_LOGI(TAG, "TIMER Reset");
            seconds = 0;
            set_timer(seconds);
            scale_timer_reset();
            scale_timer_off();
        }
        xSemaphoreGive(timer_mutex);
    }
}

void timer_start_stop_cb()
{
    init_timer();
    if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        switch (timer_state)
        {
        case RUNNING:
            ESP_LOGI(TAG, "TIMER Stop");
            scale_timer_off();
            timer_state = STOPPED;
            break;
        case STOPPED:
            ESP_LOGI(TAG, "TIMER Start");
            scale_timer_on();
            timer_state = RUNNING;
            break;
        }
        xSemaphoreGive(timer_mutex);
    }
}

void timer_task(void *params)
{
    ESP_LOGI(TAG, "Starting Timer Task");
    init_timer();

    TickType_t xLastWakeTime;
    const TickType_t xFrequency = pdMS_TO_TICKS(1000); // 1000ms = 1 second

    // Initialize the xLastWakeTime variable with the current time
    xLastWakeTime = xTaskGetTickCount();

    while (1)
    {
        if (xSemaphoreTake(timer_mutex, pdMS_TO_TICKS(100)) == pdTRUE)
        {
            if (timer_state == RUNNING)
            {
                // Increment counter
                seconds++;
                set_timer(seconds);
            }
            xSemaphoreGive(timer_mutex);
        }
        // Wait for the next cycle (1 second)
        vTaskDelayUntil(&xLastWakeTime, xFrequency);
    }
}