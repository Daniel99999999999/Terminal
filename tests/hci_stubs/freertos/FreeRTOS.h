#pragma once
#include <stdint.h>
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define pdTRUE 1
void test_hci_enter(void);
void test_hci_exit(void);
#define portENTER_CRITICAL(lock) ((void)(lock), test_hci_enter())
#define portEXIT_CRITICAL(lock) ((void)(lock), test_hci_exit())
