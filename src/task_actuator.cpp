#include "task_actuator.h"
#include <Adafruit_NeoPixel.h>

// ==========================================
// TASK 1: Single LED Blink with Temperature
// ==========================================
// Định nghĩa Máy trạng thái (FSM) cho hệ thống an toàn Cabin
enum CabinTempState {
    STATE_SAFE,         // Dưới ngưỡng cảnh báo
    STATE_WARNING,      // Từ ngưỡng cảnh báo đến nguy hiểm
    STATE_CRITICAL,     // Trên ngưỡng nguy hiểm
    STATE_SENSOR_ERROR  // Lỗi mất kết nối cảm biến
};

void TaskLEDControl(void *pvParameters) {
    pinMode(GPIO_NUM_48, OUTPUT);
    SensorData* data = (SensorData*)pvParameters;
    
    CabinTempState currentState = STATE_SAFE;
    bool ledIsOn = false;
    
    // Thông số Hysteresis (Khoảng trễ)
    const float HYSTERESIS_MARGIN = 1.0f;
    const uint32_t MANUAL_OVERRIDE_TIMEOUT_MS = 10000; // 10 giây
    
    // --- TRACKING STATE CHANGE CỦA MANUAL OVERRIDE ---
    bool prevManualOverride = false;

    while(1) {
        // 1. Khởi tạo biến cục bộ với giá trị an toàn mặc định
        float temp = 0.0;
        float tWarn = 25.0f; 
        float tCrit = 35.0f;
        uint32_t lastUpdate = 0;
        bool manualOverride = false;
        bool manualLedState = false;
        uint32_t lastManualLedTick = 0;

        // --- ĐỌC DỮ LIỆU & NGƯỠNG ĐỘNG + MANUAL OVERRIDE BẰNG MUTEX ---
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            temp = data->temperature;
            lastUpdate = data->lastSensorUpdateTick;
            
            // Đọc các ngưỡng cảnh báo mới nhất do Web Server cài đặt
            tWarn = data->tempWarning;
            tCrit = data->tempCritical;
            
            // --- KIỂM TRA MANUAL OVERRIDE VÀ TIMEOUT ---
            manualOverride = data->manualLedOverride;
            manualLedState = data->manualLedState;
            lastManualLedTick = data->lastManualLedTick;
            
            // Kiểm tra timeout 10s - nếu quá hạn thì xóa override
            if (manualOverride && (xTaskGetTickCount() - lastManualLedTick) > pdMS_TO_TICKS(MANUAL_OVERRIDE_TIMEOUT_MS)) {
                data->manualLedOverride = false;
                manualOverride = false;
            }
            
            xSemaphoreGive(data->dataMutex);
        }

        // --- DETECT STATE CHANGE: OVERRIDE BẬT/TẮT ---
        if (manualOverride && !prevManualOverride) {
            // Override vừa bật
            Serial.printf("[LEDCTRL] Manual LED override activated: LED = %s\n", manualLedState ? "ON" : "OFF");
        } 
        else if (!manualOverride && prevManualOverride) {
            // Override vừa tắt (timeout hoặc user toggle OFF)
            Serial.println("[LEDCTRL] Manual LED override disabled - back to FSM");
        }
        prevManualOverride = manualOverride;

        // --- NẾU CÓ MANUAL OVERRIDE THÌ BỎ QUA FSM ---
        if (manualOverride) {
            // Trực tiếp điều khiển LED theo trạng thái từ RPC
            digitalWrite(GPIO_NUM_48, manualLedState ? HIGH : LOW);
            vTaskDelay(pdMS_TO_TICKS(200));
            continue; // Bỏ qua logic FSM, quay lại loop
        }

        // --- CƠ CHẾ FAILSAFE (PHÁT HIỆN LỖI) ---
        if ((xTaskGetTickCount() - lastUpdate) > pdMS_TO_TICKS(5000)) {
            currentState = STATE_SENSOR_ERROR;
        } 
        else {
            // --- LOGIC HYSTERESIS VỚI NGƯỠNG ĐỘNG ---
            switch (currentState) {
                case STATE_SAFE:
                    if (temp >= tWarn) currentState = STATE_WARNING;
                    break;
                case STATE_WARNING:
                    if (temp < (tWarn - HYSTERESIS_MARGIN)) currentState = STATE_SAFE;
                    else if (temp >= tCrit) currentState = STATE_CRITICAL;
                    break;
                case STATE_CRITICAL:
                    if (temp < (tCrit - HYSTERESIS_MARGIN)) currentState = STATE_WARNING;
                    break;
                case STATE_SENSOR_ERROR:
                    currentState = STATE_SAFE; 
                    break;
            }
        }

        // --- THỰC THI HÀNH VI ĐÈN LED ---
        uint32_t delayTime = 2000;

        switch (currentState) {
            case STATE_SAFE:
                digitalWrite(GPIO_NUM_48, HIGH);
                vTaskDelay(pdMS_TO_TICKS(100));
                digitalWrite(GPIO_NUM_48, LOW);
                delayTime = 1900;
                break;
            case STATE_WARNING:
                ledIsOn = !ledIsOn;
                digitalWrite(GPIO_NUM_48, ledIsOn ? HIGH : LOW);
                delayTime = 500;
                break;
            case STATE_CRITICAL:
                ledIsOn = !ledIsOn;
                digitalWrite(GPIO_NUM_48, ledIsOn ? HIGH : LOW);
                delayTime = 50; 
                break;
            case STATE_SENSOR_ERROR:
                for(int i=0; i<3; i++) { 
                    digitalWrite(GPIO_NUM_48, HIGH); vTaskDelay(pdMS_TO_TICKS(150));
                    digitalWrite(GPIO_NUM_48, LOW);  vTaskDelay(pdMS_TO_TICKS(150));
                }
                vTaskDelay(pdMS_TO_TICKS(500));
                delayTime = 1000; 
                break;
        }

        vTaskDelay(pdMS_TO_TICKS(delayTime));
    }
}

