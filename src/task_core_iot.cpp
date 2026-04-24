#include "task_core_iot.h"

constexpr uint32_t MAX_MESSAGE_SIZE = 1024U;
constexpr size_t LOCAL_BROKER_HOST_MAX_LEN = 64U;
constexpr uint16_t LOCAL_BROKER_DEFAULT_PORT = 1883U;
constexpr char LOCAL_TELEMETRY_TOPIC_TEMPLATE[] = "local/devices/%s/telemetry";
constexpr char LOCAL_ATTRIBUTES_TOPIC_TEMPLATE[] = "local/devices/%s/attributes";
constexpr char LOCAL_COMMANDS_TOPIC_TEMPLATE[] = "local/devices/%s/commands";

WiFiClient wifiClient;
Arduino_MQTT_Client mqttClient(wifiClient);
ThingsBoard tb(mqttClient, MAX_MESSAGE_SIZE);

WiFiClient localMqttWifiClient;
PubSubClient localMqttClient(localMqttWifiClient);

static char localBrokerHost[LOCAL_BROKER_HOST_MAX_LEN] = {0};
static uint16_t localBrokerPort = LOCAL_BROKER_DEFAULT_PORT;
static bool localBrokerConfigured = false;
static bool localBrokerWaitLogged = false;
static String localDeviceId;
static bool localCommandSubscribed = false;
static TickType_t nextCloudReconnectTick = 0;
static String lastCloudServer;
static String lastCloudToken;
static int lastCloudPort = 0;
static bool cloudSubscriptionsReady = false;

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
constexpr TickType_t CLOUD_RECONNECT_BACKOFF = pdMS_TO_TICKS(3000);

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

namespace {
String buildLocalDeviceId()
{
    if (localDeviceId.length() > 0)
    {
        return localDeviceId;
    }

    String mac = WiFi.macAddress();
    mac.replace(":", "");
    localDeviceId = "esp32-" + mac;
    return localDeviceId;
}

void applyManualLedOverride(bool newState, const char *source)
{
    Serial.print("[LOCAL-MQTT] LED command from ");
    Serial.print(source);
    Serial.print(": ");
    Serial.println(newState ? "ON" : "OFF");

    if (gSensorData != NULL && gSensorData->dataMutex != NULL &&
        xSemaphoreTake(gSensorData->dataMutex, pdMS_TO_TICKS(100)) == pdTRUE)
    {
        gSensorData->manualLedOverride = true;
        gSensorData->manualLedState = newState;
        gSensorData->lastManualLedTick = xTaskGetTickCount();
        xSemaphoreGive(gSensorData->dataMutex);
        Serial.println("[LOCAL-MQTT] LED override applied.");
    }
    else
    {
        Serial.println("[LOCAL-MQTT] Failed to apply LED override (mutex busy).");
    }
}

void localMqttMessageCallback(char *topic, uint8_t *payload, unsigned int length)
{
    const String topicStr = String(topic);
    if (!topicStr.endsWith("/commands"))
    {
        return;
    }

    String payloadStr;
    payloadStr.reserve(length);
    for (unsigned int i = 0; i < length; i++)
    {
        payloadStr += static_cast<char>(payload[i]);
    }

    Serial.print("[LOCAL-MQTT] Command topic: ");
    Serial.println(topicStr);
    Serial.print("[LOCAL-MQTT] Command payload: ");
    Serial.println(payloadStr);

    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, payloadStr);
    if (err)
    {
        Serial.print("[LOCAL-MQTT] Command JSON parse failed: ");
        Serial.println(err.c_str());
        return;
    }

    bool handled = false;

    if (doc["method"].is<const char *>())
    {
        const String method = doc["method"].as<String>();
        if (method == "setLedSwitchValue")
        {
            bool newState = false;
            if (doc["params"].is<bool>())
            {
                newState = doc["params"].as<bool>();
                handled = true;
            }
            else if (doc["params"]["value"].is<bool>())
            {
                newState = doc["params"]["value"].as<bool>();
                handled = true;
            }

            if (handled)
            {
                applyManualLedOverride(newState, "cloud-bridge");
            }
        }
    }
    else if (doc["setLedSwitchValue"].is<bool>())
    {
        applyManualLedOverride(doc["setLedSwitchValue"].as<bool>(), "cloud-bridge");
        handled = true;
    }

