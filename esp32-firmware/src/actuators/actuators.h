#pragma once

void actuatorsInit();
bool actuatorsApplyCommand(const char* action);
bool actuatorsLedState();
bool actuatorsRelayState();

float actuatorsGetTempThreshold();
bool actuatorsIsAutoLedEnabled();
void actuatorsSetTempThreshold(float threshold, bool autoEnabled, bool persist);
void actuatorsApplyTemperatureRule(float celsius, bool valid);
