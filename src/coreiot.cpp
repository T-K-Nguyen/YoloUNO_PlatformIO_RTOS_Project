#include "coreiot.h"
#include "task_core_iot.h"
#include "shared_data.h"

namespace {
constexpr float DEVICE_LONGITUDE = 106.65789107082472;
constexpr float DEVICE_LATITUDE = 10.772175109674038;
}

void coreiot_task(void *pvParameters)
{
  SensorData *data = static_cast<SensorData *>(pvParameters);
  
  // Khởi tạo pointer toàn cuc để RPC callback có thể truy cập SensorData
  CORE_IOT_setSensorData(data);
  
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t lastTelemetryTick = 0;
  TickType_t lastAttributeTick = 0;
  const TickType_t loopInterval = pdMS_TO_TICKS(200);
  const TickType_t telemetryInterval = pdMS_TO_TICKS(10000);
  const TickType_t attributeInterval = pdMS_TO_TICKS(30000);
  bool loggedWifiWait = false;

  while (1)
  {
    // 1) WiFi connection
    EventBits_t bits = 0;
    if (xSystemEventGroup != NULL)
    {
      bits = xEventGroupWaitBits(
          xSystemEventGroup,
          WIFI_CONNECTED_BIT,
          pdFALSE,
          pdFALSE,
          pdMS_TO_TICKS(1000));
    }

    const bool wifiReady = ((bits & WIFI_CONNECTED_BIT) != 0) && (WiFi.status() == WL_CONNECTED);
    if (!wifiReady)
    {
      if (xSystemEventGroup != NULL)
      {
        xEventGroupClearBits(xSystemEventGroup, WIFI_CONNECTED_BIT);
      }

      // Proactively trigger STA reconnect when link is lost.
      Wifi_reconnect();

      if (!loggedWifiWait)
      {
        Serial.println("[COREIOT] Waiting for STA internet connection...");
        loggedWifiWait = true;
      }
      vTaskDelay(pdMS_TO_TICKS(500));
      continue;
    }
    loggedWifiWait = false;

    // 2) ThingsBoard connection
    CORE_IOT_reconnect();
    if (!CORE_IOT_isConnected())
    {
      Serial.println("[COREIOT] ThingsBoard not connected yet.");
      vTaskDelayUntil(&lastWake, loopInterval);
      continue;
    }

    // 3) Send telemetry data
    const TickType_t now = xTaskGetTickCount();
    if ((now - lastTelemetryTick) >= telemetryInterval &&
        data != nullptr && data->dataMutex != NULL &&
        xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      const bool tempOk = CORE_IOT_sendata("telemetry", "temperature", String(data->temperature, 2));
      const bool humOk = CORE_IOT_sendata("telemetry", "humidity", String(data->humidity, 2));
      const bool lonOk = CORE_IOT_sendata("telemetry", "longitude", String(DEVICE_LONGITUDE, 6));
      const bool latOk = CORE_IOT_sendata("telemetry", "latitude", String(DEVICE_LATITUDE, 6));

      if (tempOk && humOk && lonOk && latOk)
      {
        Serial.print("[COREIOT] Telemetry sent T=");
        Serial.print(data->temperature, 2);
        Serial.print(" H=");
        Serial.print(data->humidity, 2);
        Serial.print(" Long=");
        Serial.print(DEVICE_LONGITUDE, 6);
        Serial.print(" Lat=");
        Serial.println(DEVICE_LATITUDE, 6);
      }
      else
      {
        Serial.println("[COREIOT] Telemetry publish failed.");
      }
      lastTelemetryTick = now;
      xSemaphoreGive(data->dataMutex);
    }

    // 4) Send attribute data
    if ((now - lastAttributeTick) >= attributeInterval)
    {
      bool attrOk = true;
      attrOk = CORE_IOT_sendata("attribute", "localIp", WiFi.localIP().toString()) && attrOk;
      attrOk = CORE_IOT_sendata("attribute", "wifiConnected", (WiFi.status() == WL_CONNECTED) ? "true" : "false") && attrOk;
      attrOk = CORE_IOT_sendata("attribute", "wifiRssi", String(WiFi.RSSI())) && attrOk;

      if (xMutexCloudConfig != NULL &&
          xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(50)) == pdTRUE)
      {
        attrOk = CORE_IOT_sendata("attribute", "coreiotServer", CORE_IOT_SERVER) && attrOk;
        xSemaphoreGive(xMutexCloudConfig);
      }

      lastAttributeTick = now;
      if (attrOk)
      {
        Serial.println("[COREIOT] Attribute batch sent.");
      }
      else
      {
        Serial.println("[COREIOT] Attribute publish failed.");
      }
    }

    vTaskDelayUntil(&lastWake, loopInterval);
  }
}