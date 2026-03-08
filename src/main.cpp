#include <Arduino.h>
#include <Wire.h>
#include "shared_data.h"
#include "task_sensor.h"
#include "task_actuator.h"
#include "task_lcd.h"

void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Khoi tao he thong RTOS...");

    Wire.begin(GPIO_NUM_11, GPIO_NUM_12);

    // 1. Cấp phát vùng nhớ cho Struct
    SensorData* sharedData = new SensorData();
    sharedData->temperature = 0.0f;
    sharedData->humidity = 0.0f;
    sharedData->lastSensorUpdateTick = xTaskGetTickCount();
    sharedData->currentLcdState = LCD_NORMAL;

    // --- THÊM: KHỞI TẠO NGƯỠNG MẶC ĐỊNH ---
    sharedData->tempWarning = 25.0f;
    sharedData->tempCritical = 35.0f;
    sharedData->humDry = 40.0f;
    sharedData->humDamp = 70.0f;
    sharedData->humCritical = 85.0f;
    // --------------------------------------

    // 2. Khởi tạo các Semaphore và Mutex
    sharedData->dataMutex = xSemaphoreCreateMutex();
    sharedData->i2cMutex = xSemaphoreCreateMutex(); 
    sharedData->tempWarningSemaphore = xSemaphoreCreateBinary();
    sharedData->lcdUpdateSemaphore = xSemaphoreCreateBinary();

    // 3. Tạo Task 
    if (sharedData->dataMutex != NULL && sharedData->i2cMutex != NULL) {
        // Tạo Task Cảm biến
        xTaskCreate(TaskSensor, "Sensor_Task", 4096, (void*)sharedData, 3, NULL);
        
        // Tạo Task 1: LED Control
        xTaskCreate(TaskLEDControl, "LED_Task", 2048, (void*)sharedData, 2, NULL);

        // Tạo Task 2: NeoPixel
        xTaskCreate(neo_blinky, "Neo_Task", 2048, (void*)sharedData, 2, NULL);
    }
    if (sharedData->lcdUpdateSemaphore != NULL) {
        xTaskCreate(TaskLCD, "LCD_Task", 4096, (void*)sharedData, 2, NULL);
    }
}

void loop() {
    // Dọn dẹp task loop để tiết kiệm tài nguyên
    vTaskDelete(NULL); 
}