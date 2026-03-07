#include "task_lcd.h"
#include <LCDI2C_Multilingual.h>
#include <Wire.h> // Bắt buộc thêm thư viện Wire vào đây

void TaskLCD(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    LCDI2C_Vietnamese lcd(0x21, 16, 2); // Đảm bảo đúng địa chỉ 0x27 hoặc 0x3F

    // --- KHỞI TẠO MÀN HÌNH AN TOÀN ---
    if (xSemaphoreTake(data->i2cMutex, portMAX_DELAY) == pdTRUE) {
        
        lcd.init(); // Lệnh này sẽ phá hỏng cấu hình chân I2C hiện tại!
        
        
        lcd.backlight();
        lcd.setCursor(0, 0);
        lcd.print("Khởi động hệ");
        lcd.setCursor(0, 1);
        lcd.print("thống RTOS...");
        
        xSemaphoreGive(data->i2cMutex);
    }

    while(1) {
        // NGỦ ĐÔNG VÀ CHỜ ĐỢI
        if (xSemaphoreTake(data->lcdUpdateSemaphore, portMAX_DELAY) == pdTRUE) {
            
            float temp = 0.0;
            float hum = 0.0;
            LcdDisplayState state = LCD_NORMAL;
            uint32_t lastUpdate = 0;

            if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                temp = data->temperature;
                hum = data->humidity;
                state = data->currentLcdState;
                lastUpdate = data->lastSensorUpdateTick;
                xSemaphoreGive(data->dataMutex);
            }

            if ((xTaskGetTickCount() - lastUpdate) > pdMS_TO_TICKS(5000)) {
                state = LCD_ERROR;
            }

            if (xSemaphoreTake(data->i2cMutex, portMAX_DELAY) == pdTRUE) {
                lcd.clear(); 

                switch(state) {
                    case LCD_NORMAL:
                        lcd.setCursor(0, 0); lcd.print("TT: Bình thường");
                        lcd.setCursor(0, 1);
                        lcd.print("NĐ:"); lcd.print(temp, 1); lcd.print("C Ẩm:"); lcd.print(hum, 0); lcd.print("%");
                        break;
                    case LCD_WARNING:
                        lcd.setCursor(0, 0); lcd.print("TT: CẢNH BÁO!");
                        lcd.setCursor(0, 1);
                        lcd.print("NĐ cao: "); lcd.print(temp, 1); lcd.print("C");
                        break;
                    case LCD_CRITICAL:
                        lcd.noBacklight(); vTaskDelay(pdMS_TO_TICKS(100)); lcd.backlight();
                        lcd.setCursor(0, 0); lcd.print("!! NGUY HIỂM !!");
                        lcd.setCursor(0, 1);
                        lcd.print("Sơ tán: "); lcd.print(temp, 1); lcd.print("C");
                        break;
                    case LCD_ERROR:
                        lcd.setCursor(0, 0); lcd.print("LỖI CẢM BIẾN!!");
                        lcd.setCursor(0, 1); lcd.print("Kiểm tra cáp I2C");
                        break;
                }
                xSemaphoreGive(data->i2cMutex); 
            }
        }
    }
}