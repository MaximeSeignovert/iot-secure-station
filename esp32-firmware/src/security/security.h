#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>

void securityInit();
bool securityAuthorizeApi(const char* token);
bool securityValidateJson(const JsonDocument& doc);
bool securityValidateActuatorCommand(const JsonDocument& doc);
bool securityValidateMqttConfig(const JsonDocument& doc);
