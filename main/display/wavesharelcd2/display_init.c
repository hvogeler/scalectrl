#include "display_init.h"
#include <string.h>

#include "esp_timer.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_touch_cst816s.h"
#include "esp_lvgl_port.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "driver/i2c_master.h"
#include "driver/ledc.h"

#include "esp_log.h"
#include "esp_check.h"
#include "esp_heap_caps.h"

#define GPIO_PIN_NUM_SCLK 39
#define GPIO_PIN_NUM_MOSI 38
#define GPIO_PIN_NUM_MISO 40

#define SPI_HOST SPI2_HOST

#define I2C_NUM 0
#define GPIO_PIN_NUM_I2C_SDA 48
#define GPIO_PIN_NUM_I2C_SCL 47

#define LCD_PIXEL_CLOCK_HZ (40 * 1000 * 1000)

#define GPIO_PIN_NUM_LCD_DC 42
#define GPIO_PIN_NUM_LCD_RST -1
#define GPIO_PIN_NUM_LCD_CS 45

#define LCD_H_RES 320
#define LCD_V_RES 240
#define TOUCH_H_RES LCD_V_RES
#define TOUCH_V_RES LCD_H_RES

#define LCD_CMD_BITS 8
#define LCD_PARAM_BITS 8
#define GPIO_PIN_NUM_BK_LIGHT 1

#define LCD_BL_LEDC_TIMER LEDC_TIMER_0
#define LCD_BL_LEDC_MODE LEDC_LOW_SPEED_MODE
#define LCD_BL_LEDC_CHANNEL LEDC_CHANNEL_0
#define LCD_BL_LEDC_DUTY_RES LEDC_TIMER_10_BIT
#define LCD_BL_LEDC_DUTY (1024)
#define LCD_BL_LEDC_FREQUENCY (10000)

// Buffer size - use 1/8 of screen for better stability
#define LVGL_BUFFER_SIZE (LCD_H_RES * LCD_V_RES / 8)
#define EXAMPLE_LCD_DRAW_BUFF_DOUBLE (1)
#define EXAMPLE_LCD_DRAW_BUFF_HEIGHT (50)
#define EXAMPLE_LCD_BL_ON_LEVEL (1)

static const char *TAG = "display";

// Global variables

// Hardware handles
static esp_lcd_panel_io_handle_t io_handle = NULL;
static esp_lcd_panel_handle_t panel_handle;
static esp_lcd_touch_handle_t touch_handle;
static i2c_master_bus_handle_t i2c_bus_handle;

/* LVGL display and touch */
static lv_display_t *lvgl_disp = NULL;
static lv_indev_t *lvgl_touch_indev = NULL;

void display_init(void)
{
    ESP_LOGI(TAG, "Initialize SPI bus");
    spi_bus_config_t buscfg = {
        .sclk_io_num = GPIO_PIN_NUM_SCLK,
        .mosi_io_num = GPIO_PIN_NUM_MOSI,
        .miso_io_num = GPIO_PIN_NUM_MISO,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = LCD_H_RES * LCD_V_RES * sizeof(uint16_t),
    };
    ESP_ERROR_CHECK(spi_bus_initialize(SPI_HOST, &buscfg, SPI_DMA_CH_AUTO));

    // Simple SPI configuration - NO async callbacks
    esp_lcd_panel_io_spi_config_t io_config = {
        .dc_gpio_num = GPIO_PIN_NUM_LCD_DC,
        .cs_gpio_num = GPIO_PIN_NUM_LCD_CS,
        .pclk_hz = LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits = LCD_CMD_BITS,
        .lcd_param_bits = LCD_PARAM_BITS,
        .spi_mode = 0,
        .trans_queue_depth = 10,
        // No callbacks for maximum compatibility
        .on_color_trans_done = NULL,
        .user_ctx = NULL,
    };

    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi((esp_lcd_spi_bus_handle_t)SPI_HOST, &io_config, &io_handle));

    esp_lcd_panel_dev_config_t panel_config = {
        .reset_gpio_num = GPIO_PIN_NUM_LCD_RST,
        .rgb_ele_order = LCD_RGB_ELEMENT_ORDER_RGB,
        .bits_per_pixel = 16,
        .data_endian = LCD_RGB_DATA_ENDIAN_BIG,
    };

    ESP_LOGI(TAG, "Install ST7789 panel driver");
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(io_handle, &panel_config, &panel_handle));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(panel_handle));
    ESP_ERROR_CHECK(esp_lcd_panel_init(panel_handle));
    // ESP_ERROR_CHECK(esp_lcd_panel_mirror(panel_handle, true, false));
    // ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(panel_handle, true));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(panel_handle, true));

    ESP_LOGI(TAG, "Display initialized");
}

