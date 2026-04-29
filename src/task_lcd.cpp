#include "task_lcd.h"
#include <LCDI2C_Multilingual.h>
#include <Wire.h>

void TaskLCD(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    
    // SỬ DỤNG CON TRỎ ĐỂ CẤP PHÁT ĐỘNG
    LCDI2C_Vietnamese* lcd = new LCDI2C_Vietnamese(0x21, 16, 2); 
    
    bool isLcdConnected = false;
    bool forceDraw = true; // Cờ ép buộc vẽ lại toàn bộ màn hình khi mới cắm cáp
    
    // BỘ NHỚ ĐỆM (CACHE) ĐỂ THEO DÕI SỰ THAY ĐỔI (DELTA UPDATE)
    LcdDisplayState lastState = (LcdDisplayState)-1;
    float lastTemp = -999.0;
    float lastHum = -999.0;

    // --- KHỞI TẠO LẦN ĐẦU ---
    if (xSemaphoreTake(data->i2cMutex, portMAX_DELAY) == pdTRUE) {
        Wire.beginTransmission(0x21);
        if (Wire.endTransmission() == 0) {
            lcd->init();
            lcd->backlight();
            isLcdConnected = true;
        }
        xSemaphoreGive(data->i2cMutex);
    }

    while(1) {
        // Lệnh này ép Task LCD thức dậy nếu có Semaphore, 
        // HOẶC tự động thức dậy nếu quá 3 giây (3000 ticks) mà không nhận được tín hiệu.
        xSemaphoreTake(data->lcdUpdateSemaphore, pdMS_TO_TICKS(3000));
            
        float temp = 0.0, hum = 0.0;
        LcdDisplayState state = LCD_NORMAL;
        uint32_t lastUpdate = 0;

        // 1. CHỤP DỮ LIỆU BẰNG MUTEX
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            temp = data->temperature;
            hum = data->humidity;
            state = data->currentLcdState;
            lastUpdate = data->lastSensorUpdateTick;
            xSemaphoreGive(data->dataMutex);
        }

        // Ghi đè trạng thái nếu mất kết nối cảm biến quá 5 giây
        if ((xTaskGetTickCount() - lastUpdate) > pdMS_TO_TICKS(5000)) {
            state = LCD_ERROR;
        }

        // 2. KỸ THUẬT PARTIAL REDRAW VÀ AUTO-RECOVERY
        if (xSemaphoreTake(data->i2cMutex, portMAX_DELAY) == pdTRUE) {
            
            Wire.beginTransmission(0x21);
            if (Wire.endTransmission() == 0) { 
                
                if (!isLcdConnected) {
                    // ===================================================
                    // GIẢI QUYẾT LỖI MẤT CHỮ TIẾNG VIỆT
                    // Xóa object cũ để giải phóng Cache CGRAM bị lỗi
                    delete lcd; 
                    
                    // Khởi tạo lại object mới tinh
                    lcd = new LCDI2C_Vietnamese(0x21, 16, 2);
                    lcd->init();
                    lcd->backlight();
                    // ===================================================
                    
                    isLcdConnected = true;
                    forceDraw = true; 
                }
                
                // --- CẬP NHẬT DÒNG 1 ---
                if (state != lastState || forceDraw) {
                    lcd->setCursor(0, 0);
                    switch(state) {
                        case LCD_NORMAL:   lcd->print("TT: Bình thường "); break;
                        case LCD_WARNING:  lcd->print("TT: CẢNH BÁO!   "); break;
                        case LCD_CRITICAL: lcd->print("!! NGUY HIỂM !! "); break;
                        case LCD_ERROR:    lcd->print("LỖI CẢM BIẾN!!  "); break;
                    }
                }

                if (state == LCD_CRITICAL) {
                    lcd->noBacklight(); vTaskDelay(pdMS_TO_TICKS(100)); lcd->backlight();
                }

                // --- CẬP NHẬT DÒNG 2 ---
                if (state == LCD_ERROR) {
                    if (state != lastState || forceDraw) {
                        lcd->setCursor(0, 1); 
                        lcd->print("Kiểm tra cáp DHT");
                    }
                } else {
                    if (temp != lastTemp || hum != lastHum || state != lastState || forceDraw) {
                        lcd->setCursor(0, 1);
                        lcd->print("NĐ:"); lcd->print(temp, 1); 
                        lcd->print("C Ẩm:"); lcd->print(hum, 0); 
                        lcd->print("% "); 
                    }
                }

                lastState = state;
                lastTemp = temp;
                lastHum = hum;
                forceDraw = false; 

            } else {
                if (isLcdConnected) {
                    isLcdConnected = false; 
                }
            }
            
            xSemaphoreGive(data->i2cMutex); 
        }
    }
}