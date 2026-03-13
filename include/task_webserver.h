
#ifndef __TASK_WEBSERVER_H__
#define __TASK_WEBSERVER_H__

#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <AsyncTCP.h>
#include <ArduinoJson.h>
#include <ElegantOTA.h>
#include <global.h>

extern String output1State;
extern String output2State;
extern String output3State;
extern String output4State;

struct Sensor;  // Forward declaration

extern void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len);
extern void parseWebSocketMessage(AsyncWebSocketClient *client, const String &message);

void handleGetHistory(const String &message);

void handleSettings(const String &message);
void handleWifiConfig(const String &message);
void handleMQTT(const String &message);
void sendWebSocketMessage(String message);

extern void InitWebServer();

#endif