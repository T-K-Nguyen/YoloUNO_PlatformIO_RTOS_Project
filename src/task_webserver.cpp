#include "task_webserver.h"
#include "task_check_info.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool isAPMode = true;

// WiFi connection status tracking
volatile bool wifiConnectionPending = false;
unsigned long wifiConnectionStartTime = 0;
TaskHandle_t wifiTaskHandle = NULL;


static const int FAN1_PIN = 38;
static const int FAN2_PIN = 8;

struct DeviceRuntimeState
{
    bool desired = false;
    bool actual = false;
    bool fault = false;
    bool initialized = false;
};

static DeviceRuntimeState fan1State;
static DeviceRuntimeState fan2State;

static DeviceRuntimeState *getDeviceState(const String &device)
{
    if (device == "fan1")
        return &fan1State;
    if (device == "fan2")
        return &fan2State;
    return nullptr;
}

static int getDevicePin(const String &device)
{
    if (device == "fan1")
        return FAN1_PIN;
    if (device == "fan2")
        return FAN2_PIN;
    return -1;
}

static void sendDeviceSyncMessage(const String &device, bool desired, bool actual, bool fault)
{
    JsonDocument resp;
    resp["type"] = "device_sync";
    resp["device"] = device;
    resp["desired"] = desired;
    resp["actual"] = actual;
    resp["state"] = actual;
    resp["fault"] = fault;

    String respBuf;
    serializeJson(resp, respBuf);
    sendWebSocketMessage(respBuf);
}

static void sendDeviceFaultMessage(const String &device, bool desired, bool actual)
{
    JsonDocument resp;
    resp["type"] = "device_fault";
    resp["device"] = device;
    resp["desired"] = desired;
    resp["actual"] = actual;
    resp["message"] = "Device state mismatch detected";

    String respBuf;
    serializeJson(resp, respBuf);
    sendWebSocketMessage(respBuf);
}

static void ensureDeviceInitialized(const String &device)
{
    DeviceRuntimeState *state = getDeviceState(device);
    int pin = getDevicePin(device);
    if (state == nullptr || pin < 0)
        return;

    if (!state->initialized)
    {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
        state->desired = false;
        state->actual = (digitalRead(pin) == HIGH);
        state->fault = (state->actual != state->desired);
        state->initialized = true;
    }
}

static void syncDeviceFromHardware(const String &device, bool notifyOnChange)
{
    DeviceRuntimeState *state = getDeviceState(device);
    int pin = getDevicePin(device);
    if (state == nullptr || pin < 0)
        return;

    ensureDeviceInitialized(device);

    bool previousActual = state->actual;
    bool previousFault = state->fault;
    state->actual = (digitalRead(pin) == HIGH);
    state->fault = (state->actual != state->desired);

    if (state->fault && !previousFault)
    {
        sendDeviceFaultMessage(device, state->desired, state->actual);
    }

    if (notifyOnChange && (state->actual != previousActual || state->fault != previousFault))
    {
        sendDeviceSyncMessage(device, state->desired, state->actual, state->fault);
    }
}

static void syncAllDevices(bool notifyOnChange)
{
    syncDeviceFromHardware("fan1", notifyOnChange);
    syncDeviceFromHardware("fan2", notifyOnChange);
}

void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        syncAllDevices(false);

        JsonDocument fan1Resp;
        fan1Resp["type"] = "device_sync";
        fan1Resp["device"] = "fan1";
        fan1Resp["desired"] = fan1State.desired;
        fan1Resp["actual"] = fan1State.actual;
        fan1Resp["state"] = fan1State.actual;
        fan1Resp["fault"] = fan1State.fault;
        String fan1Buf;
        serializeJson(fan1Resp, fan1Buf);
        client->text(fan1Buf);

        JsonDocument fan2Resp;
        fan2Resp["type"] = "device_sync";
        fan2Resp["device"] = "fan2";
        fan2Resp["desired"] = fan2State.desired;
        fan2Resp["actual"] = fan2State.actual;
        fan2Resp["state"] = fan2State.actual;
        fan2Resp["fault"] = fan2State.fault;
        String fan2Buf;
        serializeJson(fan2Resp, fan2Buf);
        client->text(fan2Buf);
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        String message;
        message.reserve(len + 1);
        for (size_t i = 0; i < len; i++)
        {
            message += (char)data[i];
        }
        parseWebSocketMessage(client, message); // Gọi hàm xử lý tin nhắn
        Serial.printf("WebSocket client #%u sent data: %s\n", client->id(), message.c_str());
    }
}

