#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/queue.h>

enum LcdDisplayState {
    LCD_NORMAL,
    LCD_WARNING,
    LCD_CRITICAL,
    LCD_ERROR
};

enum SensorRunMode {
    SENSOR_MODE_NORMAL,
    SENSOR_MODE_FIXED_24_30,
    SENSOR_MODE_FIXED_30_50,
    SENSOR_MODE_FIXED_30_70
};

// struct TinyMLInputSample {
//     float temperature;
//     float humidity;
//     uint32_t tick;
// };

struct SensorData {
    float temperature;
    float humidity;

    uint32_t lastSensorUpdateTick;
    LcdDisplayState currentLcdState; 
    SensorRunMode sensorRunMode;

    // --- THÊM: CÁC NGƯỠNG CÀI ĐẶT ĐỘNG (DYNAMIC THRESHOLDS) ---
    float tempWarning;   // Ngưỡng cảnh báo nhiệt độ
    float tempCritical;  // Ngưỡng nguy hiểm nhiệt độ
    float humDry;        // Ngưỡng độ ẩm khô hanh
    float humDamp;       // Ngưỡng độ ẩm ẩm ướt
    float humCritical;   // Ngưỡng độ ẩm gây ngạt

    // --- MANUAL LED OVERRIDE (từ RPC/Dashboard) ---
    bool manualLedOverride;      // Cờ cho biết có override từ RPC không
    bool manualLedState;         // Trạng thái LED khi override (ON/OFF)
    uint32_t lastManualLedTick;  // Timestamp lần cuối RPC gửi lệnh (timeout 10s)

    SemaphoreHandle_t dataMutex; 
    SemaphoreHandle_t i2cMutex;
    SemaphoreHandle_t tempWarningSemaphore; 
    SemaphoreHandle_t lcdUpdateSemaphore; 
    QueueHandle_t tinymlInputQueue;
};

#endif