#pragma once

#include "esp_gap_bt_api.h"

/* 0: observe existing identity; 1: Computer CoD only; 2: Computer + explicit EIR.
 * These are diagnostic comparisons, not a confirmed fix for the LMP timeout. */
#ifndef BT_DISCOVERY_TEST
#define BT_DISCOVERY_TEST 2
#endif
#if BT_DISCOVERY_TEST < 0 || BT_DISCOVERY_TEST > 2
#error "BT_DISCOVERY_TEST must be 0, 1 or 2"
#endif

void bt_discovery_diag_log_cod(const char *stage);
void bt_discovery_diag_after_server_start(void);
void bt_discovery_diag_eir_event(const esp_bt_gap_cb_param_t *param);
