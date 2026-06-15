#pragma once

#include <ArduinoJson.h>

void securityInit();
bool securityValidateJson(const JsonDocument& doc);
