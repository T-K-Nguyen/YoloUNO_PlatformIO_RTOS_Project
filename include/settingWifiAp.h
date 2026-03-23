#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include "global.h"

extern bool WIFI_STATE;
extern bool WIFI_SEND;



extern void InitAP();
extern void InitWifi();

extern bool Wifi_reconnect();

extern void WifiSetCredentials(const String &ssidValue, const String &passwordValue);
extern void WifiGetCredentials(String &ssidOut, String &passwordOut);
extern bool WifiGetState();
extern void WifiSetSendFlag(bool value);
extern bool WifiGetSendFlag();
#endif