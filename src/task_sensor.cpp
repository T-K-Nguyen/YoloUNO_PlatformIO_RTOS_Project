#include "task_sensor.h"
#include <Wire.h>
#include "DHT20.h"

void TaskSensor(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    DHT20 DHT(&Wire);
    
    Wire.begin(GPIO_NUM_11, GPIO_NUM_12);
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
                
                // MẤU CHỐT Ở ĐÂY: Chỉ "gia hạn" thời gian sống khi cảm biến thực sự có dữ liệu
                data->lastSensorUpdateTick = xTaskGetTickCount();
                
                xSemaphoreGive(data->dataMutex);
            }

            if (currentTemp > 35.0) { 
                xSemaphoreGive(data->tempWarningSemaphore);
            }
        } else {
            // Khi bạn rút dây, dhtStatus sẽ khác 0, code rẽ nhánh vào đây
            // lastSensorUpdateTick KHÔNG ĐƯỢC cập nhật.
            Serial.println("Cảnh báo: Mất kết nối cảm biến DHT20!");
        }

        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}