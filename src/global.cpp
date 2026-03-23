#include "global.h"

float glob_temperature = 0;
float glob_humidity = 0;

String WIFI_SSID;
String WIFI_PASS;
String CORE_IOT_TOKEN = "68PBxgP1uYZWp1Wk4zXE";
String CORE_IOT_SERVER = "app.coreiot.io";
String CORE_IOT_PORT = "1883";


String ssid = "esp32_myown";
String password = "123456789";

String wifi_ssid;
String wifi_password;


bool led1_state = false;
bool led2_state = false;
bool pump_state = false;
bool fan_state = false;
boolean isWifiConnected = false;

EventGroupHandle_t xSystemEventGroup = xEventGroupCreate();
SemaphoreHandle_t xMutexCloudConfig = xSemaphoreCreateMutex();
SemaphoreHandle_t xMutexWifiConfig = xSemaphoreCreateMutex();