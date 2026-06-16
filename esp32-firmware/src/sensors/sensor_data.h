#pragma once

#include <Arduino.h>

struct SensorSnapshot {
    float temperature = 0.0f;
    float humidity = 0.0f;
    float potentiometerPct = 0.0f;
    bool buttonPressed = false;
    bool valid = false;
    bool outlier = false;
    uint64_t timestamp = 0;
};

void sensorDataInit();
void sensorDataUpdate(const SensorSnapshot& snapshot);
bool sensorDataGet(SensorSnapshot& out);