void parseWebSocketMessage(AsyncWebSocketClient *client, const String &message)
{
    // Xử lý tin nhắn từ client xử lý lệnh action

    if (message.startsWith("{\"action\":\"wifi\""))
    {
        // Xử lý cấu hình WiFi
        handleWifiConfig(client, message);
    } else if (message.startsWith("{\"action\":\"toggle_device")) {
        // Xử lý toggle motor/fan
        handleToggleDevice(message);
    }
}

// Xử lý lệnh toggle fan1/fan2 qua WebSocket
void handleToggleDevice(const String &message)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
        Serial.print("JSON parsing failed (toggle device): ");
        Serial.println(error.c_str());
        return;
    }
    String device = doc["device"] | "";
    DeviceRuntimeState *state = getDeviceState(device);
    int pin = getDevicePin(device);
    if (state == nullptr || pin < 0)
    {
        Serial.println("Unknown device!");
        return;
    }

    ensureDeviceInitialized(device);

    bool nextDesired = !state->desired;
    if (doc["state"].is<bool>())
    {
        nextDesired = doc["state"].as<bool>();
    }

    state->desired = nextDesired;
    digitalWrite(pin, state->desired ? HIGH : LOW);
    state->actual = (digitalRead(pin) == HIGH);
    state->fault = (state->actual != state->desired);

    Serial.printf("Set %s (pin %d) desired=%d actual=%d\n", device.c_str(), pin, state->desired, state->actual);

    if (state->fault)
    {
        sendDeviceFaultMessage(device, state->desired, state->actual);
    }
    sendDeviceSyncMessage(device, state->desired, state->actual, state->fault);
}

