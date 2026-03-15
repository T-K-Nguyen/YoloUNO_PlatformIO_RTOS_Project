#include "task_webserver.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool isAPMode = true;


void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        String message = String((char *)data);
        parseWebSocketMessage(client, message); // Gọi hàm xử lý tin nhắn
        Serial.printf("WebSocket client #%u sent data: %s\n", client->id(), message.c_str());
  }
}

void parseWebSocketMessage(AsyncWebSocketClient *client, const String &message) {
    // Xử lý tin nhắn từ client xử lý lệnh action

    if (message.startsWith("{\"action\":\"wifi\"")) {
        // Xử lý cấu hình WiFi
        handleWifiConfig(message);
    } else if (message.startsWith("{\"action\":\"toggle_device")) {
        // Xử lý toggle motor/fan
        handleToggleDevice(message);
    }

}

// Xử lý lệnh toggle motor/fan qua WebSocket
void handleToggleDevice(const String &message) {
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error) {
        Serial.print("JSON parsing failed (toggle device): ");
        Serial.println(error.c_str());
        return;
    }
    String device = doc["device"] | "";
    int pin = -1;
    if (device == "motor") pin = 10;
    else if (device == "fan") pin = 8;
    else {
        Serial.println("Unknown device!");
        return;
    }
    pinMode(pin, OUTPUT);
    int current = digitalRead(pin);
    digitalWrite(pin, !current);
    Serial.printf("Toggled %s (pin %d) to %d\n", device.c_str(), pin, !current);
    // Gửi trạng thái mới về client
    JsonDocument resp;
    resp["type"] = "device_status";
    resp["device"] = device;
    resp["state"] = !current;
    String respBuf;
    serializeJson(resp, respBuf);
    sendWebSocketMessage(respBuf);
}


void handleWifiConfig(const String &message) {
    JsonDocument doc;

    // Deserialize the JSON string
    DeserializationError error = deserializeJson(doc, message);

    if (error) {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Extract values từ JSON và kiểm tra kỹ hơn
    const char* ssid = doc["ssid"];
    const char* password = doc["password"];

    // Kiểm tra SSID kỹ lưỡng
    if (ssid == nullptr || strlen(ssid) == 0) {
        Serial.println("Error: SSID is null or empty!");
        return;
    }

    if (strlen(ssid) > 32) { // SSID tối đa 32 ký tự
        Serial.println("Error: SSID too long! Max 32 characters.");
        return;
    }

    // Kiểm tra password
    if (password != nullptr && strlen(password) > 64) { // Password tối đa 64 ký tự
        Serial.println("Error: Password too long! Max 64 characters.");
        return;
    }

    // Gán giá trị sau khi đã kiểm tra
    wifi_ssid = String(ssid);
    wifi_password = (password != nullptr) ? String(password) : "";

    Serial.println("Received WiFi config:");
    Serial.println("SSID: " + wifi_ssid);
    Serial.println("Password length: " + String(wifi_password.length()));

    WIFI_SEND = 1;

    // Gọi hàm kết nối WiFi
    InitWifi();

    // Gửi thông báo về client: chỉ thành công nếu WIFI_STATE == 1
    JsonDocument resp;
    resp["type"] = "wifi_connected";
    resp["success"] = (WIFI_STATE == 1);
    String respBuf;
    serializeJson(resp, respBuf);
    sendWebSocketMessage(respBuf);
}

void initWebServer() {
    // Khởi tạo mode WIFI_AP_STA để có thể vừa làm AP vừa kết nối WiFi
    InitAP();

    if (!LittleFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file) {
        Serial.println(file.name());
        file = root.openNextFile();
    }


    ElegantOTA.begin(&server);

    ws.onEvent(onEvent);

    server.addHandler(&ws);




    // Định nghĩa serveStatic cho tất cả các đường dẫn tĩnh
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
        
    // 404 cho các route không tồn tại
    server.onNotFound([](AsyncWebServerRequest *request)
        { request->send(404, "text/plain", "Not found"); });


    server.begin();
    Serial.println("HTTP server started");
}


void webServerTask(void *pvParameters) {
    initWebServer();
    while (true) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Task xử lý WebSocket đã ngắt kết nối và xử lý OTA
void webSocketTask(void *pvParameters) {
    while (true) {
        ws.cleanupClients();
        ElegantOTA.loop();
        
        vTaskDelay(100 / portTICK_PERIOD_MS); // kiểm tra mỗi 100ms
    }
}


void sendWebSocketMessage(String message) {
    ws.textAll(message); 
}

// Hàm gửi dữ liệu cảm biến lên WebSocket cho dashboard 
void sendSensorDataToWebSocket(float temperature, float humidity) {
    JsonDocument doc;
    doc["type"] = "sensor";
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    sendWebSocketMessage(jsonBuffer);
}


void InitWebServer() {
    xTaskCreate(webServerTask, "WebServerTask", 20000, NULL, 1, NULL);
    xTaskCreate(webSocketTask, "WebSocketTask", 10000, NULL, 1, NULL);
}