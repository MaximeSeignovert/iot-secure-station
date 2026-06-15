#include "network_task.h"

#include "../config.h"
#include "../secrets.h"
#include "../sensors/sensor_data.h"
#include "../security/device_config.h"
#include "../storage/storage.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static WiFiClient g_wifiClient;
static PubSubClient g_mqttClient(g_wifiClient);
static char g_mqttTopic[96];
static bool g_wifiStarted = false;
static bool g_mqttWasConnected = false;

void applyMqttRuntimeConfig() {
    DeviceMqttConfig mqttConfig;
    deviceConfigGetMqtt(mqttConfig);
    g_mqttClient.setServer(mqttConfig.broker, mqttConfig.port);
}

namespace {

constexpr unsigned int MQTT_BUFFER_SIZE = 512;
constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t MQTT_CONNECT_RETRY_MS = 5000;

const char* wifiStatusLabel(wl_status_t status) {
    switch (status) {
        case WL_IDLE_STATUS:
            return "IDLE";
        case WL_NO_SSID_AVAIL:
            return "NO_SSID_AVAIL";
        case WL_SCAN_COMPLETED:
            return "SCAN_COMPLETED";
        case WL_CONNECTED:
            return "CONNECTED";
        case WL_CONNECT_FAILED:
            return "CONNECT_FAILED";
        case WL_CONNECTION_LOST:
            return "CONNECTION_LOST";
        case WL_DISCONNECTED:
            return "DISCONNECTED";
        default:
            return "UNKNOWN";
    }
}

void buildMqttTopic() {
    snprintf(g_mqttTopic,
             sizeof(g_mqttTopic),
             "campus/%s/%s/data",
             MQTT_GROUP,
             DEVICE_ID);
}

void logWifiScan() {
    Serial.println("[network] scan WiFi en cours...");

    const int count = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    if (count <= 0) {
        Serial.println("[network] aucun reseau WiFi detecte");
        WiFi.scanDelete();
        return;
    }

    Serial.printf("[network] %d reseau(x) detecte(s):\n", count);
    bool targetFound = false;

    for (int i = 0; i < count; ++i) {
        const String ssid = WiFi.SSID(i);
        Serial.printf("[network]   - \"%s\" (%d dBm, ch %d)\n",
                      ssid.c_str(),
                      WiFi.RSSI(i),
                      WiFi.channel(i));

        if (ssid == WIFI_SSID) {
            targetFound = true;
            Serial.printf("[network] SSID cible \"%s\" visible (%d dBm)\n",
                          WIFI_SSID,
                          WiFi.RSSI(i));
        }
    }

    if (!targetFound) {
        Serial.printf("[network] ATTENTION: \"%s\" absent du scan 2.4 GHz\n",
                      WIFI_SSID);
        Serial.println("[network] Cause probable: hotspot 5 GHz uniquement "
                       "(ESP32 = 2.4 GHz seulement)");
        Serial.println("[network] -> activer \"Compatibilite max\" / 2.4 GHz "
                       "sur le partage de connexion");
    }

    WiFi.scanDelete();
}

void startWiFiOnce() {
    if (g_wifiStarted) {
        return;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.mode(WIFI_STA);

    g_wifiStarted = true;
    logWifiScan();
}

bool connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    startWiFiOnce();

    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(100));

    Serial.printf("[network] connexion WiFi a \"%s\"...\n", WIFI_SSID);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    const uint32_t start = millis();
    wl_status_t lastStatus = WL_IDLE_STATUS;

    while (WiFi.status() != WL_CONNECTED) {
        const wl_status_t status = WiFi.status();
        if (status != lastStatus) {
            Serial.printf("[network] WiFi statut: %s (%d)\n",
                          wifiStatusLabel(status),
                          static_cast<int>(status));
            lastStatus = status;
        }

        if (status == WL_NO_SSID_AVAIL || status == WL_CONNECT_FAILED) {
            Serial.printf("[network] echec WiFi: %s\n", wifiStatusLabel(status));
            WiFi.disconnect(true);
            return false;
        }

        if (millis() - start >= WIFI_CONNECT_TIMEOUT_MS) {
            Serial.printf("[network] echec WiFi (timeout, dernier statut: %s)\n",
                          wifiStatusLabel(WiFi.status()));
            WiFi.disconnect(true);
            return false;
        }

        vTaskDelay(pdMS_TO_TICKS(250));
    }

