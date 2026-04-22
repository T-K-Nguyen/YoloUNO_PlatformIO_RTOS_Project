#ifndef __SETTING_WIFI_AP_H__
#define __SETTING_WIFI_AP_H__

#include "system_config.h"

extern bool WIFI_STATE;
extern bool WIFI_SEND;



extern void InitAP();
extern void InitWifi();

extern bool Wifi_reconnect();
#endif