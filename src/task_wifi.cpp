#include "task_wifi.h"

bool WIFI_STATE = 0;
bool WIFI_SEND = 0;
bool WIFI_RECONNECTING = 0;

String wifi_ssid = "";
String wifi_password = "";


constexpr char AP_SSID[] = "ESP32-Hy";
constexpr char AP_PASSWORD[] = "123456789";


void InitAP() {

    Serial.println("Setting AP");

    WiFi.mode(WIFI_AP_STA);

    bool ok = WiFi.softAP("ESP32-Hy", "123456789");
    if(!ok) {
        Serial.println("softAP failed!");
        return;
    }

    Serial.println("AP Started");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

}



void InitWifi() {
    if (WIFI_RECONNECTING) {
        return; // Đang trong quá trình kết nối lại
    }

    if (wifi_ssid.length() == 0 || wifi_ssid.length() > 32) {
        Serial.println("Invalid SSID, cannot connect to WiFi");
        WIFI_SEND = 0;
        return;
    }
    
    WIFI_RECONNECTING = 1;
    Serial.println("Attempting to connect to WiFi: " + wifi_ssid);
    
    WiFi.disconnect();
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    
    WiFi.begin(wifi_ssid.c_str(), wifi_password.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 10000) { // Timeout 10 giây
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        WIFI_STATE = 1;
        Serial.println("\nConnected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
    } else {
        WIFI_STATE = 0;
        Serial.println("\nFailed to connect to WiFi!");
    }
    
    WIFI_SEND = 0;
    WIFI_RECONNECTING = 0;
}

bool Wifi_reconnect() {
    if (WiFi.status() != WL_CONNECTED && !WIFI_RECONNECTING) {
        Serial.println("WiFi disconnected, attempting reconnect...");
        InitWifi();
    }
    return WiFi.status() == WL_CONNECTED;
}