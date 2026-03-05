#include "task_actuator.h"
#include <Adafruit_NeoPixel.h>

// ==========================================
// TASK 1: Single LED Blink with Temperature
// ==========================================
// Định nghĩa Máy trạng thái (FSM) cho hệ thống an toàn Cabin
enum CabinTempState {
    STATE_SAFE,         // Dưới 25°C
    STATE_WARNING,      // Từ 25°C đến 35°C
    STATE_CRITICAL,     // Trên 35°C
    STATE_SENSOR_ERROR  // Lỗi mất kết nối cảm biến
};

void TaskLEDControl(void *pvParameters) {
    pinMode(GPIO_NUM_48, OUTPUT);
    SensorData* data = (SensorData*)pvParameters;
    
    CabinTempState currentState = STATE_SAFE;
    bool ledIsOn = false;
    
    // Thông số Hysteresis (Khoảng trễ)
    const float HYSTERESIS_MARGIN = 1.0f; 

    while(1) {
        float temp = 0.0;
        uint32_t lastUpdate = 0;

        // --- ĐỌC DỮ LIỆU AN TOÀN BẰNG MUTEX ---
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            temp = data->temperature;
            lastUpdate = data->lastSensorUpdateTick;
            xSemaphoreGive(data->dataMutex);
        }

        // --- CƠ CHẾ FAILSAFE (PHÁT HIỆN LỖI) ---
        // Nếu qua 5 giây (5000 ticks) mà DHT20 không có dữ liệu mới -> Lỗi cảm biến
        if ((xTaskGetTickCount() - lastUpdate) > pdMS_TO_TICKS(5000)) {
            currentState = STATE_SENSOR_ERROR;
        } 
        else {
            // --- LOGIC HYSTERESIS CHUYỂN TRẠNG THÁI ---
            switch (currentState) {
                case STATE_SAFE:
                    if (temp >= 25.0f) currentState = STATE_WARNING;
                    break;
                case STATE_WARNING:
                    if (temp < (25.0f - HYSTERESIS_MARGIN)) currentState = STATE_SAFE;
                    else if (temp >= 35.0f) currentState = STATE_CRITICAL;
                    break;
                case STATE_CRITICAL:
                    if (temp < (35.0f - HYSTERESIS_MARGIN)) currentState = STATE_WARNING;
                    break;
                case STATE_SENSOR_ERROR:
                    // Nếu cảm biến có dữ liệu trở lại, reset về tính toán bình thường
                    currentState = STATE_SAFE; 
                    break;
            }
        }

        // --- THỰC THI HÀNH VI ĐÈN LED DỰA TRÊN TRẠNG THÁI ---
        uint32_t delayTime = 2000;

        switch (currentState) {
            case STATE_SAFE:
                // An toàn: Heartbeat nhịp nhàng (Nháy 100ms, tắt 1900ms)
                digitalWrite(GPIO_NUM_48, HIGH);
                vTaskDelay(pdMS_TO_TICKS(100));
                digitalWrite(GPIO_NUM_48, LOW);
                delayTime = 1900;
                break;

            case STATE_WARNING:
                // Cảnh báo: Nháy đều 500ms
                ledIsOn = !ledIsOn;
                digitalWrite(GPIO_NUM_48, ledIsOn ? HIGH : LOW);
                delayTime = 500;
                break;

            case STATE_CRITICAL:
                // Nguy hiểm: Nháy chớp nhoáng (Strobe effect) gây sự chú ý mạnh
                ledIsOn = !ledIsOn;
                digitalWrite(GPIO_NUM_48, ledIsOn ? HIGH : LOW);
                delayTime = 50; 
                break;

            case STATE_SENSOR_ERROR:
                // Lỗi hệ thống: Nhấp nháy SOS liên tục báo hiệu cần bảo trì
                for(int i=0; i<3; i++) { // 3 ngắn
                    digitalWrite(GPIO_NUM_48, HIGH); vTaskDelay(pdMS_TO_TICKS(150));
                    digitalWrite(GPIO_NUM_48, LOW);  vTaskDelay(pdMS_TO_TICKS(150));
                }
                vTaskDelay(pdMS_TO_TICKS(500));
                delayTime = 1000; // Đợi 1s trước khi lặp lại mã SOS
                break;
        }

        // Nghỉ ngơi theo thời gian đã tính toán
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