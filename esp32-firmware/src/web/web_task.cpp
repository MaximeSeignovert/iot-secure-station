#include "web_task.h"

#include "../config.h"
#include "../network/network_task.h"
#include "../security/security.h"
#include "../sensors/sensor_data.h"
#include "../secrets.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
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

bool serveStaticFile(const char* path, const char* contentType) {
    if (!LittleFS.exists(path)) {
        return false;
    }

    File file = LittleFS.open(path, "r");
    if (!file) {
        return false;
    }

    g_server.streamFile(file, contentType);
    file.close();
    return true;
}

void handleApiStatus() {
    NetworkStatus network;
    networkGetStatus(network);

    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["group"] = MQTT_GROUP;
    doc["uptime"] = millis() / 1000;
    doc["heapFree"] = ESP.getFreeHeap();

    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["connected"] = network.wifiConnected;
    wifi["ssid"] = network.ssid;
    wifi["ip"] = network.ip;
    wifi["rssi"] = network.rssi;

    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["connected"] = network.mqttConnected;
    mqtt["broker"] = network.mqttBroker;
    mqtt["port"] = network.mqttPort;
    mqtt["topic"] = network.mqttTopic;
    mqtt["user"] = MQTT_USER;

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
    doc["temp"] = roundf(snapshot.temperature * 10.0f) / 10.0f;
    doc["humidity"] = roundf(snapshot.humidity * 10.0f) / 10.0f;

    sendJson(200, doc);
}

void handleApiConfigGet() {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["group"] = MQTT_GROUP;
    doc["broker"] = MQTT_BROKER;
    doc["port"] = MQTT_PORT;
    doc["user"] = MQTT_USER;
    doc["topic"] = String("campus/") + MQTT_GROUP + "/" + DEVICE_ID + "/data";
    doc["editable"] = false;
    doc["note"] = "Modifier secrets.h puis recompiler pour changer la config MQTT";

    sendJson(200, doc);
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

    JsonDocument response;
    response["ok"] = true;
    response["action"] = doc["action"].as<const char*>();
    response["status"] = "stub — actionneurs non cables (etape 3 reportee)";

    sendJson(200, response);
}

void handleNotFound() {
    const String& uri = g_server.uri();

    if (uri == "/" || uri == "/index.html") {
        if (serveStaticFile("/index.html", "text/html")) {
            return;
        }
    } else if (uri == "/style.css") {
        if (serveStaticFile("/style.css", "text/css")) {
            return;
        }
    } else if (uri == "/app.js") {
        if (serveStaticFile("/app.js", "application/javascript")) {
            return;
        }
    }

    sendError(404, "ressource introuvable");
}

void startWebServer() {
    g_server.on("/api/status", HTTP_GET, handleApiStatus);
    g_server.on("/api/sensors", HTTP_GET, handleApiSensors);
    g_server.on("/api/config", HTTP_GET, handleApiConfigGet);
    g_server.on("/api/actuators", HTTP_POST, handleApiActuatorsPost);
    g_server.onNotFound(handleNotFound);

    g_server.begin();
    g_serverStarted = true;

    Serial.printf("[web] serveur HTTP demarre — http://%s/\n",
                  WiFi.localIP().toString().c_str());
}

void webTask(void* parameter) {
    (void)parameter;
    Serial.println("[web] task started");

    if (!LittleFS.begin(/*formatOnFail=*/true)) {
        Serial.println("[web] echec montage LittleFS");
    } else {
        Serial.println("[web] LittleFS monte");
    }

    for (;;) {
        if (WiFi.status() == WL_CONNECTED) {
            if (!g_serverStarted) {
                startWebServer();
            }
            g_server.handleClient();
        } else if (g_serverStarted) {
            g_server.stop();
            g_serverStarted = false;
            Serial.println("[web] serveur HTTP arrete (WiFi perdu)");
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
