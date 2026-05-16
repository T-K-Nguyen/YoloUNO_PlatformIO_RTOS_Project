#include "task_toogle_boot.h"

#define BOOT BOOT_PIN

void Task_Toogle_BOOT(void *pvParameters)
{
    SensorData *data = (SensorData *)pvParameters;
    pinMode(BOOT, INPUT_PULLUP);

    bool lastButtonState = HIGH;
    uint32_t lastDebounceTick = 0;

    while (true)
    {
        bool currentButtonState = digitalRead(BOOT);

        if (currentButtonState != lastButtonState)
        {
            lastDebounceTick = xTaskGetTickCount();
            lastButtonState = currentButtonState;
        }

        if ((xTaskGetTickCount() - lastDebounceTick) > pdMS_TO_TICKS(50))
        {
            static bool handledPress = false;

            if (currentButtonState == LOW && !handledPress)
            {
                if (data != NULL && xSemaphoreTake(data->dataMutex, portMAX_DELAY) == pdTRUE)
                {
                    switch (data->sensorRunMode)
                    {
                        case SENSOR_MODE_NORMAL:
                            data->sensorRunMode = SENSOR_MODE_FIXED_24_30;
                            break;
                        case SENSOR_MODE_FIXED_24_30:
                            data->sensorRunMode = SENSOR_MODE_FIXED_30_50;
                            break;
                        case SENSOR_MODE_FIXED_30_50:
                            data->sensorRunMode = SENSOR_MODE_FIXED_30_70;
                            break;
                        case SENSOR_MODE_FIXED_30_70:
                        default:
                            data->sensorRunMode = SENSOR_MODE_NORMAL;
                            break;
                    }
                    xSemaphoreGive(data->dataMutex);
                }

                handledPress = true;
            }
            else if (currentButtonState == HIGH)
            {
                handledPress = false;
            }
        }

        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}