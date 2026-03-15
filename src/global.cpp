#include "global.h"

float glob_temperature = 0;
float glob_humidity = 0;



String ssid = "esp32_myown";
String password = "123456789";

String wifi_ssid;
String wifi_password;


bool led1_state = false;
bool led2_state = false;
bool pump_state = false;
bool fan_state = false;
boolean isWifiConnected = false;

SemaphoreHandle_t xBinarySemaphoreInternet = xSemaphoreCreateBinary();