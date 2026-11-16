#pragma once
#include "FreeRTOS.h"
BaseType_t xTaskCreate(void (*fn)(void *),const char *name,unsigned stack,void *arg,unsigned prio,void *handle);
void vTaskDelay(TickType_t ticks);

uint32_t ulTaskNotifyTake(BaseType_t clear,TickType_t ticks);
