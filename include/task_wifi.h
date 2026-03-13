#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include "global.h"

extern bool WIFI_STATE;
extern bool WIFI_SEND;

extern String wifi_ssid;
extern String wifi_password;

extern void InitAP();
extern void InitWifi();

extern bool Wifi_reconnect();
#endif