#include "task_webserver.h"

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");

bool isAPMode = true;
// Hàm gửi JSON qua WebSocket
static void sendJSON(JsonDocument &doc, AsyncWebSocketClient *client = nullptr)
{
    String payload;
    serializeJson(doc, payload);

    if (client != nullptr)
        client->text(payload);
    else
        ws.textAll(payload);
}
// Hàm gửi trạng thái thiết bị qua WebSocket
static void sendDeviceState(Device &device, AsyncWebSocketClient *client = nullptr)
{
    // Payload thong nhat cho dong bo ban dau va cap nhat trang thai.
    JsonDocument doc;
    doc["type"] = "device_sync";
    doc["device"] = device.name;
    doc["desired"] = device.desired;
    doc["actual"] = device.actual;
    doc["state"] = device.actual;
    doc["status"] = device.status;
    sendJSON(doc, client);
}
// Hàm xử lý tin nhắn WebSocket, phân tích JSON và thực hiện hành động tương ứng
static void handleMessage(const String &message)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);
    if (error)
    {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    String action = doc["action"] | "";
    if (action == "toggle_device")
    {
        handleToggleDevice(message);
    }
    else if (action == "wifi")
    {
        handleWifiConfig(message);
    }
}
// Hàm xử lý sự kiện WebSocket (kết nối, ngắt kết nối, nhận dữ liệu)
void onEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len)
{
    if (type == WS_EVT_CONNECT)
    {
        Serial.printf("WebSocket client #%u connected from %s\n", client->id(), client->remoteIP().toString().c_str());
        const size_t count = deviceManagerCount();
        for (size_t i = 0; i < count; i++)
        {
            Device *device = deviceManagerGetAt(i);
            if (device != nullptr)
            {
                sendDeviceState(*device, client);
            }
        }
    }
    else if (type == WS_EVT_DISCONNECT)
    {
        Serial.printf("WebSocket client #%u disconnected\n", client->id());
    }
    else if (type == WS_EVT_DATA)
    {
        // Nhận dữ liệu từ client, chuyển thành String và xử lý
        String message;
        message.reserve(len + 1);
        for (size_t i = 0; i < len; i++)
        {
            message += (char)data[i];
        }
        handleMessage(message); // Gọi hàm xử lý tin nhắn
        Serial.printf("WebSocket client #%u sent data: %s\n", client->id(), message.c_str());
    }
}

// Hàm xử lý yêu cầu toggle thiết bị, cập nhật trạng thái và gửi phản hồi qua WebSocket
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

    String name = doc["device"] | "";
    Device *device = deviceManagerGet(name);
    if (device == nullptr)
    {
        Serial.println("Unknown device!");
        return;
    }

    bool nextDesired = !device->desired;


    deviceManagerApplyDesiredState(*device, nextDesired);

    Serial.printf("Set %s (pin %d) desired=%d actual=%d\n", device->name, device->pin, device->desired, device->actual);
    sendDeviceState(*device);
}
// Hàm xử lý cấu hình WiFi, kết nối và gửi phản hồi qua WebSocket
void handleWifiConfig(const String &message)
{
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, message);

    if (error)
    {
        Serial.print("JSON parsing failed: ");
        Serial.println(error.c_str());
        return;
    }

    const char *ssid = doc["ssid"];
    const char *password = doc["password"];

    if (ssid == nullptr || strlen(ssid) == 0)
    {
        Serial.println("Error: SSID is null or empty!");
        return;
    }

    wifi_ssid = String(ssid);
    wifi_password = (password != nullptr) ? String(password) : "";

    Serial.println("Received WiFi config:");
    Serial.println("SSID: " + wifi_ssid);
    Serial.println("Password length: " + String(wifi_password.length()));

    InitWifi();
    JsonDocument resp;
    resp["type"] = "wifi_connected";
    resp["success"] = (WIFI_STATE == 1);
    sendJSON(resp);
}

// Hàm khởi tạo WebServer, thiết lập các route và WebSocket
void initWebServer()
{
    // init AP và thiết bị
    InitAP();
    deviceManagerInitAll();

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

    ws.onEvent(onEvent);

    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request)
              {
        AsyncClient *client = request->client();
        IPAddress localIP = client ? client->localIP() : IPAddress((uint32_t)0);
        IPAddress apIP = WiFi.softAPIP();
        
        // Phân biệt giữa truy cập từ AP (dashboard) và truy cập từ STA (index.html) dựa trên địa chỉ IP local của client.
        if (localIP == apIP)
        {
            request->send(LittleFS, "/index.html", "text/html");
        }
        else
        {
            request->send(LittleFS, "/dashboard_sta.html", "text/html");
        } });

    // Cấu hình để phục vụ các file tĩnh từ LittleFS, với index.html là trang mặc định.
    server.serveStatic("/", LittleFS, "/").setDefaultFile("index.html");
    // Xử lý các yêu cầu không khớp với handler nào, trả về 404.
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

        ws.cleanupClients();
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
}
// Task chuyên gửi dữ liệu cảm biến lên WebSocket, đảm bảo không bị chặn bởi việc đọc cảm biến
void webSocketTask(void *pvParameters)
{
    // Ép kiểu pvParameters về cấu trúc dùng chung của hệ thống
    SensorData *data = (SensorData *)pvParameters;

    TickType_t lastWebUpdate = xTaskGetTickCount(); // Bộ đếm gửi dữ liệu lên Web

    while (true)
    {
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
                xSemaphoreGive(data->dataMutex);
            }

            // Chỉ gửi WebSocket khi lấy được dữ liệu thành công
            if (newDataAvailable)
            {
                sendSensorDataToWebSocket(temp, hum);
            }
            lastWebUpdate = xTaskGetTickCount();
        }

        vTaskDelay(100 / portTICK_PERIOD_MS); // kiểm tra mỗi 100ms
    }
}

// Hàm gửi dữ liệu cảm biến lên WebSocket cho dashboard
void sendSensorDataToWebSocket(float temperature, float humidity)
{
    JsonDocument doc;
    doc["type"] = "sensor";
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    sendJSON(doc);
}
// SỬA HÀM NÀY: Truyền pvParameters vào cho xTaskCreate
void InitWebServer(void *pvParameters)
{
    xTaskCreate(webServerTask, "web", 10000, pvParameters, 1, NULL);
    xTaskCreate(webSocketTask, "sensor", 8000, pvParameters, 1, NULL);
}