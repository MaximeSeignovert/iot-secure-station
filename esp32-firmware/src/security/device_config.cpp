#include "device_config.h"

#include "../config.h"
#include "../secrets.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

Preferences g_prefs;
DeviceMqttConfig g_mqttConfig;

void loadDefaults() {
    snprintf(g_mqttConfig.broker, sizeof(g_mqttConfig.broker), "%s", MQTT_BROKER);
    g_mqttConfig.port = MQTT_PORT;
    snprintf(g_mqttConfig.user, sizeof(g_mqttConfig.user), "%s", MQTT_USER);
    snprintf(g_mqttConfig.password, sizeof(g_mqttConfig.password), "%s", MQTT_PASSWORD);
    g_mqttConfig.fromNvs = false;
}

void loadFromNvs() {
    loadDefaults();

    const String broker = g_prefs.getString("broker", "");
    if (broker.length() == 0) {
        return;
    }

    snprintf(g_mqttConfig.broker, sizeof(g_mqttConfig.broker), "%s", broker.c_str());
    g_mqttConfig.port = g_prefs.getUShort("port", MQTT_PORT);

    const String user = g_prefs.getString("user", "");
    const String password = g_prefs.getString("password", "");
    snprintf(g_mqttConfig.user, sizeof(g_mqttConfig.user), "%s", user.c_str());
    snprintf(g_mqttConfig.password,
             sizeof(g_mqttConfig.password),
             "%s",
             password.c_str());
    g_mqttConfig.fromNvs = true;
}

}  // namespace

void deviceConfigInit() {
    if (!g_prefs.begin("iotcfg", /*readOnly=*/false)) {
        Serial.println("[config] echec ouverture NVS");
        loadDefaults();
        return;
    }

    loadFromNvs();

    Serial.printf("[config] MQTT %s:%u (%s)\n",
                  g_mqttConfig.broker,
                  g_mqttConfig.port,
                  g_mqttConfig.fromNvs ? "NVS" : "secrets.h");
}

bool deviceConfigGetMqtt(DeviceMqttConfig& out) {
    out = g_mqttConfig;
    return true;
}

bool deviceConfigSaveMqtt(const JsonDocument& doc) {
    const char* broker = doc["broker"];
    const uint16_t port = doc["port"].as<uint16_t>();
    const char* user = doc["user"] | "";
    const char* password = doc["password"] | "";

    g_prefs.putString("broker", broker);
    g_prefs.putUShort("port", port);
    g_prefs.putString("user", user);
    g_prefs.putString("password", password);

    snprintf(g_mqttConfig.broker, sizeof(g_mqttConfig.broker), "%s", broker);
    g_mqttConfig.port = port;
    snprintf(g_mqttConfig.user, sizeof(g_mqttConfig.user), "%s", user);
    snprintf(g_mqttConfig.password, sizeof(g_mqttConfig.password), "%s", password);
    g_mqttConfig.fromNvs = true;

    Serial.printf("[config] MQTT sauvegarde NVS — %s:%u\n", broker, port);
    return true;
}
