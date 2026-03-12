#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

enum LcdDisplayState {
    LCD_NORMAL,
    LCD_WARNING,
    LCD_CRITICAL,
    LCD_ERROR
};
struct SensorData {
    float temperature;
    float humidity;

    uint32_t lastSensorUpdateTick;
    LcdDisplayState currentLcdState; 

    // --- THÊM: CÁC NGƯỠNG CÀI ĐẶT ĐỘNG (DYNAMIC THRESHOLDS) ---
    float tempWarning;   // Ngưỡng cảnh báo nhiệt độ
    float tempCritical;  // Ngưỡng nguy hiểm nhiệt độ
    float humDry;        // Ngưỡng độ ẩm khô hanh
    float humDamp;       // Ngưỡng độ ẩm ẩm ướt
    float humCritical;   // Ngưỡng độ ẩm gây ngạt

    SemaphoreHandle_t dataMutex; 
    SemaphoreHandle_t i2cMutex;
    SemaphoreHandle_t tempWarningSemaphore; 
    SemaphoreHandle_t lcdUpdateSemaphore; 
};

#endif