    if (!handled)
    {
        Serial.println("[LOCAL-MQTT] Command ignored: unsupported payload format.");
    }
}

void updateLocalBrokerConfigSnapshot()
{
    String brokerIp;
    String brokerPortString;
    if (xMutexCloudConfig != NULL &&
        xSemaphoreTake(xMutexCloudConfig, pdMS_TO_TICKS(50)) == pdTRUE)
    {
        brokerIp = LOCAL_MQTT_BROKER_IP;
        brokerPortString = LOCAL_MQTT_BROKER_PORT;
        xSemaphoreGive(xMutexCloudConfig);
    }
    else
    {
        brokerIp = LOCAL_MQTT_BROKER_IP;
        brokerPortString = LOCAL_MQTT_BROKER_PORT;
    }

    const uint16_t parsedPort = static_cast<uint16_t>(brokerPortString.toInt());
    const uint16_t port = parsedPort > 0 ? parsedPort : LOCAL_BROKER_DEFAULT_PORT;

    const bool brokerInApSubnet = brokerIp.startsWith("192.168.4.");
    if (brokerIp.isEmpty() || brokerInApSubnet)
    {
        if (localMqttClient.connected())
        {
            localMqttClient.disconnect();
        }

        if (brokerInApSubnet)
        {
            Serial.print("[LOCAL-MQTT] Ignore broker IP in AP subnet: ");
            Serial.println(brokerIp);
        }

        localBrokerHost[0] = '\0';
        localBrokerConfigured = false;
        return;
    }

    const bool hostChanged = brokerIp != String(localBrokerHost);
    const bool portChanged = localBrokerPort != port;
    if (hostChanged || portChanged)
    {
        memset(localBrokerHost, 0, sizeof(localBrokerHost));
        strncpy(localBrokerHost, brokerIp.c_str(), sizeof(localBrokerHost) - 1);
        localBrokerPort = port;
        localMqttClient.setServer(localBrokerHost, localBrokerPort);
        localMqttClient.setCallback(localMqttMessageCallback);
        if (localMqttClient.connected())
        {
            localMqttClient.disconnect();
        }
        localCommandSubscribed = false;
    }

    localBrokerConfigured = true;
}

} // namespace

void CORE_IOT_reconnectLocalBroker()
{
    updateLocalBrokerConfigSnapshot();
    if (!localBrokerConfigured)
    {
        if (!localBrokerWaitLogged)
        {
            Serial.println("[LOCAL-MQTT] Waiting for broker IP config from web UI.");
            localBrokerWaitLogged = true;
        }
        return;
    }

    localBrokerWaitLogged = false;
    if (localMqttClient.connected())
    {
        localMqttClient.loop();
        return;
    }

    const String clientId = buildLocalDeviceId();
    Serial.print("[LOCAL-MQTT] Connecting to ");
    Serial.print(localBrokerHost);
    Serial.print(":");
    Serial.println(localBrokerPort);

    if (localMqttClient.connect(clientId.c_str()))
    {
        Serial.println("[LOCAL-MQTT] Connected.");

        char commandTopic[96] = {0};
        const String deviceId = buildLocalDeviceId();
        snprintf(commandTopic, sizeof(commandTopic), LOCAL_COMMANDS_TOPIC_TEMPLATE, deviceId.c_str());
        localCommandSubscribed = localMqttClient.subscribe(commandTopic, 0);
        if (localCommandSubscribed)
        {
            Serial.print("[LOCAL-MQTT] Subscribed command topic: ");
            Serial.println(commandTopic);
        }
        else
        {
            Serial.print("[LOCAL-MQTT] Failed to subscribe command topic: ");
            Serial.println(commandTopic);
        }
    }
    else
    {
        Serial.print("[LOCAL-MQTT] Connect failed, state=");
        Serial.println(localMqttClient.state());
    }
}