    Serial.printf("[network] WiFi OK — IP %s, RSSI %d dBm\n",
                  WiFi.localIP().toString().c_str(),
                  WiFi.RSSI());
    return true;
}

bool connectMqtt() {
    if (g_mqttClient.connected()) {
        return true;
    }

    if (!connectWiFi()) {
        return false;
    }

    DeviceMqttConfig mqttConfig;
    deviceConfigGetMqtt(mqttConfig);

    const String clientId = String(DEVICE_ID) + "-" + String(esp_random(), HEX);

    Serial.printf("[network] connexion MQTT %s:%u...\n",
                  mqttConfig.broker,
                  mqttConfig.port);

    const bool connected =
        (mqttConfig.user[0] != '\0')
            ? g_mqttClient.connect(clientId.c_str(),
                                   mqttConfig.user,
                                   mqttConfig.password)
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

    const bool published = g_mqttClient.publish(g_mqttTopic, payload, false);
    if (published) {
        Serial.printf("[network] publie %s -> %s\n", g_mqttTopic, payload);
    } else {
        Serial.println("[network] echec publication MQTT");
    }

    return published;
}

bool publishStoredPayload(const char* payload) {
    const bool published = g_mqttClient.publish(g_mqttTopic, payload, false);
    if (published) {
        Serial.printf("[network] republie (offline) -> %s\n", payload);
    }
    return published;
}

void networkTask(void* parameter) {
    (void)parameter;

    buildMqttTopic();
    applyMqttRuntimeConfig();
    g_mqttClient.setBufferSize(MQTT_BUFFER_SIZE);
    g_mqttClient.setKeepAlive(60);

    Serial.printf("[network] topic MQTT : %s\n", g_mqttTopic);

    uint32_t lastWifiAttempt = 0;
    uint32_t lastMqttAttempt = 0;

    for (;;) {
        g_mqttClient.loop();

        const bool wifiUp = WiFi.status() == WL_CONNECTED;
        const bool mqttUp = wifiUp && g_mqttClient.connected();

        if (!wifiUp) {
            const uint32_t now = millis();
            if (now - lastWifiAttempt >= WIFI_RETRY_MS) {
                lastWifiAttempt = now;
                connectWiFi();
            }
        } else if (!mqttUp) {
            const uint32_t now = millis();
            if (now - lastMqttAttempt >= MQTT_CONNECT_RETRY_MS) {
                lastMqttAttempt = now;
                connectMqtt();
            }
        }

        if (mqttUp && !g_mqttWasConnected) {
            g_mqttWasConnected = true;
            storageFlush(publishStoredPayload);
        } else if (!mqttUp && g_mqttWasConnected) {
            g_mqttWasConnected = false;
        }

        SensorSnapshot snapshot;
        if (sensorDataGet(snapshot) && snapshot.valid) {
            if (mqttUp) {
                if (!publishSensorSnapshot(snapshot)) {
                    storageEnqueue(snapshot);
                }
            } else {
                storageEnqueue(snapshot);
            }
        }

        vTaskDelay(pdMS_TO_TICKS(NETWORK_INTERVAL_MS));
    }
}

}  // namespace

void networkGetStatus(NetworkStatus& out) {
    DeviceMqttConfig mqttConfig;
    deviceConfigGetMqtt(mqttConfig);

    out.wifiConnected = WiFi.status() == WL_CONNECTED;
    out.mqttConnected = g_mqttClient.connected();
    out.rssi = out.wifiConnected ? WiFi.RSSI() : 0;

    if (out.wifiConnected) {
        snprintf(out.ip, sizeof(out.ip), "%s", WiFi.localIP().toString().c_str());
        snprintf(out.ssid, sizeof(out.ssid), "%s", WiFi.SSID().c_str());
    } else {
        out.ip[0] = '\0';
        out.ssid[0] = '\0';
    }

    snprintf(out.mqttBroker, sizeof(out.mqttBroker), "%s", mqttConfig.broker);
    out.mqttPort = mqttConfig.port;
    snprintf(out.mqttTopic, sizeof(out.mqttTopic), "%s", g_mqttTopic);
}

void networkApplyMqttConfig() {
    if (g_mqttClient.connected()) {
        g_mqttClient.disconnect();
    }

    g_mqttWasConnected = false;
    applyMqttRuntimeConfig();
    Serial.println("[network] config MQTT rechargee depuis NVS");
}

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
