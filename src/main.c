/* ESP32 RetroTerminal v0.1.19: HC-05 UART1 <-> UART2, I80/SPI display profiles.
 * GPIO16/17: PSRAM. UART0: USB console. GPIO2: debug LED.
 * Native Bluetooth and Wi-Fi are excluded from this firmware. */

#include "sdkconfig.h"
#if defined(CONFIG_BT_ENABLED) || defined(CONFIG_ESP_WIFI_ENABLED)
#error "Native Bluetooth/Wi-Fi must not be included in the HC-05 firmware"
#endif

#include "esp_log.h"
#include "esp_app_desc.h"
#include "display_profile.h"
#include "debug_led.h"
#include "hc05_bridge.h"
#include "terminal_view.h"
#include "tft_display.h"

void app_main(void)
{
    ESP_LOGI("Terminal", "app_main v%s: profile=%d touch=%d; HC-05=%d; internal Bluetooth disabled",
             esp_app_get_description()->version, RETRO_TFT_PROFILE, RETRO_TOUCH_ENABLE,
             CONFIG_RETRO_HC05_BAUD);
    ESP_ERROR_CHECK(debug_led_init(hc05_bridge_led_mode));
    ESP_ERROR_CHECK(terminal_view_init(tft_display_enabled() ? tft_display_cols() : 80,
                                      tft_display_enabled() ? tft_display_rows() : 24));
    ESP_ERROR_CHECK(hc05_bridge_init());
    ESP_ERROR_CHECK(tft_display_start());
}
