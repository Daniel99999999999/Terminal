#pragma once

#include "esp_err.h"

#ifndef BT_HCI_SCAN_DIAG
#define BT_HCI_SCAN_DIAG 1
#endif

/* Call before starting Bluetooth/tasks. Poll from the existing UART diagnostics. */
esp_err_t bt_hci_diag_init(void);
void bt_hci_diag_poll(void);
