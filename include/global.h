#ifndef __GLOBAL_H__
#define __GLOBAL_H__

#include <Arduino.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include <SPIFFS.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <FS.h>

#include "shared_data.h"
#include "task_actuator.h"
#include "task_mainserver.h"
#include "task_lcd.h"
#include "task_sensor.h"

#define LED1_PIN 48
#define LED2_PIN 41
#define BOOT_PIN 0

extern String ssid;
extern String password;
extern String wifi_ssid;
extern String wifi_password;

extern bool pump_state;
extern bool fan_state;
extern bool led1_state;
extern bool led2_state;

#endif

