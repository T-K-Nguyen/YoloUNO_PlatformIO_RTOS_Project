#include "task_core_iot.h"

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

// --- GLOBAL POINTER để SensorData có thể dùng được trong RPC callback ---
static SensorData* gSensorData = NULL;

// --- TRACK TOKEN CHANGE ĐỂ FORCE RECONNECT ---
// static String lastCachedToken = "";

constexpr char LED_STATE_ATTR[] = "ledState";

volatile int ledMode = 0;
volatile bool ledState = false;

constexpr uint16_t BLINKING_INTERVAL_MS_MIN = 10U;
constexpr uint16_t BLINKING_INTERVAL_MS_MAX = 60000U;
volatile uint16_t blinkingInterval = 1000U;

constexpr int16_t telemetrySendInterval = 10000U;

constexpr std::array<const char *, 1U> SHARED_ATTRIBUTES_LIST = {
    LED_STATE_ATTR,
};

void processSharedAttributes(const Shared_Attribute_Data &data)
{
    for (auto it = data.begin(); it != data.end(); ++it)
    {
        // if (strcmp(it->key().c_str(), BLINKING_INTERVAL_ATTR) == 0)
        // {
        //     const uint16_t new_interval = it->value().as<uint16_t>();
        //     if (new_interval >= BLINKING_INTERVAL_MS_MIN && new_interval <= BLINKING_INTERVAL_MS_MAX)
        //     {
        //         blinkingInterval = new_interval;
        //         Serial.print("Blinking interval is set to: ");
        //         Y
        //             Serial.println(new_interval);
        //     }
        // }
        // if (strcmp(it->key().c_str(), LED_STATE_ATTR) == 0)
        // {
        //     ledState = it->value().as<bool>();
        // digitalWrite(LED_PIN, ledState);
        // Serial.print("LED state is set to: ");
        // Serial.println(ledState);
        // }
    }
}

RPC_Response setLedSwitchValue(const RPC_Data &data)
{
    Serial.println("[COREIOT][RPC] Received method: setLedSwitchValue");
    bool newState = data;
    Serial.print("[COREIOT][RPC] LED state change: ");
    Serial.println(newState ? "ON" : "OFF");

    // Lưu trạng thái override vào shared data để TaskLEDControl xử lý
    if (gSensorData != NULL && gSensorData->dataMutex != NULL &&
        xSemaphoreTake(gSensorData->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        gSensorData->manualLedOverride = true;
        gSensorData->manualLedState = newState;
        gSensorData->lastManualLedTick = xTaskGetTickCount();
        xSemaphoreGive(gSensorData->dataMutex);
        Serial.println("[COREIOT][RPC] LED override activated (10s timeout)");
    }
    
    return RPC_Response("setLedSwitchValue", newState);
}

const std::array<RPC_Callback, 1U> callbacks = {
    RPC_Callback{"setLedSwitchValue", setLedSwitchValue}};

const Shared_Attribute_Callback attributes_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());
const Attribute_Request_Callback attribute_shared_request_callback(&processSharedAttributes, SHARED_ATTRIBUTES_LIST.cbegin(), SHARED_ATTRIBUTES_LIST.cend());

bool CORE_IOT_sendata(String mode, String feed, String data)
{
    if (!tb.connected())
    {
        return false;
    }

    if (mode == "attribute")
    {
        return tb.sendAttributeData(feed.c_str(), data.c_str());
    }
    else if (mode == "telemetry")
    {
        float value = data.toFloat();
        return tb.sendTelemetryData(feed.c_str(), value);
    }
    else
    {
        // handle unknown mode
        return false;
    }
}

void CORE_IOT_reconnect()
{
    String server;
    String token;
    int port = 1883;

    if (xMutexCloudConfig != NULL &&
        xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        server = CORE_IOT_SERVER;
        token = CORE_IOT_TOKEN;
        port = CORE_IOT_PORT.toInt();
        xSemaphoreGive(xMutexCloudConfig);
    }

    if (server.isEmpty() || token.isEmpty() || port <= 0)
    {
        Serial.println("[COREIOT] Missing server/token/port config.");
        return;
    }

    // // --- KIỂM TRA NẾUOKEN ĐÃ THAY ĐỔI -> FORCE DISCONNECT ---
    // if (lastCachedToken != token)
    // {
    //     if (tb.connected())
    //     {
    //         Serial.println("[COREIOT] Token changed detected! Disconnecting old connection...");
    //         tb.disconnect();
    //         vTaskDelay(pdMS_TO_TICKS(500)); // Chờ disconnect hoàn toàn
    //     }
    //     lastCachedToken = token;
    //     Serial.print("[COREIOT] New token: ");
    //     Serial.println(token.substring(0, min((int)token.length(), 8)) + "...");
    // }

    if (!tb.connected())
    {
        Serial.print("[COREIOT] Connecting to ");
        Serial.print(server);
        Serial.print(":");
        Serial.print(port);
        Serial.print(" token=");
        Serial.println(token.substring(0, min((int)token.length(), 8)) + "...");

        if (!tb.connect(server.c_str(), token.c_str(), port))
        {
            Serial.println("[COREIOT] ThingsBoard connect failed.");
            return;
        }

        Serial.println("[COREIOT] ThingsBoard connected.");

        tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());

        Serial.println("Subscribing for RPC...");
        if (!tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend()))
        {
            Serial.println("[COREIOT] Failed to subscribe for RPC");
            return;
        }
        Serial.println("[COREIOT] RPC subscribe success");

        if (!tb.Shared_Attributes_Subscribe(attributes_callback))
        {
            Serial.println("[COREIOT] Failed to subscribe for shared attribute updates");
            return;
        }

        Serial.println("[COREIOT] Shared attributes subscribe success");

        if (!tb.Shared_Attributes_Request(attribute_shared_request_callback))
        {
            Serial.println("[COREIOT] Failed to request shared attributes");
            return;
        }
        Serial.println("[COREIOT] Shared attributes request success");
        tb.sendAttributeData("localIp", WiFi.localIP().toString().c_str());
    }

    if (tb.connected())
    {
        tb.loop();
    }
}

bool CORE_IOT_isConnected()
{
    return tb.connected();
}

void CORE_IOT_setSensorData(SensorData* sensor_data)
{
    gSensorData = sensor_data;
}