#ifndef TASK_SENSOR_H
#define TASK_SENSOR_H

#include <Arduino.h>
#include "shared_data.h" // Chứa định nghĩa SensorData
#include "system_config.h" 

// Khai báo nguyên mẫu hàm (Function Prototype)
void TaskSensor(void *pvParameters);

#endif