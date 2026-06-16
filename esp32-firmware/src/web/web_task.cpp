#include "web_task.h"
#include "web_assets.h"

#include "../actuators/actuators.h"
#include "../config.h"
#include "../network/network_task.h"
#include "../security/device_config.h"
#include "../security/security.h"
#include "../sensors/sensor_data.h"
#include "../secrets.h"

#ifndef API_TOKEN
#define API_TOKEN ""
#endif

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

WebServer g_server(WEB_SERVER_PORT);
bool g_serverStarted = false;

void sendJson(int code, const JsonDocument& doc) {
    char buffer[512];
    serializeJson(doc, buffer, sizeof(buffer));
    g_server.send(code, "application/json", buffer);
}

void sendError(int code, const char* message) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = message;
    sendJson(code, doc);
}

bool serveWebAsset(const char* path) {
    WebAsset asset;
    if (!webAssetFind(path, asset)) {
        return false;
    }

    g_server.sendHeader("Cache-Control", "no-cache, no-store, must-revalidate");
    g_server.send_P(200, asset.contentType, asset.data);
    return true;
}

String staticPath(const String& uri) {
    const int query = uri.indexOf('?');
    return query >= 0 ? uri.substring(0, query) : uri;
}

void handleNotFound() {
    const String path = staticPath(g_server.uri());

    if (path == "/" || path == "/index.html") {
        if (serveWebAsset("/index.html")) {
            return;
        }
    } else if (path == "/style.css") {
        if (serveWebAsset("/style.css")) {
            return;
        }
    } else if (path == "/app.js") {
        if (serveWebAsset("/app.js")) {
            return;
        }
    }

    sendError(404, "ressource introuvable");
}

void handleApiStatus() {
    NetworkStatus network;
    networkGetStatus(network);

    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["group"] = MQTT_GROUP;
    doc["uiVersion"] = 3;
    doc["uptime"] = millis() / 1000;
    doc["heapFree"] = ESP.getFreeHeap();

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = network.wifiConnected;
    wifi["ssid"] = network.ssid;
    wifi["ip"] = network.ip;
    wifi["rssi"] = network.rssi;
    wifi["apIp"] = WiFi.softAPIP().toString();

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["connected"] = network.mqttConnected;
    mqtt["broker"] = network.mqttBroker;
    mqtt["port"] = network.mqttPort;
    mqtt["topic"] = network.mqttTopic;
    mqtt["cmdTopic"] = network.mqttCmdTopic;
    mqtt["qos"] = network.mqttQos;
    mqtt["publishLatencyMs"] = network.publishLatencyMs;
    mqtt["lastPublishTs"] = network.lastPublishTs;

    DeviceMqttConfig mqttConfig;
    deviceConfigGetMqtt(mqttConfig);
    mqtt["user"] = mqttConfig.user;

    JsonObject actuators = doc["actuators"].to<JsonObject>();
    actuators["led"] = actuatorsLedState();
    actuators["relay"] = actuatorsRelayState();
    actuators["tempThreshold"] = roundf(actuatorsGetTempThreshold() * 10.0f) / 10.0f;
    actuators["autoLed"] = actuatorsIsAutoLedEnabled();

    sendJson(200, doc);
}

void handleApiSensors() {
    SensorSnapshot snapshot;
    if (!sensorDataGet(snapshot)) {
        sendError(503, "capteur indisponible");
        return;
    }

    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["ts"] = snapshot.timestamp;
    doc["valid"] = snapshot.valid;
    doc["outlier"] = snapshot.outlier;
    doc["temp"] = roundf(snapshot.temperature * 10.0f) / 10.0f;
    doc["humidity"] = roundf(snapshot.humidity * 10.0f) / 10.0f;
    doc["potentiometer"] = roundf(snapshot.potentiometerPct);
    doc["buttonPressed"] = snapshot.buttonPressed;

    sendJson(200, doc);
}

void handleApiConfigGet() {
    DeviceMqttConfig mqttConfig;
    deviceConfigGetMqtt(mqttConfig);

    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["group"] = MQTT_GROUP;
    doc["broker"] = mqttConfig.broker;
    doc["port"] = mqttConfig.port;
    doc["user"] = mqttConfig.user;
    doc["topic"] = String("campus/") + MQTT_GROUP + "/" + DEVICE_ID + "/data";
    doc["editable"] = true;
    doc["authRequired"] = false;
    doc["persisted"] = mqttConfig.fromNvs;
    doc["note"] =
        mqttConfig.fromNvs
            ? "Configuration MQTT chargee depuis la NVS"
            : "Valeurs par defaut (secrets.h) — enregistrer pour persister";

    sendJson(200, doc);
}