void handleWifiConfig(AsyncWebSocketClient *client, const String &message) {
    JsonDocument doc;

    // Deserialize the JSON string
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    // Extract values từ JSON và kiểm tra kỹ hơn
    // Extract values từ JSON và kiểm tra kỹ hơn
    const char* ssid = doc["ssid"];
    const char* password = doc["password"];
    const char* coreiotToken = doc["coreiot_token"];
    const char* coreiotServer = doc["coreiot_server"];
    const char* coreiotPort = doc["coreiot_port"];
    const char* localMqttBrokerIp = doc["local_mqtt_ip"];
    if (localMqttBrokerIp == nullptr || strlen(localMqttBrokerIp) == 0) {
        localMqttBrokerIp = doc["localMqttIp"];
    }
    if (localMqttBrokerIp == nullptr || strlen(localMqttBrokerIp) == 0) {
        localMqttBrokerIp = doc["mqtt_ip"];
    }

    const char* localMqttBrokerPort = doc["local_mqtt_port"];
    if (localMqttBrokerPort == nullptr || strlen(localMqttBrokerPort) == 0) {
        localMqttBrokerPort = doc["localMqttPort"];
    }
    if (localMqttBrokerPort == nullptr || strlen(localMqttBrokerPort) == 0) {
        localMqttBrokerPort = doc["mqtt_port"];
    }

    // Kiểm tra SSID kỹ lưỡng
    if (ssid == nullptr || strlen(ssid) == 0)
    {
        Serial.println("Error: SSID is null or empty!");
        return;
    }

    if (strlen(ssid) > 32)
    { // SSID tối đa 32 ký tự
        Serial.println("Error: SSID too long! Max 32 characters.");
        return;
    }

    // Kiểm tra password
    if (password != nullptr && strlen(password) > 64)
    { // Password tối đa 64 ký tự
        Serial.println("Error: Password too long! Max 64 characters.");
        return;
    }

    String newWifiSsid = String(ssid);
    String newWifiPass = (password != nullptr) ? String(password) : "";

    // Gán giá trị sau khi đã kiểm tra
    WifiSetCredentials(newWifiSsid, newWifiPass);
    WifiSetSendFlag(true);

    String tokenSnapshot;
    String serverSnapshot;
    String portSnapshot;
    String localBrokerIpSnapshot;
    String localBrokerPortSnapshot;
    if (xMutexCloudConfig != NULL &&
        xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(50)) == pdTRUE) {
        tokenSnapshot = CORE_IOT_TOKEN;
        serverSnapshot = CORE_IOT_SERVER;
        portSnapshot = CORE_IOT_PORT;
        localBrokerIpSnapshot = LOCAL_MQTT_BROKER_IP;
        localBrokerPortSnapshot = LOCAL_MQTT_BROKER_PORT;
        xSemaphoreGive(xMutexCloudConfig);
    } else {
        tokenSnapshot = CORE_IOT_TOKEN;
        serverSnapshot = CORE_IOT_SERVER;
        portSnapshot = CORE_IOT_PORT;
        localBrokerIpSnapshot = LOCAL_MQTT_BROKER_IP;
        localBrokerPortSnapshot = LOCAL_MQTT_BROKER_PORT;
    }

    String tokenToSave = (coreiotToken != nullptr && strlen(coreiotToken) > 0) ? String(coreiotToken) : tokenSnapshot;
    String serverToSave = (coreiotServer != nullptr && strlen(coreiotServer) > 0) ? String(coreiotServer) : serverSnapshot;
    String portToSave = (coreiotPort != nullptr && strlen(coreiotPort) > 0) ? String(coreiotPort) : portSnapshot;
    String localBrokerIpToSave = (localMqttBrokerIp != nullptr && strlen(localMqttBrokerIp) > 0) ? String(localMqttBrokerIp) : localBrokerIpSnapshot;
    String localBrokerPortToSave = (localMqttBrokerPort != nullptr && strlen(localMqttBrokerPort) > 0) ? String(localMqttBrokerPort) : localBrokerPortSnapshot;
    bool localBrokerIpRejected = false;

    if (localBrokerIpToSave.length() == 0 && client != nullptr) {
            const String candidateIp = client->remoteIP().toString();
            const bool isDeviceApSubnet = candidateIp.startsWith("192.168.4.");

            if (!isDeviceApSubnet) {
                localBrokerIpToSave = candidateIp;
                Serial.print("[WebConfig] local_mqtt_ip missing. Fallback to websocket client IP: ");
                Serial.println(localBrokerIpToSave);
            } else {
                Serial.print("[WebConfig] local_mqtt_ip missing and websocket client IP is AP subnet (");
                Serial.print(candidateIp);
                Serial.println("). Please input PC LAN IP running TinyMQTT (example: 172.20.10.x).\n[WebConfig] Keep local broker IP empty to avoid wrong target.");
            }
    }

    if (localBrokerPortToSave.length() == 0) {
        localBrokerPortToSave = "1883";
    }

    // Reject AP subnet as MQTT broker target: 192.168.4.x is ESP config AP network,
    // not the LAN where TinyMQTT broker usually runs.
    if (localBrokerIpToSave.startsWith("192.168.4.")) {
        localBrokerIpRejected = true;
        if (localBrokerIpSnapshot.length() > 0 && !localBrokerIpSnapshot.startsWith("192.168.4.")) {
            Serial.print("[WebConfig] Reject local_mqtt_ip in AP subnet: ");
            Serial.print(localBrokerIpToSave);
            Serial.print(". Keep previous valid broker IP: ");
            Serial.println(localBrokerIpSnapshot);
            localBrokerIpToSave = localBrokerIpSnapshot;
        } else {
            Serial.print("[WebConfig] Reject local_mqtt_ip in AP subnet: ");
            Serial.print(localBrokerIpToSave);
            Serial.println(". Please set PC LAN IP running TinyMQTT (example: 172.20.10.2).");
            localBrokerIpToSave = "";
        }
    }

    Serial.println("Received WiFi config:");
    Serial.println("SSID: " + newWifiSsid);
    Serial.println("Password length: " + String(newWifiPass.length()));
    Serial.println("Local MQTT Broker: " + localBrokerIpToSave + ":" + localBrokerPortToSave);

    // Lưu cấu hình vào file ngay
    bool saved = Save_info_File(newWifiSsid, newWifiPass, tokenToSave, serverToSave, portToSave, localBrokerIpToSave, localBrokerPortToSave, false);
    if (!saved) {
        Serial.println("Failed to persist config to /info.dat");
    }

    // ⚠️ KHÔNG gọi InitWifi() ở đây! Nó sẽ block WebSocket handler
    // Thay vào đó, chỉ set flag để coreiot_task xử lý kết nối
    // Gửi thông báo về client: konfigurasi đã lưu
    JsonDocument resp;
    resp["type"] = "wifi_config_saved";
    resp["success"] = saved;
    if (localBrokerIpRejected) {
        resp["warning"] = "local_mqtt_ip belongs to ESP AP subnet (192.168.4.x). Use LAN IP of the TinyMQTT host, e.g. 172.20.10.2.";
    }
    String respBuf;
    serializeJson(resp, respBuf);
    sendWebSocketMessage(respBuf);
}

