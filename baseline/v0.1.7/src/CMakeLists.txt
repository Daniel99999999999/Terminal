idf_component_register(
    SRCS "main.c" "bt_tx_state.c" "debug_led.c" "debug_led_logic.c" "bt_discovery_diag.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES bt nvs_flash esp_driver_uart esp_driver_gpio esp_psram esp_timer
)

# Mark controlled aborts (including ESP_ERROR_CHECK) before the panic handler.
target_link_options(${COMPONENT_LIB} INTERFACE "-Wl,--wrap=esp_system_abort")
