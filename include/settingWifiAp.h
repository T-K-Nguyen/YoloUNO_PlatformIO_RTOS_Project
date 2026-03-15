#ifndef __TASK_WIFI_H__
#define __TASK_WIFI_H__

#include "global.h"

extern bool WIFI_STATE;
extern bool WIFI_SEND;



extern void InitAP();
extern void InitWifi();

extern bool Wifi_reconnect();
#endif