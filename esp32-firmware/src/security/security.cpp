#include "security.h"

#include "../secrets.h"
#include "device_config.h"

#include <Arduino.h>
#include <cstring>

#ifndef API_TOKEN
#define API_TOKEN ""
#endif

namespace {

bool isAllowedAction(const char* action) {
    return strcmp(action, "led_on") == 0 || strcmp(action, "led_off") == 0;
}

bool isPrintableField(const char* value, size_t maxLen) {
    if (value == nullptr) {
        return true;
    }

    const size_t len = strlen(value);
    if (len >= maxLen) {
        return false;
    }

    for (size_t i = 0; i < len; ++i) {
        const unsigned char c = static_cast<unsigned char>(value[i]);
        if (c < 0x20 || c == 0x7F) {
            return false;
        }
    }

    return true;
}

}  // namespace

void securityInit() {
    deviceConfigInit();

    if (API_TOKEN[0] == '\0') {
        Serial.println("[security] ATTENTION: API_TOKEN vide — API non protegee");
    } else {
        Serial.println("[security] auth API active");
    }
}

bool securityAuthorizeApi(const char* token) {
    if (API_TOKEN[0] == '\0') {
        return true;
    }

    if (token == nullptr || token[0] == '\0') {
        return false;
    }

    return strcmp(token, API_TOKEN) == 0;
}

bool securityValidateJson(const JsonDocument& doc) {
    return !doc.overflowed();
}

bool securityValidateActuatorCommand(const JsonDocument& doc) {
    if (!securityValidateJson(doc)) {
        return false;
    }

    const char* action = doc["action"];
    if (action == nullptr || action[0] == '\0' || !isAllowedAction(action)) {
        return false;
    }

    return true;
}

bool securityValidateMqttConfig(const JsonDocument& doc) {
    if (!securityValidateJson(doc)) {
        return false;
    }

    const char* broker = doc["broker"];
    if (broker == nullptr || broker[0] == '\0' || !isPrintableField(broker, 64)) {
        return false;
    }

    if (!doc["port"].is<uint16_t>() && !doc["port"].is<int>()) {
        return false;
    }

    const int port = doc["port"].as<int>();
    if (port < 1 || port > 65535) {
        return false;
    }

    const char* user = doc["user"] | "";
    const char* password = doc["password"] | "";
    if (!isPrintableField(user, 33) || !isPrintableField(password, 65)) {
        return false;
    }

    return true;
}
