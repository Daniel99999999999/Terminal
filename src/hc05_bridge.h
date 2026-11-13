#pragma once
#include <stddef.h>
#include "esp_err.h"
#include "debug_led_logic.h"
#include "hc05_link.h"

#define HC05_PACKET_BYTES 128
esp_err_t hc05_bridge_init(void);
hc05_status_t hc05_bridge_status(void);
debug_led_mode_t hc05_bridge_led_mode(void);
/* Nonblocking requests. KEY must be explicitly enabled after hardware checks.
 * AT requests pause the bridge and switch UART1 baud, never UART2. They cannot
 * power-cycle HC-05 or confirm it has entered AT mode; check its OK response. */
esp_err_t hc05_bridge_request_at(bool enable);
esp_err_t hc05_bridge_at_send(const uint8_t *data, size_t length);
esp_err_t hc05_bridge_at_receive(uint8_t *data, size_t capacity, size_t *length);
