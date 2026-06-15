#include "network_task.h"

#include "../config.h"
#include "../secrets.h"
#include "../sensors/sensor_data.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

constexpr unsigned int MQTT_BUFFER_SIZE = 512;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 20000;
constexpr uint32_t MQTT_CONNECT_RETRY_MS = 5000;

WiFiClient g_wifiClient;
PubSubClient g_mqttClient(g_wifiClient);
char g_mqttTopic[96];

void buildMqttTopic() {
    snprintf(g_mqttTopic,
             sizeof(g_mqttTopic),
             "campus/%s/%s/data",
             MQTT_GROUP,
             DEVICE_ID);
}

bool connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    Serial.printf("[network] connexion WiFi a %s...\n", WIFI_SSID);

    const uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.println("[network] echec WiFi (timeout)");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(250));
    }

    Serial.printf("[network] WiFi OK — IP %s\n", WiFi.localIP().toString().c_str());
    return true;
}

bool connectMqtt() {
    if (g_mqttClient.connected()) {
        return true;
    }

    if (!connectWiFi()) {
        return false;
    }

    const String clientId = String(DEVICE_ID) + "-" + String(esp_random(), HEX);

    Serial.printf("[network] connexion MQTT %s:%d...\n", MQTT_BROKER, MQTT_PORT);

    const bool connected =
        (MQTT_USER[0] != '\0')
            ? g_mqttClient.connect(clientId.c_str(), MQTT_USER, MQTT_PASSWORD)
            : g_mqttClient.connect(clientId.c_str());

    if (!connected) {
        Serial.printf("[network] echec MQTT (rc=%d)\n", g_mqttClient.state());
        return false;
    }

    Serial.println("[network] MQTT connecte");
    return true;
}

bool publishSensorSnapshot(const SensorSnapshot& snapshot) {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["ts"] = snapshot.timestamp;

    JsonObject data = doc["data"].to<JsonObject>();
    data["temp"] = roundf(snapshot.temperature * 10.0f) / 10.0f;
    data["humidity"] = roundf(snapshot.humidity * 10.0f) / 10.0f;

    char payload[192];
    const size_t length = serializeJson(doc, payload, sizeof(payload));
    if (length == 0) {
        Serial.println("[network] erreur serialisation JSON");
        return false;
    }

    // PubSubClient publie en QoS 0 ; la persistance offline viendra a l'etape 6.
    const bool published = g_mqttClient.publish(g_mqttTopic, payload, false);
    if (published) {
        Serial.printf("[network] publie %s -> %s\n", g_mqttTopic, payload);
    } else {
        Serial.println("[network] echec publication MQTT");
    }

    return published;
}

void networkTask(void* parameter) {
    (void)parameter;

    buildMqttTopic();
    g_mqttClient.setServer(MQTT_BROKER, MQTT_PORT);
    g_mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    g_mqttClient.setKeepAlive(60);

    Serial.printf("[network] topic MQTT : %s\n", g_mqttTopic);

    uint32_t lastMqttAttempt = 0;

    for (;;) {
        g_mqttClient.loop();

        if (!g_mqttClient.connected()) {
            const uint32_t now = millis();
            if (now - lastMqttAttempt >= MQTT_CONNECT_RETRY_MS) {
                lastMqttAttempt = now;
                connectMqtt();
            }
        } else {
            SensorSnapshot snapshot;
            if (sensorDataGet(snapshot) && snapshot.valid) {
                publishSensorSnapshot(snapshot);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(NETWORK_INTERVAL_MS));
    }
}

}  // namespace

void networkTaskStart() {
    xTaskCreatePinnedToCore(
        networkTask,
        "network",
        TASK_NETWORK_STACK,
        nullptr,
        PRIORITY_NETWORK,
        nullptr,
        0);
}
