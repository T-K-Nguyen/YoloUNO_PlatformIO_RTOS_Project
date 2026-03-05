#include "task_sensor.h"
#include <Wire.h>
#include "DHT20.h"

void TaskSensor(void *pvParameters) {
    // Ép kiểu con trỏ vùng nhớ dùng chung
    SensorData* data = (SensorData*)pvParameters;

    // Khởi tạo đối tượng cục bộ để tránh biến toàn cục
    DHT20 DHT(&Wire);
    
    // Cấu hình I2C theo thông số phần cứng của bạn
    Wire.begin(GPIO_NUM_11, GPIO_NUM_12);
    DHT.begin();

    while(1) {
        // 1. Lấy chìa khóa I2C trước khi giao tiếp phần cứng
        if (xSemaphoreTake(data->i2cMutex, portMAX_DELAY) == pdTRUE) {
            DHT.read();
            // Trả chìa khóa I2C ngay lập tức
            xSemaphoreGive(data->i2cMutex);
        }

        float currentTemp = DHT.getTemperature();
        float currentHum = DHT.getHumidity();

        Serial.print("Humidity: ");
        Serial.print(currentHum, 1);
        Serial.print("%, Temperature: ");
        Serial.println(currentTemp, 1);

        // 2. Lấy chìa khóa Dữ Liệu để lưu kết quả an toàn
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            data->temperature = currentTemp;
            data->humidity = currentHum;
            xSemaphoreGive(data->dataMutex);
        }

        // 3. Kích hoạt cảnh báo nếu nhiệt độ quá cao
        if (currentTemp > 35.0) { 
            xSemaphoreGive(data->tempWarningSemaphore);
        }

        // Delay 2 giây theo chuẩn RTOS
        vTaskDelay(pdMS_TO_TICKS(2000)); 
    }
}