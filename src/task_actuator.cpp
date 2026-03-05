#include "task_actuator.h"
#include <Adafruit_NeoPixel.h>

// ==========================================
// TASK 1: Single LED Blink with Temperature
// ==========================================
void TaskLEDControl(void *pvParameters) {
    pinMode(GPIO_NUM_48, OUTPUT);
    int ledState = 0;
    
    // Ép kiểu con trỏ vùng nhớ dùng chung để tránh biến toàn cục
    SensorData* data = (SensorData*)pvParameters;
    int delayTime = 2000; // Giá trị mặc định

    while(1) {
        float currentTemp = 0.0;
        
        // --- SEMAPHORE SYNCHRONIZATION ---
        // Sử dụng Mutex để lấy khóa bảo vệ, đảm bảo quá trình đọc nhiệt độ 
        // không bị xung đột khi Task Sensor đang ghi dữ liệu mới.
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            currentTemp = data->temperature; // Đọc dữ liệu an toàn
            xSemaphoreGive(data->dataMutex); // Trả khóa ngay lập tức
        }

        // --- CONDITION HANDLING ---
        // Phân loại 3 hành vi nháy LED dựa trên nhiệt độ
        if (currentTemp < 25.0) {
            // Trạng thái Bình thường: Nháy chậm (Chu kỳ 2s)
            delayTime = 2000; 
        } else if (currentTemp >= 25.0 && currentTemp < 35.0) {
            // Trạng thái Cảnh báo: Nháy nhanh (Chu kỳ 0.5s)
            delayTime = 500;  
        } else {
            // Trạng thái Nguy hiểm: Nháy rất nhanh (Chu kỳ 0.1s)
            delayTime = 100;  
        }

        // Logic đảo trạng thái LED gốc của bạn
        if (ledState == 0) {
            digitalWrite(GPIO_NUM_48, HIGH); 
        } else {
            digitalWrite(GPIO_NUM_48, LOW); 
        }
        ledState = 1 - ledState;
        
        // Thời gian chờ được điều chỉnh linh hoạt theo nhiệt độ
        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
}

// ==========================================
// TASK 2: NeoPixel LED Control Based on Humidity
// ==========================================
void neo_blinky(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    
    // Khởi tạo đối tượng NeoPixel cục bộ bên trong Task
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.show(); // Tắt LED khi khởi động
    
    while(1) {
        float currentHum = 0.0;

        // --- SEMAPHORE SYNCHRONIZATION ---
        // Tương tự Task 1, dùng Mutex để đồng bộ và đọc giá trị độ ẩm an toàn
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            currentHum = data->humidity;
            xSemaphoreGive(data->dataMutex);
        }

        // --- CONDITION HANDLING & MAPPING ---
        // Ánh xạ 3 dải độ ẩm sang 3 màu sắc khác nhau
        if (currentHum < 40.0) {
            // Khô hanh (< 40%): Màu Vàng (Red 255, Green 255, Blue 0)
            strip.setPixelColor(0, strip.Color(255, 255, 0)); 
        } else if (currentHum >= 40.0 && currentHum <= 70.0) {
            // Lý tưởng (40% - 70%): Màu Xanh lá cây (Red 0, Green 255, Blue 0)
            strip.setPixelColor(0, strip.Color(0, 255, 0));   
        } else {
            // Ẩm thấp (> 70%): Màu Xanh dương (Red 0, Green 0, Blue 255)
            strip.setPixelColor(0, strip.Color(0, 0, 255));   
        }

        strip.show(); // Cập nhật màu sắc ra phần cứng
        
        // Không cần thay đổi liên tục, kiểm tra và cập nhật màu mỗi 1 giây
        vTaskDelay(pdMS_TO_TICKS(1000)); 
    }
}