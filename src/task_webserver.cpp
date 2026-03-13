#include "task_webserver.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

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
    }

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
}

void initWebServer() {

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

    // server.on("/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send(LittleFS, "/js/script.js", "application/javascript"); });
    // server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
    //           { request->send(LittleFS, "/css/style.css", "text/css"); });




            // Định nghĩa các route cho server
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/index.html", "text/html"); });


    server.on("/components/sidebar.html", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/components/sidebar.html", "text/html"); });

    server.on("/pages/home.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/pages/home.html", "text/html"); });

    server.on("/pages/dashboard.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/pages/dashboard.html", "text/html"); });

    server.on("/pages/wifi.html", HTTP_GET, [](AsyncWebServerRequest *request)
              { request->send(LittleFS, "/pages/wifi.html", "text/html"); });





            // Định nghĩa các route cho các file JavaScript
    server.on("/js/sidebar.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/js/sidebar.js", "application/javascript"); });

    server.on("/js/script.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/js/script.js", "application/javascript"); });

    server.on("/js/router.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/js/router.js", "application/javascript"); });

    server.on("/js/wifi.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/js/wifi.js", "application/javascript"); });

    server.on("/js/dashboard.js", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/js/dashboard.js", "application/javascript"); });


            //Định nghĩa các route cho các file CSS
    server.on("/css/style.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/css/style.css", "text/css"); });

    server.on("/css/bootstrap.min.css", HTTP_GET, [](AsyncWebServerRequest *request)
            { request->send(LittleFS, "/css/bootstrap.min.css", "text/css"); });



  server.begin();
  Serial.println("HTTP server started");
}


void webServerTask(void *pvParameters) {
    initWebServer();
    while (true) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

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


void InitWebServer() {
    xTaskCreate(webServerTask, "WebServerTask", 20000, NULL, 1, NULL);
    xTaskCreate(webSocketTask, "WebSocketTask", 10000, NULL, 1, NULL);
}