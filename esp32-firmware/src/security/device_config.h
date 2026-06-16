#pragma once

#include "../config.h"
#include <Arduino.h>
#include <ArduinoJson.h>

struct DeviceMqttConfig {
    char broker[64] = {};
    uint16_t port = 1883;
    char user[33] = {};
    char password[65] = {};
    bool fromNvs = false;
};

void deviceConfigInit();
bool deviceConfigGetMqtt(DeviceMqttConfig& out);
bool deviceConfigSaveMqtt(const JsonDocument& doc);

struct TempLedConfig {
    float threshold = TEMP_LED_THRESHOLD_DEFAULT;
    bool autoEnabled = true;
};

void deviceConfigGetTempLed(TempLedConfig& out);
bool deviceConfigSaveTempLed(float threshold, bool autoEnabled);
