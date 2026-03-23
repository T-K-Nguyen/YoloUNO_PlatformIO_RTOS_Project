#include "settingWifiAp.h"

bool WIFI_STATE = 0;
bool WIFI_SEND = 0;
bool WIFI_RECONNECTING = 0;







void InitAP() {

    Serial.println("Setting AP");

    WiFi.mode(WIFI_AP_STA);

    bool ok = WiFi.softAP(ssid, password);
    if(!ok) {
        Serial.println("softAP failed!");
        return;
    }

    Serial.println("AP Started");
    Serial.print("AP IP: ");
    Serial.println(WiFi.softAPIP());

}



void InitWifi() {
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        if (WIFI_RECONNECTING) {
            xSemaphoreGive(xMutexWifiConfig);
            return; // Đang trong quá trình kết nối lại
        }
        WIFI_RECONNECTING = 1;
        xSemaphoreGive(xMutexWifiConfig);
    } else if (WIFI_RECONNECTING) {
        return; // Đang trong quá trình kết nối lại
    }

    String localSsid;
    String localPass;
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        localSsid = wifi_ssid;
        localPass = wifi_password;
        xSemaphoreGive(xMutexWifiConfig);
    } else {
        localSsid = wifi_ssid;
        localPass = wifi_password;
    }

    if (localSsid.length() == 0 || localSsid.length() > 32) {
        Serial.println("Invalid SSID, cannot connect to WiFi");
        if (xMutexWifiConfig != NULL &&
            xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
            WIFI_SEND = 0;
            WIFI_RECONNECTING = 0;
            xSemaphoreGive(xMutexWifiConfig);
        } else {
            WIFI_SEND = 0;
            WIFI_RECONNECTING = 0;
        }
        return;
    }

    Serial.println("Attempting to connect to WiFi: " + localSsid);
    
    // ⚠️ Trong AP_STA mode, chỉ disconnect STA, KHÔNG turn off WiFi
    // WiFi.disconnect(turnOffWiFi=true) sẽ tắt cả AP!
    WiFi.disconnect(false, false);  // Keep WiFi on, just disconnect STA
    vTaskDelay(500 / portTICK_PERIOD_MS);
    
    // Ensure we're in AP_STA mode trước khi begin
    WiFi.mode(WIFI_AP_STA);
    vTaskDelay(100 / portTICK_PERIOD_MS);
    
    WiFi.begin(localSsid.c_str(), localPass.c_str());
    
    unsigned long startTime = millis();
    while (WiFi.status() != WL_CONNECTED && (millis() - startTime) < 10000) { // Timeout 10 giây
        vTaskDelay(500 / portTICK_PERIOD_MS);
        Serial.print(".");
    }
    
    if (WiFi.status() == WL_CONNECTED) {
        if (xMutexWifiConfig != NULL &&
            xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
            WIFI_STATE = 1;
            isWifiConnected = true;
            WIFI_SEND = 0;
            WIFI_RECONNECTING = 0;
            xSemaphoreGive(xMutexWifiConfig);
        } else {
            WIFI_STATE = 1;
            isWifiConnected = true;
            WIFI_SEND = 0;
            WIFI_RECONNECTING = 0;
        }
        Serial.println("\nConnected to WiFi!");
        Serial.print("IP Address: ");
        Serial.println(WiFi.localIP());
        if (xSystemEventGroup != NULL) {
            xEventGroupSetBits(xSystemEventGroup, WIFI_CONNECTED_BIT);
        }
    } else {
        if (xMutexWifiConfig != NULL &&
            xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
            WIFI_STATE = 0;
            isWifiConnected = false;
            WIFI_SEND = 0;
            WIFI_RECONNECTING = 0;
            xSemaphoreGive(xMutexWifiConfig);
        } else {
            WIFI_STATE = 0;
            isWifiConnected = false;
            WIFI_SEND = 0;
            WIFI_RECONNECTING = 0;
        }
        Serial.println("\nFailed to connect to WiFi!");
        if (xSystemEventGroup != NULL) {
            xEventGroupClearBits(xSystemEventGroup, WIFI_CONNECTED_BIT);
        }
    }
}

bool Wifi_reconnect() {
    bool reconnecting = WIFI_RECONNECTING;
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        reconnecting = WIFI_RECONNECTING;
        xSemaphoreGive(xMutexWifiConfig);
    }

    if (WiFi.status() != WL_CONNECTED) {
        if (xSystemEventGroup != NULL) {
            xEventGroupClearBits(xSystemEventGroup, WIFI_CONNECTED_BIT);
        }
    }

    if (WiFi.status() != WL_CONNECTED && !reconnecting) {
        Serial.println("WiFi disconnected, attempting reconnect...");
        InitWifi();
    }
    return WiFi.status() == WL_CONNECTED;
}

void WifiSetCredentials(const String &ssidValue, const String &passwordValue) {
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        wifi_ssid = ssidValue;
        wifi_password = passwordValue;
        xSemaphoreGive(xMutexWifiConfig);
    } else {
        wifi_ssid = ssidValue;
        wifi_password = passwordValue;
    }
}

void WifiGetCredentials(String &ssidOut, String &passwordOut) {
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        ssidOut = wifi_ssid;
        passwordOut = wifi_password;
        xSemaphoreGive(xMutexWifiConfig);
    } else {
        ssidOut = wifi_ssid;
        passwordOut = wifi_password;
    }
}

bool WifiGetState() {
    bool connected = false;
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        connected = WIFI_STATE;
        xSemaphoreGive(xMutexWifiConfig);
    } else {
        connected = WIFI_STATE;
    }
    return connected;
}

void WifiSetSendFlag(bool value) {
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        WIFI_SEND = value;
        xSemaphoreGive(xMutexWifiConfig);
    } else {
        WIFI_SEND = value;
    }
}

bool WifiGetSendFlag() {
    bool sendFlag = false;
    if (xMutexWifiConfig != NULL &&
        xSemaphoreTake(xMutexWifiConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        sendFlag = WIFI_SEND;
        xSemaphoreGive(xMutexWifiConfig);
    } else {
        sendFlag = WIFI_SEND;
    }
    return sendFlag;
}