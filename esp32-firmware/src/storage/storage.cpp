#include "storage.h"

#include "../config.h"

#include <Arduino.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace {

SemaphoreHandle_t g_fsMutex = nullptr;
bool g_mounted = false;

void storageFsLockInternal() {
    if (g_fsMutex != nullptr) {
        xSemaphoreTake(g_fsMutex, portMAX_DELAY);
    }
}

void storageFsUnlockInternal() {
    if (g_fsMutex != nullptr) {
        xSemaphoreGive(g_fsMutex);
    }
}

bool ensureMounted() {
    if (g_mounted) {
        return true;
    }

    if (LittleFS.begin(/*formatOnFail=*/true)) {
        g_mounted = true;
        return true;
    }

    Serial.println("[storage] echec montage LittleFS");
    return false;
}

bool buildPayload(const SensorSnapshot& snapshot, char* out, size_t outSize) {
    JsonDocument doc;
    doc["device"] = DEVICE_ID;
    doc["ts"] = snapshot.timestamp;

    JsonObject data = doc["data"].to<JsonObject>();
    data["temp"] = roundf(snapshot.temperature * 10.0f) / 10.0f;
    data["humidity"] = roundf(snapshot.humidity * 10.0f) / 10.0f;

    const size_t length = serializeJson(doc, out, outSize);
    return length > 0 && length < outSize;
}

bool trimOldestLine() {
    File file = LittleFS.open(STORAGE_QUEUE_PATH, "r");
    if (!file) {
        return false;
    }

    String remaining;
    bool skippedFirst = false;

    while (file.available()) {
        const String line = file.readStringUntil('\n');
        if (line.length() == 0) {
            continue;
        }

        if (!skippedFirst) {
            skippedFirst = true;
            Serial.println("[storage] fichier plein — suppression entree la plus ancienne");
            continue;
        }

        remaining += line;
        remaining += '\n';
    }
    file.close();

    file = LittleFS.open(STORAGE_QUEUE_PATH, "w");
    if (!file) {
        return false;
    }

    if (remaining.length() > 0) {
        file.print(remaining);
    }
    file.close();
    return true;
}

bool getQueueFileSize(size_t& outSize) {
    File file = LittleFS.open(STORAGE_QUEUE_PATH, "r");
    if (!file) {
        return false;
    }

    outSize = file.size();
    file.close();
    return true;
}

bool appendLine(const char* payload) {
    if (!ensureMounted()) {
        return false;
    }

    storageFsLockInternal();

    const size_t payloadLen = strlen(payload);

    while (LittleFS.exists(STORAGE_QUEUE_PATH)) {
        size_t currentSize = 0;
        if (!getQueueFileSize(currentSize)) {
            storageFsUnlockInternal();
            Serial.println("[storage] echec lecture taille file d'attente");
            return false;
        }

        if (currentSize + payloadLen + 1 < STORAGE_MAX_BYTES) {
            break;
        }

        if (!trimOldestLine()) {
            storageFsUnlockInternal();
            Serial.println("[storage] file plein, purge impossible");
            return false;
        }
    }

    File file = LittleFS.open(STORAGE_QUEUE_PATH, "a");
    if (!file) {
        storageFsUnlockInternal();
        Serial.println("[storage] echec ouverture file d'attente");
        return false;
    }

    file.println(payload);
    file.close();

    storageFsUnlockInternal();

    Serial.printf("[storage] enqueue OK (%u o)\n", static_cast<unsigned>(payloadLen));
    return true;
}

}  // namespace

void storageFsLock() {
    storageFsLockInternal();
}

void storageFsUnlock() {
    storageFsUnlockInternal();
}

void storageInit() {
    g_fsMutex = xSemaphoreCreateMutex();
    if (g_fsMutex == nullptr) {
        Serial.println("[storage] echec creation mutex LittleFS");
        return;
    }

    if (ensureMounted()) {
        Serial.println("[storage] init OK — LittleFS pret");
    }
}

bool storageEnqueue(const SensorSnapshot& snapshot) {
    char payload[STORAGE_MAX_LINE_BYTES];
    if (!buildPayload(snapshot, payload, sizeof(payload))) {
        Serial.println("[storage] echec serialisation JSON");
        return false;
    }

    return appendLine(payload);
}

size_t storageFlush(StoragePublishCallback publish) {
    if (publish == nullptr || !ensureMounted()) {
        return 0;
    }

    storageFsLockInternal();

    if (!LittleFS.exists(STORAGE_QUEUE_PATH)) {
        storageFsUnlockInternal();
        return 0;
    }

    File file = LittleFS.open(STORAGE_QUEUE_PATH, "r");
    if (!file) {
        storageFsUnlockInternal();
        Serial.println("[storage] echec lecture file d'attente");
        return 0;
    }

    String pending;
    size_t flushed = 0;
    size_t corrupted = 0;

    while (file.available()) {
        const String line = file.readStringUntil('\n');
        if (line.length() == 0) {
            continue;
        }

        JsonDocument doc;
        const DeserializationError err = deserializeJson(doc, line);
        if (err || !doc["device"].is<const char*>() || !doc["ts"].is<unsigned long>() ||
            !doc["data"].is<JsonObject>()) {
            ++corrupted;
            Serial.println("[storage] ligne corrompue ignoree");
            continue;
        }

        if (publish(line.c_str())) {
            ++flushed;
        } else {
            pending += line;
            pending += '\n';
            Serial.println("[storage] republication echouee, entree conservee");

            while (file.available()) {
                const String rest = file.readStringUntil('\n');
                if (rest.length() == 0) {
                    continue;
                }
                pending += rest;
                pending += '\n';
            }
            break;
        }
    }
    file.close();

    if (corrupted > 0) {
        Serial.printf("[storage] %u ligne(s) corrompue(s) supprimee(s)\n",
                      static_cast<unsigned>(corrupted));
    }

    file = LittleFS.open(STORAGE_QUEUE_PATH, "w");
    if (file) {
        if (pending.length() > 0) {
            file.print(pending);
        }
        file.close();
    } else {
        Serial.println("[storage] echec reecriture file d'attente");
    }

    storageFsUnlockInternal();

    if (flushed > 0) {
        Serial.printf("[storage] flush OK — %u message(s) republie(s)\n",
                      static_cast<unsigned>(flushed));
    }

    return flushed;
}