void initWebServer()
{
    // Khởi tạo mode WIFI_AP_STA để có thể vừa làm AP vừa kết nối WiFi
    InitAP();

    ensureDeviceInitialized("fan1");
    ensureDeviceInitialized("fan2");
    syncAllDevices(false);

    if (!LittleFS.begin(true))
    {
        Serial.println("An Error has occurred while mounting LittleFS");
        return;
    }

    File root = LittleFS.open("/");
    File file = root.openNextFile();
    while (file)
    {
        Serial.println(file.name());
        file = root.openNextFile();
    }

    ElegantOTA.begin(&server);

    ws.onEvent(onEvent);

    server.addHandler(&ws);

    // Route goc: tra ve giao dien theo interface (AP hoac STA)
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        AsyncClient *client = request->client();
        IPAddress localIP = client ? client->localIP() : IPAddress((uint32_t)0);
        IPAddress apIP = WiFi.softAPIP();

        if (localIP == apIP)
        {
            request->send(LittleFS, "/index.html", "text/html");
        }
        else
        {
            request->send(LittleFS, "/dashboard_sta.html", "text/html");
        } });

    // Định nghĩa serveStatic cho tất cả các đường dẫn tĩnh
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");

    // 404 cho các route không tồn tại
    server.onNotFound([](AsyncWebServerRequest *request)
                      { request->send(404, "text/plain", "Not found"); });

    server.begin();
    Serial.println("HTTP server started");
}

void webServerTask(void *pvParameters)
{
    initWebServer();
    while (true)
    {
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}

// Task xử lý WebSocket đã ngắt kết nối và xử lý OTA
void webSocketTask(void *pvParameters) {
    static bool lastWifiStatus = false;
    static unsigned long lastStatusCheck = 0;
    // Ép kiểu pvParameters về cấu trúc dùng chung của hệ thống
    SensorData *data = (SensorData *)pvParameters;
    
    TickType_t lastSync = xTaskGetTickCount();
    TickType_t lastWebUpdate = xTaskGetTickCount(); // Bộ đếm gửi dữ liệu lên Web

    while (true)
    {
        ws.cleanupClients();
        ElegantOTA.loop();
        
        // Monitor WiFi status changes and notify clients
        bool currentWifiStatus = WiFi.status() == WL_CONNECTED;
        unsigned long now = millis();
        
        if (currentWifiStatus != lastWifiStatus) {
            lastWifiStatus = currentWifiStatus;
            
            // Send WiFi status update to all clients
            JsonDocument resp;
            if (currentWifiStatus) {
                resp["type"] = "wifi_connected";
                resp["success"] = true;
                resp["ip"] = WiFi.localIP().toString();
                Serial.println("[WebSocket] Notifying clients - WiFi connected!");
            } else {
                resp["type"] = "wifi_disconnected";
                resp["success"] = false;
                Serial.println("[WebSocket] Notifying clients - WiFi disconnected!");
            }
            
            String respBuf;
            serializeJson(resp, respBuf);
            sendWebSocketMessage(respBuf);
        }

        if ((xTaskGetTickCount() - lastSync) >= pdMS_TO_TICKS(500))
        {
            syncAllDevices(true);
            lastSync = xTaskGetTickCount();
        }

        // =========================================================
        // KIẾN TRÚC MỚI: ĐỌC DỮ LIỆU CẢM BIẾN AN TOÀN QUA MUTEX
        // =========================================================
        if (data != NULL && (xTaskGetTickCount() - lastWebUpdate) >= pdMS_TO_TICKS(2000)) 
        {
            float temp = 0.0f;
            float hum = 0.0f;
            bool newDataAvailable = false;

            // Xin cấp quyền Mutex tối đa 100 Ticks để không chặn luồng Web quá lâu
            if (xSemaphoreTake(data->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE) 
            {
                temp = data->temperature;
                hum = data->humidity;
                newDataAvailable = true;
                
                // Nhả Mutex ngay lập tức sau khi sao chép xong
                xSemaphoreGive(data->dataMutex); 
            }

            // Chỉ gửi WebSocket khi lấy được dữ liệu thành công
            if (newDataAvailable) 
            {
                sendSensorDataToWebSocket(temp, hum);
            }
            
            lastWebUpdate = xTaskGetTickCount();
        }
        // =========================================================

        vTaskDelay(100 / portTICK_PERIOD_MS); // kiểm tra mỗi 100ms
    }
}

void sendWebSocketMessage(String message)
{
    ws.textAll(message);
}

// Hàm gửi dữ liệu cảm biến lên WebSocket cho dashboard
void sendSensorDataToWebSocket(float temperature, float humidity)
{
    JsonDocument doc;
    doc["type"] = "sensor";
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    String jsonBuffer;
    serializeJson(doc, jsonBuffer);
    sendWebSocketMessage(jsonBuffer);
}

// SỬA HÀM NÀY: Truyền pvParameters vào cho xTaskCreate
void InitWebServer(void *pvParameters)
{
    xTaskCreate(webServerTask, "WebServerTask", 20000, pvParameters, 1, NULL);
    xTaskCreate(webSocketTask, "WebSocketTask", 10000, pvParameters, 1, NULL);
}