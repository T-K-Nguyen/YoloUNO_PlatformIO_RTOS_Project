#ifndef SHARED_DATA_H
#define SHARED_DATA_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

struct SensorData {
    float temperature;
    float humidity;

    uint32_t lastSensorUpdateTick; // Lưu lại thời điểm cuối cùng cảm biến đọc thành công

    // Mutex bảo vệ vùng nhớ dữ liệu (tránh đọc/ghi xung đột)
    SemaphoreHandle_t dataMutex; 
    
    // Mutex bảo vệ đường truyền I2C (bắt buộc khi có nhiều thiết bị I2C)
    SemaphoreHandle_t i2cMutex;

    // Semaphore kích hoạt cảnh báo nhiệt độ
    SemaphoreHandle_t tempWarningSemaphore; 
};

#endif