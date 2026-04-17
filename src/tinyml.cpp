#include "tinyml.h"
#include "shared_data.h"

namespace {
    tflite::ErrorReporter *error_reporter = nullptr;
    const tflite::Model *model = nullptr;
    tflite::MicroInterpreter *interpreter = nullptr;
    TfLiteTensor *input = nullptr;
    TfLiteTensor *output = nullptr;
    
    // Model MLP của chúng ta rất nhỏ, 4KB RAM là dư sức chạy
    constexpr int kTensorArenaSize = 4 * 1024; 
    uint8_t tensor_arena[kTensorArenaSize];
}

// --- HÀM KHỞI TẠO TENSORFLOW LITE ---
void setupTinyML() {
    Serial.println("TensorFlow Lite Init....");
    static tflite::MicroErrorReporter micro_error_reporter;
    error_reporter = &micro_error_reporter;

    model = tflite::GetModel(dht_anomaly_model_tflite); 
    if (model->version() != TFLITE_SCHEMA_VERSION) {
        error_reporter->Report("Model provided is schema version %d, not equal to supported version %d.",
                               model->version(), TFLITE_SCHEMA_VERSION);
        return;
    }

    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        model, resolver, tensor_arena, kTensorArenaSize, error_reporter);
    interpreter = &static_interpreter;

    TfLiteStatus allocate_status = interpreter->AllocateTensors();
    if (allocate_status != kTfLiteOk) {
        error_reporter->Report("AllocateTensors() failed");
        return;
    }

    input = interpreter->input(0);
    output = interpreter->output(0);

    Serial.println("TensorFlow Lite Micro initialized on ESP32.");
}

// --- TASK RTOS CHẠY AI (ANOMALY DETECTION) ---
void tiny_ml_task(void *pvParameters) {
    SensorData* data = (SensorData*)pvParameters;
    
    // Gọi hàm khởi tạo ngay khi Task bắt đầu
    setupTinyML();

    while (1) {
        float currentTemp = 0.0f;
        float currentHum = 0.0f;
        uint32_t lastUpdate = 0;

        // 1. CHỤP DỮ LIỆU TỪ MUTEX
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            currentTemp = data->temperature;
            currentHum = data->humidity;
            lastUpdate = data->lastSensorUpdateTick;
            xSemaphoreGive(data->dataMutex);
        }

        // Bỏ qua quá trình suy luận (AI Inference) nếu cảm biến DHT20 đang bị đứt cáp
        if ((xTaskGetTickCount() - lastUpdate) < pdMS_TO_TICKS(5000)) {
            
            // 2. CHUẨN BỊ DỮ LIỆU CHO MẠNG NƠ-RON
            // Chuẩn hóa (Normalize) tỷ lệ giống hệt lúc train trên Colab
            input->data.f[0] = currentTemp / 60.0f; 
            input->data.f[1] = currentHum / 100.0f;

            // 3. CHẠY SUY LUẬN
            TfLiteStatus invoke_status = interpreter->Invoke();
            
            if (invoke_status == kTfLiteOk) {
                // Nhận kết quả từ Layer Output (Sigmoid trả về 0.0 -> 1.0)
                float anomalyProbability = output->data.f[0];
                
                Serial.print("TinyML Anomaly Score: ");
                Serial.print(anomalyProbability * 100);
                Serial.println("%");

                // 4. KẾT HỢP TINYML VỚI FSM (GHI ĐÈ TRẠNG THÁI KHẨN CẤP)
                // Nếu AI đánh giá rủi ro > 65%, lập tức kích hoạt báo động đỏ
                if (anomalyProbability > 0.65f) {
                    if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                        data->currentLcdState = LCD_CRITICAL;
                        xSemaphoreGive(data->dataMutex);
                        
                        // Kích hoạt Semaphores để đánh thức màn hình và còi/LED ngay lập tức
                        xSemaphoreGive(data->lcdUpdateSemaphore); 
                        xSemaphoreGive(data->tempWarningSemaphore); 
                    }
                }
            } else {
                Serial.println("Lỗi: TinyML Invoke Failed!");
            }
        }

        // Task AI tốn nhiều chu kỳ CPU, chỉ cần quét 5 giây một lần
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}