void handleApiConfigPost() {
    if (!g_server.hasArg("plain")) {
        sendError(400, "corps JSON manquant");
        return;
    }

    JsonDocument doc;
    const DeserializationError err =
        deserializeJson(doc, g_server.arg("plain"));

    if (err || !securityValidateMqttConfig(doc)) {
        sendError(400, "configuration MQTT invalide");
        return;
    }

    if (!deviceConfigSaveMqtt(doc)) {
        sendError(500, "echec sauvegarde NVS");
        return;
    }

    networkApplyMqttConfig();

    JsonDocument response;
    response["ok"] = true;
    response["message"] = "configuration MQTT enregistree";
    response["broker"] = doc["broker"];
    response["port"] = doc["port"];

    sendJson(200, response);
}

void handleApiActuatorsPost() {
    if (!g_server.hasArg("plain")) {
        sendError(400, "corps JSON manquant");
        return;
    }

    JsonDocument doc;
    const DeserializationError err =
        deserializeJson(doc, g_server.arg("plain"));

    if (err || !securityValidateActuatorCommand(doc)) {
        sendError(400, "commande actionneur invalide");
        return;
    }

    const char* action = doc["action"];
    if (!actuatorsApplyCommand(action)) {
        sendError(400, "action non supportee");
        return;
    }

    JsonDocument response;
    response["ok"] = true;
    response["action"] = action;
    response["led"] = actuatorsLedState();
    response["relay"] = actuatorsRelayState();
    response["status"] = "commande appliquee";

    sendJson(200, response);
}

void handleApiThresholdGet() {
    SensorSnapshot snapshot;
    sensorDataGet(snapshot);

    JsonDocument doc;
    doc["threshold"] = roundf(actuatorsGetTempThreshold() * 10.0f) / 10.0f;
    doc["min"] = TEMP_LED_THRESHOLD_MIN;
    doc["max"] = TEMP_LED_THRESHOLD_MAX;
    doc["auto"] = actuatorsIsAutoLedEnabled();
    doc["led"] = actuatorsLedState();
    if (snapshot.valid) {
        doc["temp"] = roundf(snapshot.temperature * 10.0f) / 10.0f;
    }
    doc["rule"] = "LED allumee si temperature > seuil";

    sendJson(200, doc);
}

void handleApiThresholdPost() {
    if (!g_server.hasArg("plain")) {
        sendError(400, "corps JSON manquant");
        return;
    }

    JsonDocument doc;
    const DeserializationError err =
        deserializeJson(doc, g_server.arg("plain"));

    if (err || (!doc["threshold"].is<float>() && !doc["threshold"].is<int>())) {
        sendError(400, "seuil invalide");
        return;
    }

    const float threshold = doc["threshold"].as<float>();
    if (threshold < TEMP_LED_THRESHOLD_MIN || threshold > TEMP_LED_THRESHOLD_MAX) {
        sendError(400, "seuil hors plage");
        return;
    }

    const bool autoEnabled = doc["auto"] | true;
    actuatorsSetTempThreshold(threshold, autoEnabled, /*persist=*/true);

    SensorSnapshot snapshot;
    if (sensorDataGet(snapshot) && snapshot.valid) {
        actuatorsApplyTemperatureRule(snapshot.temperature, true);
    }

    JsonDocument response;
    response["ok"] = true;
    response["threshold"] = roundf(actuatorsGetTempThreshold() * 10.0f) / 10.0f;
    response["auto"] = actuatorsIsAutoLedEnabled();
    response["led"] = actuatorsLedState();

    sendJson(200, response);
}

void startWebServer() {
    g_server.on("/api/status", HTTP_GET, handleApiStatus);
    g_server.on("/api/sensors", HTTP_GET, handleApiSensors);
    g_server.on("/api/config", HTTP_GET, handleApiConfigGet);
    g_server.on("/api/config", HTTP_POST, handleApiConfigPost);
    g_server.on("/api/actuators", HTTP_POST, handleApiActuatorsPost);
    g_server.on("/api/threshold", HTTP_GET, handleApiThresholdGet);
    g_server.on("/api/threshold", HTTP_POST, handleApiThresholdPost);
    g_server.onNotFound(handleNotFound);

    g_server.begin();
    g_serverStarted = true;

    Serial.printf("[web] serveur HTTP demarre — http://%s/\n",
                  WiFi.localIP().toString().c_str());
}

void webTask(void* parameter) {
    (void)parameter;
    Serial.println("[web] task started");

    while (WiFi.getMode() == WIFI_OFF) {
        vTaskDelay(pdMS_TO_TICKS(50));
    }

    if (!g_serverStarted) {
        startWebServer();
    }

    for (;;) {
        if (g_serverStarted) {
            g_server.handleClient();
        }

        vTaskDelay(pdMS_TO_TICKS(WEB_HANDLE_INTERVAL_MS));
    }
}

}  // namespace

void webTaskStart() {
    xTaskCreatePinnedToCore(
        webTask,
        "web",
        TASK_WEB_STACK,
        nullptr,
        PRIORITY_WEB,
        nullptr,
        1);
}
