idf_component_register(
    SRCS "main.c" "debug_led.c" "debug_led_logic.c"
         "ansi_parser.c" "terminal.c" "terminal_view.c"
         "hc05_link.c" "hc05_bridge.c" "terminal_raster.c" "tft_display.c"
         "../vendor/esp_lcd_ili9341/esp_lcd_ili9341.c"
    INCLUDE_DIRS "." "../vendor/esp_lcd_ili9341/include"
    PRIV_REQUIRES esp_driver_uart esp_driver_gpio esp_psram esp_timer esp_lcd
)

# Vendored Espressif driver 2.1.0 normally gets these from cmake_utilities.
# Keep its sources unchanged and supply the same version locally, offline.
set_source_files_properties("../vendor/esp_lcd_ili9341/esp_lcd_ili9341.c"
    PROPERTIES COMPILE_DEFINITIONS
    "ESP_LCD_ILI9341_VER_MAJOR=2;ESP_LCD_ILI9341_VER_MINOR=1;ESP_LCD_ILI9341_VER_PATCH=0")

# Mark controlled aborts (including ESP_ERROR_CHECK) before the panic handler.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_system_abort")
