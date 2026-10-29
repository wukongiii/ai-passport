#pragma once
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
void vTaskDelay(TickType_t);
unsigned uxTaskGetStackHighWaterMark(TaskHandle_t);
unsigned ulTaskNotifyTake(int,TickType_t);
void xTaskNotifyGive(TaskHandle_t);
int xTaskCreate(void (*)(void *),const char *,unsigned,void *,unsigned,TaskHandle_t *);
