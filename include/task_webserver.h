
#ifndef __TASK_WEBSERVER_H__
#define __TASK_WEBSERVER_H__

#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <global.h>


struct Sensor;  // Forward declaration

extern void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
extern void parseWebSocketMessage(AsyncWebSocketClient *client, const String &message);
extern void sendSensorDataToWebSocket(float temperature, float humidity);
extern void handleToggleDevice(const String &message);
extern void handleWifiConfig(AsyncWebSocketClient *client, const String &message);

extern void sendWebSocketMessage(String message);

extern void InitWebServer();

#endif