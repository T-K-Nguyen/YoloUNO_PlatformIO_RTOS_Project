#include <Arduino.h>
#include "shared_data.h"

void TaskLEDControl(void *pvParameters) {
  pinMode(GPIO_NUM_48, OUTPUT);
  int ledState = 0;
  
  // 1. Cast the parameter back to our shared data struct
  SensorData* data = (SensorData*)pvParameters;

  while(1) {
    // 2. Use semaphores to manage task synchronization 
    // Wait for the Sensor Task to signal that a new temperature reading is ready
    if (xSemaphoreTake(data->tempWarningSemaphore, portMAX_DELAY) == pdTRUE) {
        
        float currentTemp = 20.0; // Default safe value
        
        // Safely read the shared temperature variable using Mutex
        if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            currentTemp = data->temperature;
            xSemaphoreGive(data->dataMutex);
        }

        // 3. Redefine the LED blinking behavior to respond to different temperature conditions 
        int delayTime = 2000; // Your original default delay

        if (currentTemp < 25.0) {
            delayTime = 2000; // Behavior 1: Normal (Original 2-second blink)
        } else if (currentTemp >= 25.0 && currentTemp < 35.0) {
            delayTime = 500;  // Behavior 2: Warning (Fast blink)
        } else {
            delayTime = 100;  // Behavior 3: Critical (Very fast blink)
        }

        // Your exact original LED toggle logic
        if (ledState == 0) {
          digitalWrite(GPIO_NUM_48, HIGH); // Turn ON LED
        } else {
          digitalWrite(GPIO_NUM_48, LOW); // Turn OFF LED
        }
        ledState = 1 - ledState;
        
        // Your original delay, now dynamically responding to temperature
        vTaskDelay(pdMS_TO_TICKS(delayTime)); 
    }
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  
  // Initialize the shared struct on the heap (removes global variables) 
  SensorData* sharedData = new SensorData();
  sharedData->temperature = 20.0; // Set an initial safe temperature
  
  // Initialize Semaphores
  sharedData->dataMutex = xSemaphoreCreateMutex();
  sharedData->tempWarningSemaphore = xSemaphoreCreateBinary();

  // Pass the sharedData pointer into your original task
  xTaskCreate(TaskLEDControl, "LED Control", 2048, (void*)sharedData, 2, NULL);
  
  // Note: You will also need to create your Sensor Task here later so it can "Give" the semaphore.
}

void loop() {
  // Serial.println("Hello Custom Board");
  // delay(1000);
  
  // FreeRTOS best practice: delete the loop task if it's not being used to save memory
  vTaskDelete(NULL); 
}