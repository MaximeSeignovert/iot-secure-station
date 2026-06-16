#include "network_task.h"

#include "../actuators/actuators.h"
#include "../config.h"
#include "../secrets.h"
#include "../security/device_config.h"
#include "../security/security.h"
#include "../sensors/sensor_data.h"
#include "../storage/storage.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <MQTT.h>
#include <WiFi.h>
#include <esp_wifi_types.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static WiFiClient g_wifiClient;
static MQTTClient g_mqttClient(1024);
static char g_mqttTopic[96];
static char g_mqttCmdTopic[96];
static bool g_wifiStarted = false;
static bool g_mqttWasConnected = false;
static uint32_t g_lastPublishLatencyMs = 0;
static uint64_t g_lastPublishTs = 0;
static uint64_t g_lastSnapshotTs = 0;

void applyMqttRuntimeConfig() {
    DeviceMqttConfig mqttConfig;
    deviceConfigGetMqtt(mqttConfig);
    g_mqttClient.begin(mqttConfig.broker, mqttConfig.port, g_wifiClient);
}

namespace {

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 30000;
constexpr uint32_t WIFI_RETRY_MS = 10000;
constexpr uint32_t MQTT_CONNECT_RETRY_MS = 5000;
constexpr uint8_t MQTT_REQUIRED_QOS = 1;

uint64_t epochMsNow() {
    struct tm tmNow;
    if (getLocalTime(&tmNow, 5)) {
        return static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    }
    return static_cast<uint64_t>(millis());
}

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

const char* wifiAuthLabel(wifi_auth_mode_t auth) {
    switch (auth) {
        case WIFI_AUTH_OPEN:
            return "OPEN";
        case WIFI_AUTH_WEP:
            return "WEP";
        case WIFI_AUTH_WPA_PSK:
            return "WPA_PSK";
        case WIFI_AUTH_WPA2_PSK:
            return "WPA2_PSK";
        case WIFI_AUTH_WPA_WPA2_PSK:
            return "WPA_WPA2_PSK";
        case WIFI_AUTH_WPA2_ENTERPRISE:
            return "WPA2_ENTERPRISE";
        case WIFI_AUTH_WPA3_PSK:
            return "WPA3_PSK";
        case WIFI_AUTH_WPA2_WPA3_PSK:
            return "WPA2_WPA3_PSK";
        default:
            return "UNKNOWN";
    }
}

const char* wifiReasonLabel(uint8_t reason) {
    switch (reason) {
        case WIFI_REASON_AUTH_FAIL:
            return "AUTH_FAIL (mot de passe incorrect ou refus d'authentification)";
        case WIFI_REASON_ASSOC_FAIL:
            return "ASSOC_FAIL (association refusee par le point d'acces)";
        case WIFI_REASON_HANDSHAKE_TIMEOUT:
            return "HANDSHAKE_TIMEOUT (echec negociation — verifier mot de passe)";
        case WIFI_REASON_NO_AP_FOUND:
            return "NO_AP_FOUND (SSID introuvable sur 2.4 GHz)";
        case WIFI_REASON_CONNECTION_FAIL:
            return "CONNECTION_FAIL";
        case WIFI_REASON_BEACON_TIMEOUT:
            return "BEACON_TIMEOUT (signal trop faible ou reseau hors de portee)";
        default:
            return "AUTRE";
    }
}

void onWiFiEvent(WiFiEvent_t event, WiFiEventInfo_t info) {
    if (event == ARDUINO_EVENT_WIFI_STA_DISCONNECTED) {
        const uint8_t reason = info.wifi_sta_disconnected.reason;
        Serial.printf("[network] deconnexion WiFi: %s (code %u)\n",
                      wifiReasonLabel(reason),
                      reason);
    } else if (event == ARDUINO_EVENT_WIFI_STA_CONNECTED) {
        Serial.println("[network] evenement WiFi: STA_CONNECTED");
    }
}

bool scanTargetNetwork(int32_t* outRssi, wifi_auth_mode_t* outAuth) {
    const int count = WiFi.scanNetworks(/*async=*/false, /*show_hidden=*/true);
    if (count <= 0) {
        Serial.println("[network] scan: aucun reseau detecte");
        WiFi.scanDelete();
        return false;
    }

    bool found = false;
    for (int i = 0; i < count; ++i) {
        const String ssid = WiFi.SSID(i);
        if (ssid != WIFI_SSID) {
            continue;
        }

        found = true;
        const wifi_auth_mode_t auth = WiFi.encryptionType(i);
        if (outRssi != nullptr) {
            *outRssi = WiFi.RSSI(i);
        }
        if (outAuth != nullptr) {
            *outAuth = auth;
        }

        Serial.printf("[network] SSID cible visible: \"%s\" (%d dBm, ch %d, %s)\n",
                      ssid.c_str(),
                      WiFi.RSSI(i),
                      WiFi.channel(i),
                      wifiAuthLabel(auth));

        if (auth == WIFI_AUTH_WPA2_ENTERPRISE) {
            Serial.println("[network] ERREUR: reseau WPA2-Enterprise — "
                           "non supporte par WiFi.begin(ssid, mdp)");
            Serial.println("[network] -> utiliser un hotspot 2.4 GHz ou un routeur WPA2 personnel");
        } else if (auth == WIFI_AUTH_WPA3_PSK) {
            Serial.println("[network] ATTENTION: WPA3 seul — activer WPA2/WPA3 mixte sur le routeur");
        }
        break;
    }

    if (!found) {
        Serial.printf("[network] ATTENTION: \"%s\" absent du scan 2.4 GHz\n", WIFI_SSID);
        Serial.println("[network] Causes probables:");
        Serial.println("[network]   - hotspot iPhone en 5 GHz uniquement");
        Serial.println("[network]   - SSID mal orthographie dans secrets.h (casse, accents, apostrophe)");
        Serial.println("[network]   - reseau hors de portee");
        Serial.println("[network] -> iPhone: Reglages > Donnees cellulaires > Mode de compatibilite");
    }

    WiFi.scanDelete();
    return found;
}

void buildMqttTopic() {
    snprintf(g_mqttTopic,
             sizeof(g_mqttTopic),
             "campus/%s/%s/data",
             MQTT_GROUP,
             DEVICE_ID);

    snprintf(g_mqttCmdTopic,
             sizeof(g_mqttCmdTopic),
             "campus/%s/%s/cmd",
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
    for (int i = 0; i < count; ++i) {
        const String ssid = WiFi.SSID(i);
        Serial.printf("[network]   - \"%s\" (%d dBm, ch %d, %s)\n",
                      ssid.c_str(),
                      WiFi.RSSI(i),
                      WiFi.channel(i),
                      wifiAuthLabel(WiFi.encryptionType(i)));
    }

    WiFi.scanDelete();
    scanTargetNetwork(nullptr, nullptr);
}

void startWiFiOnce() {
    if (g_wifiStarted) {
        return;
    }

    WiFi.persistent(false);
    WiFi.setAutoReconnect(true);
    WiFi.setSleep(false);
    WiFi.onEvent(onWiFiEvent);
    WiFi.mode(WIFI_AP_STA);

    g_wifiStarted = true;
    logWifiScan();
    WiFi.softAP(String("esp32-") + DEVICE_ID, "iotsecure");
    Serial.printf("[network] AP local actif : %s\n", WiFi.softAPIP().toString().c_str());
}

bool connectWiFi() {
    if (WiFi.status() == WL_CONNECTED) {
        return true;
    }

    startWiFiOnce();

    WiFi.disconnect(true);
    vTaskDelay(pdMS_TO_TICKS(250));

    int32_t targetRssi = 0;
    wifi_auth_mode_t targetAuth = WIFI_AUTH_OPEN;
    const bool targetVisible = scanTargetNetwork(&targetRssi, &targetAuth);
    if (targetVisible && targetAuth == WIFI_AUTH_WPA2_ENTERPRISE) {
        return false;
    }

    if (targetVisible) {
        Serial.printf("[network] connexion WiFi a \"%s\" (%d dBm)...\n",
                      WIFI_SSID,
                      targetRssi);
    } else {
        Serial.printf("[network] connexion WiFi a \"%s\" (SSID non visible au scan)...\n",
                      WIFI_SSID);
    }
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

    const bool connected = (mqttConfig.user[0] != '\0')
                               ? g_mqttClient.connect(clientId.c_str(),
                                                      mqttConfig.user,
                                                      mqttConfig.password)
                               : g_mqttClient.connect(clientId.c_str());

    if (!connected) {
        Serial.println("[network] echec MQTT");
        return false;
    }

    g_mqttClient.subscribe(g_mqttCmdTopic, MQTT_REQUIRED_QOS);
    Serial.printf("[network] subscribe %s (qos=%u)\n", g_mqttCmdTopic, MQTT_REQUIRED_QOS);
    Serial.println("[network] MQTT connecte");
    return true;
}

bool parseAndApplyCommand(const String& payload) {
    JsonDocument doc;
    const DeserializationError err = deserializeJson(doc, payload);
    if (err || !securityValidateJson(doc)) {
        Serial.println("[network] commande MQTT invalide (JSON)");
        return false;
    }

    // Compatibilité avec deux formats:
    // 1) { "action": "led_on" }
    // 2) { "cmd": "led", "value": "on" }
    if (!doc["action"].is<const char*>()) {
        const char* cmd = doc["cmd"] | "";
        const char* value = doc["value"] | "";

        if (strcmp(cmd, "led") == 0 && strcmp(value, "on") == 0) {
            doc["action"] = "led_on";
        } else if (strcmp(cmd, "led") == 0 && strcmp(value, "off") == 0) {
            doc["action"] = "led_off";
        } else if (strcmp(cmd, "relay") == 0 && strcmp(value, "on") == 0) {
            doc["action"] = "relay_on";
        } else if (strcmp(cmd, "relay") == 0 && strcmp(value, "off") == 0) {
            doc["action"] = "relay_off";
        }
    }

    if (!securityValidateActuatorCommand(doc)) {
        Serial.println("[network] commande MQTT refusee (validation)");
        return false;
    }

    const char* action = doc["action"];
    return actuatorsApplyCommand(action);
}

bool publishSensorSnapshot(const SensorSnapshot& snapshot) {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["ts"] = snapshot.timestamp;

    JsonObject data = doc["data"].to<JsonObject>();
    data["temp"] = roundf(snapshot.temperature * 10.0f) / 10.0f;
    data["humidity"] = roundf(snapshot.humidity * 10.0f) / 10.0f;
    data["potentiometer"] = roundf(snapshot.potentiometerPct);
    data["button"] = snapshot.buttonPressed ? 1 : 0;
    data["outlier"] = snapshot.outlier ? 1 : 0;

    char payload[STORAGE_MAX_LINE_BYTES];
    const size_t length = serializeJson(doc, payload, sizeof(payload));
    if (length == 0) {
        Serial.println("[network] erreur serialisation JSON");
        return false;
    }

    const uint32_t startedAt = millis();
    const bool published = g_mqttClient.publish(g_mqttTopic, payload, false, MQTT_REQUIRED_QOS);
    g_lastPublishLatencyMs = millis() - startedAt;
    g_lastPublishTs = epochMsNow();

    if (published) {
        Serial.printf("[network] publie qos=%u (%ums) %s\n",
                      MQTT_REQUIRED_QOS,
                      g_lastPublishLatencyMs,
                      g_mqttTopic);
    } else {
        Serial.println("[network] echec publication MQTT");
    }

    return published;
}

bool publishStoredPayload(const char* payload) {
    const uint32_t startedAt = millis();
    const bool published = g_mqttClient.publish(g_mqttTopic, payload, false, MQTT_REQUIRED_QOS);
    g_lastPublishLatencyMs = millis() - startedAt;
    g_lastPublishTs = epochMsNow();
    if (published) {
        Serial.printf("[network] republie (offline) -> %s\n", payload);
    }
    return published;
}

void onMqttMessage(MQTTClient* client, char topic[], char bytes[], int length) {
    (void)client;
    const String topicStr(topic == nullptr ? "" : topic);
    String payload;
    payload.reserve(static_cast<size_t>(length) + 1U);
    for (int i = 0; i < length; ++i) {
        payload += bytes[i];
    }

    if (topicStr == g_mqttCmdTopic) {
        const bool ok = parseAndApplyCommand(payload);
        Serial.printf("[network] cmd topic=%s status=%s\n",
                      topicStr.c_str(),
                      ok ? "ok" : "ko");
    }
}

void networkTask(void* parameter) {
    (void)parameter;

    buildMqttTopic();
    applyMqttRuntimeConfig();
    g_mqttClient.setKeepAlive(60);
    g_mqttClient.onMessageAdvanced(onMqttMessage);

    Serial.printf("[network] topic MQTT : %s\n", g_mqttTopic);
    Serial.printf("[network] topic CMD  : %s\n", g_mqttCmdTopic);

    configTime(0, 0, "pool.ntp.org", "time.google.com");

    uint32_t lastWifiAttempt = millis() - WIFI_RETRY_MS;
    uint32_t lastMqttAttempt = 0;
    uint32_t lastNtpSync = 0;

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

        const uint32_t now = millis();
        if (wifiUp && (lastNtpSync == 0 || now - lastNtpSync >= NTP_SYNC_INTERVAL_MS)) {
            configTime(0, 0, "pool.ntp.org", "time.google.com");
            lastNtpSync = now;
        }

        SensorSnapshot snapshot;
        if (sensorDataGet(snapshot) && snapshot.valid && snapshot.timestamp != g_lastSnapshotTs) {
            g_lastSnapshotTs = snapshot.timestamp;
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

void networkInitStack() {
    startWiFiOnce();
}

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
    snprintf(out.mqttCmdTopic, sizeof(out.mqttCmdTopic), "%s", g_mqttCmdTopic);
    out.mqttQos = MQTT_REQUIRED_QOS;
    out.publishLatencyMs = g_lastPublishLatencyMs;
    out.lastPublishTs = g_lastPublishTs;
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
