#pragma once
#include <stdint.h>
#include "esp_err.h"
typedef struct {
    void (*notify_host_send_available)(void);
    int (*notify_host_recv)(uint8_t *, uint16_t);
} esp_vhci_host_callback_t;