bool CORE_IOT_publishLocalTelemetry(float temperature, float humidity, float longitude, float latitude)
{
    if (!localMqttClient.connected())
    {
        return false;
    }

    JsonDocument doc;
    doc["ts"] = static_cast<uint32_t>(millis());
    doc["temperature"] = temperature;
    doc["humidity"] = humidity;
    doc["longitude"] = longitude;
    doc["latitude"] = latitude;

    String payload;
    serializeJson(doc, payload);

    char topic[96] = {0};
    const String deviceId = buildLocalDeviceId();
    snprintf(topic, sizeof(topic), LOCAL_TELEMETRY_TOPIC_TEMPLATE, deviceId.c_str());
    return localMqttClient.publish(topic, payload.c_str());
}

bool CORE_IOT_publishLocalAttributes(const String &localIp, bool wifiConnected, int32_t wifiRssi, const String &coreiotServer)
{
    if (!localMqttClient.connected())
    {
        return false;
    }

    JsonDocument doc;
    doc["ts"] = static_cast<uint32_t>(millis());
    doc["localIp"] = localIp;
    doc["wifiConnected"] = wifiConnected;
    doc["wifiRssi"] = wifiRssi;
    doc["coreiotServer"] = coreiotServer;

    String payload;
    serializeJson(doc, payload);

    char topic[96] = {0};
    const String deviceId = buildLocalDeviceId();
    snprintf(topic, sizeof(topic), LOCAL_ATTRIBUTES_TOPIC_TEMPLATE, deviceId.c_str());
    return localMqttClient.publish(topic, payload.c_str());
}

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

    const bool cloudConfigChanged =
        (server != lastCloudServer) ||
        (token != lastCloudToken) ||
        (port != lastCloudPort);

    if (cloudConfigChanged)
    {
        if (tb.connected())
        {
            tb.disconnect();
        }
        cloudSubscriptionsReady = false;
        lastCloudServer = server;
        lastCloudToken = token;
        lastCloudPort = port;
        nextCloudReconnectTick = 0;
        Serial.println("[COREIOT] Cloud config changed, reset connection state.");
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
        const TickType_t now = xTaskGetTickCount();
        if (nextCloudReconnectTick != 0 && now < nextCloudReconnectTick)
        {
            return;
        }

        const String cloudClientId = "tb-" + buildLocalDeviceId();

        Serial.print("[COREIOT] Connecting to ");
        Serial.print(server);
        Serial.print(":");
        Serial.print(port);
        Serial.print(" token=");
        Serial.println(token.substring(0, min((int)token.length(), 8)) + "...");

        if (!tb.connect(server.c_str(), token.c_str(), port, cloudClientId.c_str()))
        {
            Serial.println("[COREIOT] ThingsBoard connect failed.");
            nextCloudReconnectTick = now + CLOUD_RECONNECT_BACKOFF;
            return;
        }

        Serial.println("[COREIOT] ThingsBoard connected.");
        nextCloudReconnectTick = 0;

        tb.sendAttributeData("macAddress", WiFi.macAddress().c_str());

        if (!cloudSubscriptionsReady)
        {
            Serial.println("Subscribing for RPC...");
            if (!tb.RPC_Subscribe(callbacks.cbegin(), callbacks.cend()))
            {
                Serial.println("[COREIOT] Failed to subscribe for RPC");
                tb.disconnect();
                nextCloudReconnectTick = now + CLOUD_RECONNECT_BACKOFF;
                return;
            }
            Serial.println("[COREIOT] RPC subscribe success");

            if (!tb.Shared_Attributes_Subscribe(attributes_callback))
            {
                Serial.println("[COREIOT] Failed to subscribe for shared attribute updates");
                tb.disconnect();
                nextCloudReconnectTick = now + CLOUD_RECONNECT_BACKOFF;
                return;
            }

            Serial.println("[COREIOT] Shared attributes subscribe success");

            if (!tb.Shared_Attributes_Request(attribute_shared_request_callback))
            {
                Serial.println("[COREIOT] Failed to request shared attributes");
                tb.disconnect();
                nextCloudReconnectTick = now + CLOUD_RECONNECT_BACKOFF;
                return;
            }
            Serial.println("[COREIOT] Shared attributes request success");

            cloudSubscriptionsReady = true;
        }

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