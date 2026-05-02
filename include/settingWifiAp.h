#ifndef __SETTING_WIFI_AP_H__
#define __SETTING_WIFI_AP_H__

#include "system_config.h"

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