#pragma once

#include <Arduino.h>

struct SensorSnapshot {
    float temperature = 0.0f;
    float humidity = 0.0f;
    bool valid = false;
    unsigned long timestamp = 0;
};

void sensorDataInit();
void sensorDataUpdate(const SensorSnapshot& snapshot);
bool sensorDataGet(SensorSnapshot& out);