void touch_init(void)
{
    esp_lcd_panel_io_handle_t tp_io_handle = NULL;

    ESP_LOGI(TAG, "Initialize I2C master bus");

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = I2C_NUM,
        .scl_io_num = GPIO_PIN_NUM_I2C_SCL,
        .sda_io_num = GPIO_PIN_NUM_I2C_SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,
    };

    ESP_ERROR_CHECK(i2c_new_master_bus(&i2c_mst_config, &i2c_bus_handle));

    ESP_LOGI(TAG, "Initialize touch panel IO");

    const esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_CST816S_CONFIG();
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_i2c(i2c_bus_handle, &tp_io_config, &tp_io_handle));

    esp_lcd_touch_config_t tp_cfg = {
        .x_max = TOUCH_H_RES,
        .y_max = TOUCH_V_RES,
        .rst_gpio_num = -1,
        .int_gpio_num = -1,
        .flags = {
            .swap_xy = 1,
            .mirror_x = 1,
            .mirror_y = 0,
        },
    };

    ESP_LOGI(TAG, "Initialize touch controller CST816S");
    ESP_ERROR_CHECK(esp_lcd_touch_new_i2c_cst816s(tp_io_handle, &tp_cfg, &touch_handle));

    ESP_LOGI(TAG, "Touch controller initialized");
}

esp_err_t lvgl_init(void)
{
    /* Initialize LVGL */
    const lvgl_port_cfg_t lvgl_cfg = {
        .task_priority = 4,       /* LVGL task priority */
        .task_stack = 1024 * 24,  /* LVGL task stack size */
        .task_affinity = -1,      /* LVGL task pinned to core (-1 is no affinity) */
        .task_max_sleep_ms = 500, /* Maximum sleep in LVGL task */
        .timer_period_ms = 5      /* LVGL timer tick period in ms */
    };
    ESP_RETURN_ON_ERROR(lvgl_port_init(&lvgl_cfg), TAG, "LVGL port initialization failed");

    /* Add LCD screen */
    ESP_LOGD(TAG, "Add LCD screen");
    const lvgl_port_display_cfg_t disp_cfg = {
        .io_handle = io_handle,
        .panel_handle = panel_handle,
        .buffer_size = LVGL_BUFFER_SIZE * sizeof(lv_color_t),
        .double_buffer = EXAMPLE_LCD_DRAW_BUFF_DOUBLE,
        .hres = LCD_H_RES,
        .vres = LCD_V_RES,
        .monochrome = false,
        .color_format = LV_COLOR_FORMAT_RGB565,
        .rotation = {
            .swap_xy = true,
            .mirror_x = true,
            .mirror_y = false,
        },
        .flags = {
            .buff_dma = true,
            .swap_bytes = true,
        }};
    lvgl_disp = lvgl_port_add_disp(&disp_cfg);

    /* Add touch input (for selected screen) */
    const lvgl_port_touch_cfg_t touch_cfg = {
        .disp = lvgl_disp,
        .handle = touch_handle,
    };
    lvgl_touch_indev = lvgl_port_add_touch(&touch_cfg);

    return ESP_OK;
}

void bsp_brightness_init(void)
{
    ESP_LOGI(TAG, "Initialize backlight");

    gpio_set_direction(GPIO_PIN_NUM_BK_LIGHT, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_PIN_NUM_BK_LIGHT, 1);

    ledc_timer_config_t ledc_timer = {
        .speed_mode = LCD_BL_LEDC_MODE,
        .timer_num = LCD_BL_LEDC_TIMER,
        .duty_resolution = LCD_BL_LEDC_DUTY_RES,
        .freq_hz = LCD_BL_LEDC_FREQUENCY,
        .clk_cfg = LEDC_AUTO_CLK};
    ESP_ERROR_CHECK(ledc_timer_config(&ledc_timer));

    ledc_channel_config_t ledc_channel = {
        .speed_mode = LCD_BL_LEDC_MODE,
        .channel = LCD_BL_LEDC_CHANNEL,
        .timer_sel = LCD_BL_LEDC_TIMER,
        .intr_type = LEDC_INTR_DISABLE,
        .gpio_num = GPIO_PIN_NUM_BK_LIGHT,
        .duty = 0,
        .hpoint = 0};
    ESP_ERROR_CHECK(ledc_channel_config(&ledc_channel));
}

void bsp_brightness_set_level(uint8_t level)
{
    if (level > 100)
    {
        ESP_LOGE(TAG, "Brightness out of range");
        return;
    }

    uint32_t duty = (level * (LCD_BL_LEDC_DUTY - 1)) / 100;
    ESP_ERROR_CHECK(ledc_set_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LCD_BL_LEDC_MODE, LCD_BL_LEDC_CHANNEL));

    ESP_LOGI(TAG, "Brightness set to %d%%", level);
}

void lcd_off()
{
    // esp_lcd_panel_disp_on_off(panel_handle, false);
    esp_lcd_panel_disp_sleep(panel_handle, true);
    esp_lcd_touch_enter_sleep(touch_handle);
}