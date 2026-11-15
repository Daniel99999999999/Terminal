idf_component_register(
    SRCS "main.c" "app_health.c" "uart_input.c" "screen.c" "lcd_panel.c"
         "ansi_parser.c" "terminal.c" "terminal_raster.c" "xpt2046.c"
         "touch_calibration.c" "touch_ui.c"
         "../vendor/esp_lcd_ili9341/esp_lcd_ili9341.c"
    INCLUDE_DIRS "." "../vendor/esp_lcd_ili9341/include"
    PRIV_REQUIRES esp_driver_uart esp_driver_gpio esp_driver_spi esp_psram esp_timer esp_lcd nvs_flash
)
set_source_files_properties("../vendor/esp_lcd_ili9341/esp_lcd_ili9341.c"
    PROPERTIES COMPILE_DEFINITIONS
    "ESP_LCD_ILI9341_VER_MAJOR=2;ESP_LCD_ILI9341_VER_MINOR=1;ESP_LCD_ILI9341_VER_PATCH=0")
