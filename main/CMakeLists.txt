# idf_component_register(SRCS "main.c"
#                     INCLUDE_DIRS ".")


idf_component_register(
    SRCS 
      "main.c"
      "display/wavesharelcd2/display_init.c"
      "lvgl/lvgl_init.c"
      "ui/view01.c"
      "ui/fonts/roboto_bold_40.c"      
      "ui/fonts/roboto_bold_60.c"      
      "ui/fonts/roboto_regular_20.c"      
      "scale/ble.c"
    INCLUDE_DIRS 
      "."
      "ui/fonts"
    REQUIRES 
        lvgl 
        esp_timer 
        driver 
        esp_lcd 
        esp_lcd_touch_cst816s
        nvs_flash
        bt
)