#ifndef __TASK_CHECK_INFO_H__
#define __TASK_CHECK_INFO_H__

#include <ArduinoJson.h>
#include "LittleFS.h"
#include "system_config.h"
#include "settingWifiAp.h"


bool check_info_File(bool check);
void Load_info_File();
void Delete_info_File();
bool Save_info_File(String WIFI_SSID, String WIFI_PASS, String CORE_IOT_TOKEN, String CORE_IOT_SERVER, String CORE_IOT_PORT, String LOCAL_MQTT_BROKER_IP = "", String LOCAL_MQTT_BROKER_PORT = "1883", bool restartDevice = true);

#endif