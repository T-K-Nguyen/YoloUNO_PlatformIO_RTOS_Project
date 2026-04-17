#include <Arduino.h>
#include <Wire.h>
#include "shared_data.h"
#include "task_sensor.h"
#include "task_actuator.h"
#include "task_lcd.h"
#include "tinyml.h"

void setup() {
    Serial.begin(115200);
    Wire.begin(GPIO_NUM_11, GPIO_NUM_12); 

    SensorData* sharedData = new SensorData();
    sharedData->temperature = 0.0f;
    sharedData->humidity = 0.0f;
    sharedData->lastSensorUpdateTick = xTaskGetTickCount();
    sharedData->currentLcdState = LCD_NORMAL;
    
    // Ngưỡng cấu hình động
    sharedData->tempWarning = 25.0f;
    sharedData->tempCritical = 35.0f;
    sharedData->humDry = 40.0f;
    sharedData->humDamp = 70.0f;
    sharedData->humCritical = 85.0f;

    sharedData->dataMutex = xSemaphoreCreateMutex();
    sharedData->i2cMutex = xSemaphoreCreateMutex(); 
    sharedData->tempWarningSemaphore = xSemaphoreCreateBinary();
    sharedData->lcdUpdateSemaphore = xSemaphoreCreateBinary();

    if (sharedData->dataMutex != NULL && sharedData->i2cMutex != NULL) {
        
        // Task Sensor (Ưu tiên cao nhất để không lỡ nhịp dữ liệu)
        xTaskCreate(TaskSensor, "Sensor_Task", 4096, (void*)sharedData, 4, NULL);
        
        // Các Task điều khiển phần cứng (Ưu tiên trung bình)
        xTaskCreate(TaskLEDControl, "LED_Task", 2048, (void*)sharedData, 3, NULL);
        xTaskCreate(neo_blinky, "NeoPixel_Task", 2048, (void*)sharedData, 3, NULL);
        xTaskCreate(TaskLCD, "LCD_Task", 4096, (void*)sharedData, 3, NULL);
        
        // --- THÊM TASK 5: TINYML ANOMALY DETECTION ---
        // Cấp phát 8192 bytes RAM (TensorFlow ngốn khá nhiều RAM)
        // Mức ưu tiên thấp nhất (Priority 1) vì AI chạy tốn CPU, không được làm nghẽn các Task khác
        xTaskCreate(tiny_ml_task, "TinyML_Task", 8192, (void*)sharedData, 1, NULL);
        
        Serial.println("Tao Task thanh cong! He thong dang chay...");
    }
}

void loop() {
    vTaskDelete(NULL); 
}