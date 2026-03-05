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
// Định nghĩa các dải trạng thái độ ẩm (FSM)
enum HumidityState {
    HUM_DRY,        // Khô hanh (< 40%)
    HUM_OPTIMAL,    // Lý tưởng (40% - 70%)
    HUM_DAMP,       // Ẩm thấp (> 70%)
    HUM_ERROR       // Lỗi cảm biến
};

void neo_blinky(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    
    // Khởi tạo NeoPixel cục bộ
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.setBrightness(150); // Giới hạn độ sáng để không bị chói và tiết kiệm điện
    strip.show();
    
    HumidityState currentState = HUM_OPTIMAL;
    const float HUM_HYSTERESIS = 2.0f; // Trễ 2% độ ẩm để chống nhiễu màu
    
    // Biến lưu trữ màu hiện tại (Current RGB) và màu mục tiêu (Target RGB)
    uint8_t currR = 0, currG = 0, currB = 0;
    uint8_t targR = 0, targG = 0, targB = 0;

    // Biến cho hiệu ứng chớp lỗi
    bool errorBlinkState = false;
    uint32_t errorBlinkTimer = 0;

    while(1) {
        float hum = 0.0;
        uint32_t lastUpdate = 0;

        // --- 1. LẤY DỮ LIỆU CỰC NHANH BẰNG MUTEX ---
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            hum = data->humidity;
            lastUpdate = data->lastSensorUpdateTick;
            xSemaphoreGive(data->dataMutex);
        }

        // --- 2. XÁC ĐỊNH TRẠNG THÁI VÀ MÀU MỤC TIÊU (MAPPING) ---
        // Kiểm tra lỗi mất kết nối (5 giây)
        if ((xTaskGetTickCount() - lastUpdate) > pdMS_TO_TICKS(5000)) {
            currentState = HUM_ERROR;
        } else {
            // Logic Hysteresis chuyển trạng thái độ ẩm
            switch (currentState) {
                case HUM_DRY:
                    if (hum >= 40.0f) currentState = HUM_OPTIMAL;
                    break;
                case HUM_OPTIMAL:
                    if (hum < (40.0f - HUM_HYSTERESIS)) currentState = HUM_DRY;
                    else if (hum >= 70.0f) currentState = HUM_DAMP;
                    break;
                case HUM_DAMP:
                    if (hum < (70.0f - HUM_HYSTERESIS)) currentState = HUM_OPTIMAL;
                    break;
                case HUM_ERROR:
                    currentState = HUM_OPTIMAL; // Reset khi có tín hiệu lại
                    break;
            }
        }

        // Ánh xạ dải độ ẩm sang màu sắc [cite: 20, 22]
        switch (currentState) {
            case HUM_DRY:     // Vàng Cam (Cảnh báo khô)
                targR = 255; targG = 120; targB = 0; 
                break;
            case HUM_OPTIMAL: // Xanh ngọc mượt mà (Thư giãn)
                targR = 0; targG = 255; targB = 100; 
                break;
            case HUM_DAMP:    // Xanh dương sâu (Ẩm ướt)
                targR = 0; targG = 50; targB = 255; 
                break;
            case HUM_ERROR:   // Đỏ (Chuẩn bị nhấp nháy)
                targR = 255; targG = 0; targB = 0;
                break;
        }

        // --- 3. THUẬT TOÁN LERP CHUYỂN MÀU MƯỢT MÀ (FADE EFFECT) ---
        if (currentState != HUM_ERROR) {
            // Từ từ tiến màu hiện tại về màu mục tiêu (mỗi bước lặp tăng/giảm 1 đơn vị)
            if (currR < targR) currR++; else if (currR > targR) currR--;
            if (currG < targG) currG++; else if (currG > targG) currG--;
            if (currB < targB) currB++; else if (currB > targB) currB--;
            
            strip.setPixelColor(0, strip.Color(currR, currG, currB));
        } else {
            // Nếu lỗi, nháy đèn Đỏ chớp tắt liên tục (chu kỳ 500ms)
            errorBlinkTimer += 20; // Task chạy mỗi 20ms
            if (errorBlinkTimer >= 500) {
                errorBlinkState = !errorBlinkState;
                errorBlinkTimer = 0;
            }
            if (errorBlinkState) {
                strip.setPixelColor(0, strip.Color(targR, targG, targB));
            } else {
                strip.setPixelColor(0, strip.Color(0, 0, 0)); // Tắt
            }
            // Reset current RGB để khi hết lỗi sẽ fade từ màu đen lên
            currR = 0; currG = 0; currB = 0; 
        }

        strip.show(); // Đẩy màu ra phần cứng 

        // --- 4. RENDER Ở TỐC ĐỘ 50 FPS ---
        // Không dùng delay 1000ms nữa, delay 20ms để vòng lặp chạy liên tục tạo hiệu ứng fade
        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}