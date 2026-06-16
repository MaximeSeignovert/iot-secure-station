#include "device_config.h"

#include "../config.h"
#include "../secrets.h"

#include <Arduino.h>
#include <Preferences.h>

namespace {

Preferences g_prefs;
DeviceMqttConfig g_mqttConfig;
TempLedConfig g_tempLedConfig;

void loadDefaults() {
    snprintf(g_mqttConfig.broker, sizeof(g_mqttConfig.broker), "%s", MQTT_BROKER);
    g_mqttConfig.port = MQTT_PORT;
    snprintf(g_mqttConfig.user, sizeof(g_mqttConfig.user), "%s", MQTT_USER);
    snprintf(g_mqttConfig.password, sizeof(g_mqttConfig.password), "%s", MQTT_PASSWORD);
    g_mqttConfig.fromNvs = false;
    g_tempLedConfig.threshold = TEMP_LED_THRESHOLD_DEFAULT;
    g_tempLedConfig.autoEnabled = true;
}

void loadFromNvs() {
    loadDefaults();

    g_tempLedConfig.threshold =
        g_prefs.getFloat("tempTh", TEMP_LED_THRESHOLD_DEFAULT);
    g_tempLedConfig.autoEnabled = g_prefs.getBool("tempAuto", true);

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
    Serial.printf("[config] seuil LED %.1f C (auto=%d)\n",
                  g_tempLedConfig.threshold,
                  g_tempLedConfig.autoEnabled);
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

void deviceConfigGetTempLed(TempLedConfig& out) {
    out = g_tempLedConfig;
}

bool deviceConfigSaveTempLed(float threshold, bool autoEnabled) {
    const float clamped =
        constrain(threshold, TEMP_LED_THRESHOLD_MIN, TEMP_LED_THRESHOLD_MAX);

    g_prefs.putFloat("tempTh", clamped);
    g_prefs.putBool("tempAuto", autoEnabled);

    g_tempLedConfig.threshold = clamped;
    g_tempLedConfig.autoEnabled = autoEnabled;

    Serial.printf("[config] seuil LED sauvegarde NVS — %.1f C (auto=%d)\n",
                  clamped,
                  autoEnabled);
    return true;
}
