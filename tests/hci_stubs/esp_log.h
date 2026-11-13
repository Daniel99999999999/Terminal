#pragma once
void test_hci_log(const char *tag, const char *format, ...);
#define ESP_LOGI(tag, ...) test_hci_log(tag, __VA_ARGS__)
