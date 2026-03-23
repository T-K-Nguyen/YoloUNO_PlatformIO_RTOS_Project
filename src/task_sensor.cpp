#include "task_sensor.h"
#include <Wire.h>
#include "DHT20.h"

void TaskSensor(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    DHT20 DHT(&Wire);
    
    DHT.begin();

    while(1) {
        int dhtStatus = -1; // Biến lưu trạng thái đọc

        // 1. Lấy chìa khóa I2C trước khi giao tiếp
        if (xSemaphoreTake(data->i2cMutex, portMAX_DELAY) == pdTRUE) {
            dhtStatus = DHT.read(); // ĐỌC VÀ LƯU LẠI KẾT QUẢ
            xSemaphoreGive(data->i2cMutex);
        }

        // 2. CHỈ XỬ LÝ DỮ LIỆU KHI ĐỌC THÀNH CÔNG (status == 0)
        if (dhtStatus == 0) { 
            float currentTemp = DHT.getTemperature();
            float currentHum = DHT.getHumidity();

            Serial.print("Humidity: ");
            Serial.print(currentHum, 1);
            Serial.print("%, Temperature: ");
            Serial.println(currentTemp, 1);

            // Lấy chìa khóa Dữ Liệu để lưu kết quả an toàn
            if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
                data->temperature = currentTemp;
                data->humidity = currentHum;
                data->lastSensorUpdateTick = xTaskGetTickCount();

                // --- LOGIC SENSOR FUSION ĐỘNG ---
                if (currentTemp >= data->tempCritical || currentHum >= data->humCritical) {
                    data->currentLcdState = LCD_CRITICAL;
                } 
                else if (currentTemp >= data->tempWarning || currentHum < data->humDry || currentHum >= data->humDamp) {
                    data->currentLcdState = LCD_WARNING;
                } 
                else {
                    data->currentLcdState = LCD_NORMAL;
                }

                xSemaphoreGive(data->dataMutex);

                // PHÁT TÍN HIỆU ĐÁNH THỨC TASK LCD: Báo rằng đã có dữ liệu & trạng thái mới
                xSemaphoreGive(data->lcdUpdateSemaphore);

            }

            if (currentTemp > data->tempCritical) { 
                xSemaphoreGive(data->tempWarningSemaphore);
            }

            // Gọi hàm gửi dữ liệu cảm biến lên WebSocket 
            sendSensorDataToWebSocket(currentTemp, currentHum);
        } else {
            // Khi bạn rút dây, dhtStatus sẽ khác 0, code rẽ nhánh vào đây
            // lastSensorUpdateTick KHÔNG ĐƯỢC cập nhật.
            Serial.println("Cảnh báo: Mất kết nối cảm biến DHT20!");
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}