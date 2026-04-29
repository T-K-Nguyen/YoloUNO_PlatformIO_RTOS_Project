#include <Arduino.h>
#include <Wire.h>
#include "shared_data.h"
#include "task_sensor.h"
#include "task_actuator.h"
#include "task_lcd.h"
#include "tinyml.h"
#include "task_webserver.h"
#include "coreiot.h"
#include "task_check_info.h"
#include "settingWifiAp.h"
#include "task_toogle_boot.h"
#include "system_config.h"

void setup()
{
    Serial.begin(115200);
    delay(5000); // Đợi Serial ổn định
    Wire.begin(GPIO_NUM_11, GPIO_NUM_12); 

    const bool hasSavedInfo = check_info_File(false);
    bool hasWifiCredential = false;
    String bootSsid;
    String bootPass;
    WifiGetCredentials(bootSsid, bootPass);
    hasWifiCredential = bootSsid.length() > 0;

    if (hasSavedInfo && hasWifiCredential) {
        Serial.println("[BOOT] Found saved STA credentials, trying WiFi...");
        InitWifi();
    } else {
        Serial.println("[BOOT] No STA credentials saved yet. Cloud publish is waiting for WiFi config.");
    }

    // 1. Cấp phát vùng nhớ cho Struct
    SensorData *sharedData = new SensorData();
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
    // ---

    // --- KHỞI TẠO MANUAL LED OVERRIDE ---
    sharedData->manualLedOverride = false;
    sharedData->manualLedState = false;
    sharedData->lastManualLedTick = 0;
    // ---

    sharedData->dataMutex = xSemaphoreCreateMutex();
    sharedData->i2cMutex = xSemaphoreCreateMutex();
    sharedData->tempWarningSemaphore = xSemaphoreCreateBinary();
    sharedData->lcdUpdateSemaphore = xSemaphoreCreateBinary();
    // sharedData->tinymlInputQueue = xQueueCreate(1, sizeof(TinyMLInputSample));

    if (sharedData->dataMutex != NULL && sharedData->i2cMutex != NULL) {
        xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);
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

    InitWebServer((void *)sharedData);
    xTaskCreate(coreiot_task, "CoreIOT_Task", 8192, (void*)sharedData, 2, NULL);
}

void loop() {
    vTaskDelete(NULL); 
}