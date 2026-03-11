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

#endif

