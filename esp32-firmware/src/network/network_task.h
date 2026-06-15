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
};

void networkTaskStart();
void networkGetStatus(NetworkStatus& out);
void networkApplyMqttConfig();
