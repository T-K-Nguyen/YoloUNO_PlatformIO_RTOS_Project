#include "task_check_info.h"

void Load_info_File()
{
  File file = LittleFS.open("/info.dat", "r");
  if (!file)
  {
    return;
  }
  DynamicJsonDocument doc(4096);
  DeserializationError error = deserializeJson(doc, file);
  if (error)
  {
    Serial.print(F("deserializeJson() failed: "));
  }
  else
  {
    if (xMutexCloudConfig != NULL &&
        xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      WIFI_SSID = doc["WIFI_SSID"] | "";
      WIFI_PASS = doc["WIFI_PASS"] | "";
      CORE_IOT_TOKEN = doc["CORE_IOT_TOKEN"] | CORE_IOT_TOKEN;
      CORE_IOT_SERVER = doc["CORE_IOT_SERVER"] | CORE_IOT_SERVER;
      CORE_IOT_PORT = doc["CORE_IOT_PORT"] | CORE_IOT_PORT;
      LOCAL_MQTT_BROKER_IP = doc["LOCAL_MQTT_BROKER_IP"] | LOCAL_MQTT_BROKER_IP;
      LOCAL_MQTT_BROKER_PORT = doc["LOCAL_MQTT_BROKER_PORT"] | LOCAL_MQTT_BROKER_PORT;
      xSemaphoreGive(xMutexCloudConfig);
    }

    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(100)) == pdTRUE)
    {
      wifi_ssid = WIFI_SSID;
      wifi_password = WIFI_PASS;
      xSemaphoreGive(xMutexWifiConfig);
    }
    else
    {
      wifi_ssid = WIFI_SSID;
      wifi_password = WIFI_PASS;
    }
  }
  file.close();
}

void Delete_info_File()
{
  if (LittleFS.exists("/info.dat"))
  {
    LittleFS.remove("/info.dat");
  }
  ESP.restart();
}

bool Save_info_File(String wifi_ssid, String wifi_pass, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT, String local_mqtt_broker_ip, String local_mqtt_broker_port, bool restartDevice)
{
  if (xMutexCloudConfig != NULL &&
      xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    WIFI_SSID = wifi_ssid;
    WIFI_PASS = wifi_pass;
    ::CORE_IOT_TOKEN = CORE_IOT_TOKEN;
    ::CORE_IOT_SERVER = CORE_IOT_SERVER;
    ::CORE_IOT_PORT = CORE_IOT_PORT;
    ::LOCAL_MQTT_BROKER_IP = local_mqtt_broker_ip;
    ::LOCAL_MQTT_BROKER_PORT = local_mqtt_broker_port;
    xSemaphoreGive(xMutexCloudConfig);
  }

  if (xMutexWifiConfig != NULL &&
      xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    ::wifi_ssid = wifi_ssid;
    ::wifi_password = wifi_pass;
    xSemaphoreGive(xMutexWifiConfig);
  }
  else
  {
    ::wifi_ssid = wifi_ssid;
    ::wifi_password = wifi_pass;
  }

  DynamicJsonDocument doc(4096);
  doc["WIFI_SSID"] = wifi_ssid;
  doc["WIFI_PASS"] = wifi_pass;
  doc["CORE_IOT_TOKEN"] = CORE_IOT_TOKEN;
  doc["CORE_IOT_SERVER"] = CORE_IOT_SERVER;
  doc["CORE_IOT_PORT"] = CORE_IOT_PORT;
  doc["LOCAL_MQTT_BROKER_IP"] = local_mqtt_broker_ip;
  doc["LOCAL_MQTT_BROKER_PORT"] = local_mqtt_broker_port;

  File configFile = LittleFS.open("/info.dat", "w");
  if (configFile)
  {
    serializeJson(doc, configFile);
    configFile.close();
    if (restartDevice)
    {
      ESP.restart();
    }
    return true;
  }
  else
  {
    Serial.println("Unable to save the configuration.");
    return false;
  }
}

bool check_info_File(bool check)
{
  if (!check)
  {
    if (!LittleFS.begin(true))
    {
      Serial.println("❌ Lỗi khởi động LittleFS!");
      return false;
    }
    Load_info_File();
  }
  
  bool emptyWifiInfo = false;
  if (xMutexCloudConfig != NULL &&
      xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(100)) == pdTRUE)
  {
    emptyWifiInfo = WIFI_SSID.isEmpty() && WIFI_PASS.isEmpty();
    xSemaphoreGive(xMutexCloudConfig);
  }
  else
  {
    emptyWifiInfo = WIFI_SSID.isEmpty() && WIFI_PASS.isEmpty();
  }

  if (emptyWifiInfo)
  {
    if (!check)
    {
      InitAP();
    }
    return false;
  }
  return true;
}