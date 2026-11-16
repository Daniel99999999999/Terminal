#pragma once
#include "FreeRTOS.h"
typedef void *QueueHandle_t;
QueueHandle_t xQueueCreate(unsigned n,unsigned size);
BaseType_t xQueueSend(QueueHandle_t q,const void *p,TickType_t ticks);
BaseType_t xQueueReceive(QueueHandle_t q,void *p,TickType_t ticks);
