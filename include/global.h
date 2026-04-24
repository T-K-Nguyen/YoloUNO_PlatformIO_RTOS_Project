#ifndef __GLOBAL_H__
#define __GLOBAL_H__

// Include libraries
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#include "LittleFS.h"
#include "FS.h"
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <PubSubClient.h>
#include <HTTPClient.h>
#include <Wire.h>



// Include task headers

#include "shared_data.h"
#include "task_actuator.h"
#include "task_webserver.h"
#include "task_lcd.h"
#include "task_sensor.h"
#include "settingWifiAp.h"

// Define pins
#define LED1_PIN 48
#define LED2_PIN 41
#define BOOT_PIN 0
#define WIFI_CONNECTED_BIT BIT0


// Declare global variables
extern float glob_temperature;
extern float glob_humidity;

extern String WIFI_SSID;
extern String WIFI_PASS;
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;
extern String LOCAL_MQTT_BROKER_IP;
extern String LOCAL_MQTT_BROKER_PORT;

extern String ssid;
extern String password;
extern String wifi_ssid;
extern String wifi_password;

extern bool pump_state;
extern bool fan_state;
extern bool led1_state;
extern bool led2_state;

extern boolean isWifiConnected;

extern EventGroupHandle_t xSystemEventGroup;
extern SemaphoreHandle_t xMutexCloudConfig;
extern SemaphoreHandle_t xMutexWifiConfig;


#endif

