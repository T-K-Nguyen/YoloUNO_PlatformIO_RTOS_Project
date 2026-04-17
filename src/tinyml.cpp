#include "tinyml.h"
// Cần include file chứa định nghĩa struct SensorData của dự án
#include "shared_data.h" 

namespace {
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    constexpr int kTensorArenaSize = 4 * 1024; // Model đơn giản chỉ cần 4KB RAM
    uint8_t tensor_arena[kTensorArenaSize];
}

// Hàm setupTinyML() giữ nguyên như cũ...

void tiny_ml_task(void *pvParameters) {
    // Ép kiểu để lấy vùng nhớ chung
    SensorData* data = (SensorData*)pvParameters;
    
    setupTinyML();

    while (1) {
        float currentTemp = 0.0f;
        float currentHum = 0.0f;
        uint32_t lastUpdate = 0;

        // 1. LẤY DỮ LIỆU AN TOÀN TỪ MUTEX
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            currentTemp = data->temperature;
            currentHum = data->humidity;
            lastUpdate = data->lastSensorUpdateTick;
            xSemaphoreGive(data->dataMutex);
        }

        // Bỏ qua suy luận (Inference) nếu cảm biến đang bị lỗi cáp
        if ((xTaskGetTickCount() - lastUpdate) < pdMS_TO_TICKS(5000)) {
            
            // 2. CHUẨN BỊ INPUT CHO MẠNG NƠ-RON
            // Lưu ý: Cần Normalize (chuẩn hóa) dữ liệu giống lúc train trên Python
            // Giả sử lúc train ta chia Nhiệt cho 60.0 và Ẩm cho 100.0
            input->data.f[0] = currentTemp / 60.0f; 
            input->data.f[1] = currentHum / 100.0f;

            // 3. CHẠY SUY LUẬN TENSORFLOW LITE
            TfLiteStatus invoke_status = interpreter->Invoke();
            if (invoke_status == kTfLiteOk) {
                // Get output (Là một xác suất từ 0.0 đến 1.0)
                float anomalyProbability = output->data.f[0];
                
                Serial.print("TinyML Anomaly Score: ");
                Serial.print(anomalyProbability * 100);
                Serial.println("%");

                // 4. KẾT HỢP TINYML VỚI FSM BÁO ĐỘNG
                // Nếu AI phát hiện xác suất bất thường > 85%, ép hệ thống vào trạng thái Nguy hiểm
                if (anomalyProbability > 0.85f) {
                    if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        data->currentLcdState = LCD_CRITICAL;
                        xSemaphoreGive(data->dataMutex);
                        
                        // Đánh thức LCD lập tức để hiện cảnh báo
                        xSemaphoreGive(data->lcdUpdateSemaphore); 
                        // Kích hoạt chuông/led phụ nếu có
                        xSemaphoreGive(data->tempWarningSemaphore); 
                    }
                }
            } else {
                Serial.println("Lỗi: TinyML Invoke Failed!");
            }
        }

        // Task AI nặng nên chỉ cần chạy 5 giây một lần để tiết kiệm CPU
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}