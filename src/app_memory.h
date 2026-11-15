#pragma once
#include "sdkconfig.h"
#include "esp_heap_caps.h"
#ifdef CONFIG_SPIRAM
#define APP_MEMORY_CAPS (MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT)
#define APP_MEMORY_NAME "PSRAM"
#else
#define APP_MEMORY_CAPS (MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)
#define APP_MEMORY_NAME "internal RAM"
#endif
