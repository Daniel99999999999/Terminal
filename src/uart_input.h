#pragma once
#include "esp_err.h"
esp_err_t uart_input_start(void);
void uart_input_poll(void);
void uart_input_log(void);
