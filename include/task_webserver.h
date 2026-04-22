#ifndef __TASK_WEBSERVER_H__
#define __TASK_WEBSERVER_H__

#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include "global.h"
#include "shared_data.h"

extern void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
extern void parseWebSocketMessage(AsyncWebSocketClient *client, const String &message);
extern void sendSensorDataToWebSocket(float temperature, float humidity);
extern void handleToggleDevice(const String &message);
extern void handleWifiConfig(const String &message);

extern void sendWebSocketMessage(String message);

extern void InitWebServer(void *pvParameters); 

#endif