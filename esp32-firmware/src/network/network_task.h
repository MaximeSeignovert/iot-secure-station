#pragma once

#include <Arduino.h>

struct NetworkStatus {
    bool wifiConnected = false;
    bool mqttConnected = false;
    int32_t rssi = 0;
    char ip[16] = {};
    char ssid[33] = {};
    char mqttBroker[64] = {};
    uint16_t mqttPort = 0;
    char mqttTopic[96] = {};
    char mqttCmdTopic[96] = {};
    uint16_t mqttQos = 1;
    uint32_t publishLatencyMs = 0;
    uint64_t lastPublishTs = 0;
};

// Initialise la pile WiFi (mode AP+STA) — appeler depuis setup() avant les tasks.
void networkInitStack();

void networkTaskStart();
void networkGetStatus(NetworkStatus& out);
void networkApplyMqttConfig();
