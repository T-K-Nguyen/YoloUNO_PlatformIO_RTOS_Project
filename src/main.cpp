#include <Arduino.h>
#include <Wire.h>
#include "shared_data.h"
#include "task_sensor.h"
#include "task_actuator.h"
#include "task_lcd.h"
#include "task_webserver.h"
#include "global.h"
#include "coreiot.h"
#include "task_check_info.h"
#include "settingWifiAp.h"
#include "task_toogle_boot.h"


void setup() {
    Serial.begin(115200);
    delay(2000);
    Serial.println("Khoi tao he thong RTOS...");

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
    // ---

    // --- KHỞI TẠO MANUAL LED OVERRIDE ---
    sharedData->manualLedOverride = false;
    sharedData->manualLedState = false;
    sharedData->lastManualLedTick = 0;
    // ---

    // 2. Khởi tạo các Semaphore và Mutex
    sharedData->dataMutex = xSemaphoreCreateMutex();
    sharedData->i2cMutex = xSemaphoreCreateMutex(); 
    sharedData->tempWarningSemaphore = xSemaphoreCreateBinary();
    sharedData->lcdUpdateSemaphore = xSemaphoreCreateBinary();
    // sharedData->tinymlInputQueue = xQueueCreate(1, sizeof(TinyMLInputSample));

    // 3. Tạo Task 
    if (sharedData->dataMutex != NULL && sharedData->i2cMutex != NULL) {
        // Tạo Task Cảm biến
        xTaskCreate(TaskSensor, "Sensor_Task", 4096, (void*)sharedData, 3, NULL);
        
        // Tạo Task 1: LED Control
        xTaskCreate(TaskLEDControl, "LED_Task", 2048, (void*)sharedData, 2, NULL);

        // Tạo Task 2: NeoPixel
        // xTaskCreate(neo_blinky, "Neo_Task", 2048, (void*)sharedData, 2, NULL);
    }

    // if (sharedData->lcdUpdateSemaphore != NULL) {
    //     xTaskCreate(TaskLCD, "LCD_Task", 4096, (void*)sharedData, 2, NULL);
    // }

    InitWebServer();
    xTaskCreate(coreiot_task, "CoreIOT_Task", 8192, (void*)sharedData, 2, NULL);
    // xTaskCreate(tiny_ml_task, "Tiny ML Task" ,2048  ,NULL  ,2 , NULL);
    xTaskCreate(Task_Toogle_BOOT, "Task_Toogle_BOOT", 4096, NULL, 2, NULL);
}

void loop() {
    // Dọn dẹp task loop để tiết kiệm tài nguyên
    vTaskDelete(NULL); 
}