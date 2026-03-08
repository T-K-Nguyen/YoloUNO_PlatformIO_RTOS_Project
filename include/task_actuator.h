#ifndef TASK_ACTUATOR_H
#define TASK_ACTUATOR_H

#include <Arduino.h>
#include "shared_data.h"

#define NEO_PIN 45
#define LED_COUNT 1

void TaskLEDControl(void *pvParameters);
void neo_blinky(void *pvParameters);

#endif