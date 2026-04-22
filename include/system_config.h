#ifndef __SYSTEM_CONFIG_H__
#define __SYSTEM_CONFIG_H__

// Include libraries
#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

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

// Declare system variables
extern String CORE_IOT_TOKEN;
extern String CORE_IOT_SERVER;
extern String CORE_IOT_PORT;

#endif
