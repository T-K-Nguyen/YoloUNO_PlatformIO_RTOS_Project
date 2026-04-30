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

    // 2) Cloud connection (local broker disabled for Part 1 testing)
    CORE_IOT_reconnect();
    // CORE_IOT_reconnectLocalBroker();  // DISABLED: Testing CoreIOT only

    // 3) Send telemetry data
    const TickType_t now = xTaskGetTickCount();
    if ((now - lastTelemetryTick) >= telemetryInterval &&
        data != nullptr && data->dataMutex != NULL &&
        xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      bool cloudTelemetryOk = false;
      if (CORE_IOT_isConnected())
      {
        const bool tempOk = CORE_IOT_sendata("telemetry", "temperature", String(data->temperature, 2));
        const bool humOk = CORE_IOT_sendata("telemetry", "humidity", String(data->humidity, 2));
        const bool lonOk = CORE_IOT_sendata("telemetry", "longitude", String(DEVICE_LONGITUDE, 6));
        const bool latOk = CORE_IOT_sendata("telemetry", "latitude", String(DEVICE_LATITUDE, 6));
        cloudTelemetryOk = tempOk && humOk && lonOk && latOk;
      }

      // DISABLED: Local broker testing deferred to Part 2
      // const bool localTelemetryOk = CORE_IOT_publishLocalTelemetry(data->temperature, data->humidity, DEVICE_LONGITUDE, DEVICE_LATITUDE);

      if (cloudTelemetryOk)
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
        Serial.println("[COREIOT] Cloud telemetry skipped/failed (cloud not connected or publish error).");
      }

      // DISABLED: Local broker testing deferred to Part 2
      // if (!localTelemetryOk) { Serial.println("[LOCAL-MQTT] Telemetry publish failed."); }
      // else { Serial.println("[LOCAL-MQTT] Telemetry published."); }
      lastTelemetryTick = now;
      xSemaphoreGive(data->dataMutex);
    }

    // 4) Send attribute data
    if ((now - lastAttributeTick) >= attributeInterval)
    {
      bool cloudAttrOk = true;
      if (CORE_IOT_isConnected())
      {
        cloudAttrOk = CORE_IOT_sendata("attribute", "localIp", WiFi.localIP().toString()) && cloudAttrOk;
        cloudAttrOk = CORE_IOT_sendata("attribute", "wifiConnected", (WiFi.status() == WL_CONNECTED) ? "true" : "false") && cloudAttrOk;
        cloudAttrOk = CORE_IOT_sendata("attribute", "wifiRssi", String(WiFi.RSSI())) && cloudAttrOk;
      }
      else
      {
        cloudAttrOk = false;
      }

      // DISABLED: Local broker testing deferred to Part 2
      // bool localAttrOk = false;

      if (xMutexCloudConfig != NULL &&
          xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(50)) == pdTRUE)
      {
        if (CORE_IOT_isConnected())
        {
          cloudAttrOk = CORE_IOT_sendata("attribute", "coreiotServer", CORE_IOT_SERVER) && cloudAttrOk;
        }
        // DISABLED: Local broker publish - deferred to Part 2
        // localAttrOk = CORE_IOT_publishLocalAttributes(WiFi.localIP().toString(), (WiFi.status() == WL_CONNECTED), WiFi.RSSI(), CORE_IOT_SERVER);
        xSemaphoreGive(xMutexCloudConfig);
      }
      // DISABLED: Local broker publish - deferred to Part 2
      // else {
      //   localAttrOk = CORE_IOT_publishLocalAttributes(WiFi.localIP().toString(), (WiFi.status() == WL_CONNECTED), WiFi.RSSI(), CORE_IOT_SERVER);
      // }

      lastAttributeTick = now;
      if (cloudAttrOk)
      {
        Serial.println("[COREIOT] Attribute batch sent.");
      }
      else
      {
        Serial.println("[COREIOT] Cloud attribute skipped/failed (cloud not connected or publish error).");
      }

      // DISABLED: Local broker logging - deferred to Part 2
      // if (!localAttrOk) { Serial.println("[LOCAL-MQTT] Attribute publish failed."); }
      // else { Serial.println("[LOCAL-MQTT] Attribute published."); }
    }

    vTaskDelayUntil(&lastWake, loopInterval);
  }
}