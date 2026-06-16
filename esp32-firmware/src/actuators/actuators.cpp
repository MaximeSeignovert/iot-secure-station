#include "actuators.h"

#include "../config.h"
#include "../security/device_config.h"

#include <Arduino.h>
#include <cstring>

namespace {

bool g_ledOn = false;
bool g_relayOn = false;
float g_tempThreshold = TEMP_LED_THRESHOLD_DEFAULT;
bool g_autoLedEnabled = true;

uint8_t levelFromState(bool state, bool activeHigh) {
    return (state == activeHigh) ? HIGH : LOW;
}

void applyOutputs() {
    digitalWrite(PIN_LED, levelFromState(g_ledOn, LED_ACTIVE_HIGH != 0));
    digitalWrite(PIN_RELAY, levelFromState(g_relayOn, RELAY_ACTIVE_HIGH != 0));
}

void setLedState(bool on) {
    if (g_ledOn == on) {
        return;
    }

    g_ledOn = on;
    applyOutputs();
}

}  // namespace

void actuatorsInit() {
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_RELAY, OUTPUT);

    TempLedConfig cfg;
    deviceConfigGetTempLed(cfg);
    g_tempThreshold = cfg.threshold;
    g_autoLedEnabled = cfg.autoEnabled;

    g_ledOn = false;
    g_relayOn = false;
    applyOutputs();

    Serial.printf("[actuators] init OK (led=%d, relay=%d, seuil=%.1f C, auto=%d)\n",
                  PIN_LED,
                  PIN_RELAY,
                  g_tempThreshold,
                  g_autoLedEnabled);
}

bool actuatorsApplyCommand(const char* action) {
    if (action == nullptr || action[0] == '\0') {
        return false;
    }

    if (strcmp(action, "led_on") == 0) {
        g_autoLedEnabled = false;
        setLedState(true);
    } else if (strcmp(action, "led_off") == 0) {
        g_autoLedEnabled = false;
        setLedState(false);
    } else if (strcmp(action, "relay_on") == 0) {
        g_relayOn = true;
        applyOutputs();
    } else if (strcmp(action, "relay_off") == 0) {
        g_relayOn = false;
        applyOutputs();
    } else {
        return false;
    }

    Serial.printf("[actuators] command=%s led=%d relay=%d auto=%d\n",
                  action,
                  g_ledOn,
                  g_relayOn,
                  g_autoLedEnabled);
    return true;
}

bool actuatorsLedState() {
    return g_ledOn;
}

bool actuatorsRelayState() {
    return g_relayOn;
}

float actuatorsGetTempThreshold() {
    return g_tempThreshold;
}

bool actuatorsIsAutoLedEnabled() {
    return g_autoLedEnabled;
}

void actuatorsSetTempThreshold(float threshold, bool autoEnabled, bool persist) {
    g_tempThreshold =
        constrain(threshold, TEMP_LED_THRESHOLD_MIN, TEMP_LED_THRESHOLD_MAX);
    g_autoLedEnabled = autoEnabled;

    if (persist) {
        deviceConfigSaveTempLed(g_tempThreshold, g_autoLedEnabled);
    }

    Serial.printf("[actuators] seuil LED=%.1f C auto=%d\n",
                  g_tempThreshold,
                  g_autoLedEnabled);
}

void actuatorsApplyTemperatureRule(float celsius, bool valid) {
    if (!g_autoLedEnabled || !valid) {
        return;
    }

    const bool shouldOn = celsius > g_tempThreshold;
    if (shouldOn == g_ledOn) {
        return;
    }

    setLedState(shouldOn);
    Serial.printf("[actuators] auto-led temp=%.1f seuil=%.1f -> %s\n",
                  celsius,
                  g_tempThreshold,
                  shouldOn ? "ON" : "OFF");
}
