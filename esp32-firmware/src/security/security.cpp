#include "security.h"

#include <Arduino.h>

void securityInit() {
    Serial.println("[security] init (stub)");
}

bool securityValidateJson(const JsonDocument& doc) {
    return !doc.overflowed();
}

bool securityValidateActuatorCommand(const JsonDocument& doc) {
    if (doc.overflowed()) {
        return false;
    }

    const char* action = doc["action"];
    if (action == nullptr || action[0] == '\0') {
        return false;
    }

    return true;
}
