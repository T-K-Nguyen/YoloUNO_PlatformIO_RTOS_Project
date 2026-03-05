#include <Arduino.h>
#include "shared_data.h"
#include "task_sensor.h"
#include "task_actuator.h"

void setup() {
    Serial.begin(115200);
    Serial.println("Khoi tao he thong RTOS...");

    // 1. Cấp phát vùng nhớ cho Struct
    SensorData* sharedData = new SensorData();
    sharedData->temperature = 0.0f;
    sharedData->humidity = 0.0f;

    // 2. Khởi tạo các Semaphore và Mutex
    sharedData->dataMutex = xSemaphoreCreateMutex();
    sharedData->i2cMutex = xSemaphoreCreateMutex(); 
    sharedData->tempWarningSemaphore = xSemaphoreCreateBinary();

    // 3. Tạo Task 
    if (sharedData->dataMutex != NULL && sharedData->i2cMutex != NULL) {
        // Tạo Task Cảm biến
        xTaskCreate(TaskSensor, "Sensor_Task", 4096, (void*)sharedData, 3, NULL);
        
        // Tạo Task 1: LED Control
        xTaskCreate(TaskLEDControl, "LED_Task", 2048, (void*)sharedData, 2, NULL);

        // Tạo Task 2: NeoPixel
        xTaskCreate(neo_blinky, "Neo_Task", 2048, (void*)sharedData, 2, NULL);
    }
}

void loop() {
    // Dọn dẹp task loop để tiết kiệm tài nguyên
    vTaskDelete(NULL); 
}