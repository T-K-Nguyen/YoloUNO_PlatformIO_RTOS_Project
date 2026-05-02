#ifndef __TASK_CORE_IOT_H__
#define __TASK_CORE_IOT_H__

#include <WiFi.h>
#include <ThingsBoard.h>
#include <Arduino_MQTT_Client.h>
#include <HTTPClient.h>
#include "task_check_info.h"
#include "system_config.h"
#include "shared_data.h"


bool CORE_IOT_sendata(String mode, String feed, String data);
void CORE_IOT_reconnect();
bool CORE_IOT_isConnected();
void CORE_IOT_setSensorData(SensorData* sensor_data);
void CORE_IOT_reconnectLocalBroker();
bool CORE_IOT_publishLocalTelemetry(float temperature, float humidity, float longitude, float latitude);
bool CORE_IOT_publishLocalAttributes(const String &localIp, bool wifiConnected, int32_t wifiRssi, const String &coreiotServer);

#endif