#include "security.h"

#include <Arduino.h>

void securityInit() {
    Serial.println("[security] init (stub)");
}

bool securityValidateJson(const JsonDocument& doc) {
    return !doc.overflowed();
}