// ==========================================
// TASK 2: NeoPixel LED Control Based on Humidity
// ==========================================
enum HumidityState {
    HUM_DRY,        // Dưới ngưỡng khô hanh
    HUM_OPTIMAL,    // Giữa khô hanh và ẩm ướt
    HUM_DAMP,       // Trên ngưỡng ẩm ướt
    HUM_ERROR       // Lỗi cảm biến
};

void neo_blinky(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    
    Adafruit_NeoPixel strip(LED_COUNT, NEO_PIN, NEO_GRB + NEO_KHZ800);
    strip.begin();
    strip.setBrightness(150); 
    strip.show();
    
    HumidityState currentState = HUM_OPTIMAL;
    const float HUM_HYSTERESIS = 2.0f; 
    
    uint8_t currR = 0, currG = 0, currB = 0;
    uint8_t targR = 0, targG = 0, targB = 0;

    bool errorBlinkState = false;
    uint32_t errorBlinkTimer = 0;

    while(1) {
        // Khởi tạo biến cục bộ với giá trị an toàn mặc định
        float hum = 0.0;
        float hDry = 40.0f;
        float hDamp = 70.0f;
        uint32_t lastUpdate = 0;

        // --- 1. LẤY DỮ LIỆU CỰC NHANH VÀ ĐỌC NGƯỠNG ĐỘNG BẰNG MUTEX ---
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            hum = data->humidity;
            lastUpdate = data->lastSensorUpdateTick;
            
            // Cập nhật ngưỡng động từ Struct
            hDry = data->humDry;
            hDamp = data->humDamp;
            
            xSemaphoreGive(data->dataMutex);
        }

        // --- 2. XÁC ĐỊNH TRẠNG THÁI (MAPPING) VỚI NGƯỠNG ĐỘNG ---
        if ((xTaskGetTickCount() - lastUpdate) > pdMS_TO_TICKS(5000)) {
            currentState = HUM_ERROR;
        } else {
            switch (currentState) {
                case HUM_DRY:
                    if (hum >= hDry) currentState = HUM_OPTIMAL;
                    break;
                case HUM_OPTIMAL:
                    if (hum < (hDry - HUM_HYSTERESIS)) currentState = HUM_DRY;
                    else if (hum >= hDamp) currentState = HUM_DAMP;
                    break;
                case HUM_DAMP:
                    if (hum < (hDamp - HUM_HYSTERESIS)) currentState = HUM_OPTIMAL;
                    break;
                case HUM_ERROR:
                    currentState = HUM_OPTIMAL; 
                    break;
            }
        }

        // Ánh xạ màu sắc
        switch (currentState) {
            case HUM_DRY:     
                targR = 255; targG = 120; targB = 0; 
                break;
            case HUM_OPTIMAL: 
                targR = 0; targG = 255; targB = 100; 
                break;
            case HUM_DAMP:    
                targR = 0; targG = 50; targB = 255; 
                break;
            case HUM_ERROR:   
                targR = 255; targG = 0; targB = 0;
                break;
        }

        // --- 3. THUẬT TOÁN LERP CHUYỂN MÀU MƯỢT MÀ ---
        if (currentState != HUM_ERROR) {
            if (currR < targR) currR++; else if (currR > targR) currR--;
            if (currG < targG) currG++; else if (currG > targG) currG--;
            if (currB < targB) currB++; else if (currB > targB) currB--;
            
            strip.setPixelColor(0, strip.Color(currR, currG, currB));
        } else {
            errorBlinkTimer += 20; 
            if (errorBlinkTimer >= 500) {
                errorBlinkState = !errorBlinkState;
                errorBlinkTimer = 0;
            }
            if (errorBlinkState) {
                strip.setPixelColor(0, strip.Color(targR, targG, targB));
            } else {
                strip.setPixelColor(0, strip.Color(0, 0, 0)); 
            }
            currR = 0; currG = 0; currB = 0; 
        }

        strip.show(); 

        vTaskDelay(pdMS_TO_TICKS(20)); 
    }
}