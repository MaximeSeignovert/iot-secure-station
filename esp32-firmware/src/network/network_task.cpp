#include "network_task.h"

#include "../config.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Étape 4 : WiFi + MQTT (PubSubClient)
// - inclure secrets.h (copie locale de secrets.h.example)
// - publier JSON { device, ts, data: { temp, humidity } }
// - topic : campus/<MQTT_GROUP>/<DEVICE_ID>/data

static void networkTask(void* parameter) {
    (void)parameter;
    Serial.println("[network] en attente — WiFi + MQTT (étape 4)");

    for (;;) {
        Serial.println("[network] offline — non configuré");
        vTaskDelay(pdMS_TO_TICKS(NETWORK_INTERVAL_MS));
    }
}

void networkTaskStart() {
    xTaskCreatePinnedToCore(
        networkTask,
        "network",
        TASK_NETWORK_STACK,
        nullptr,
        PRIORITY_NETWORK,
        nullptr,
        0);
}
