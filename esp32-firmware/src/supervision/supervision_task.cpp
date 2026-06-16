#include "supervision_task.h"

#include "../config.h"
#include "../network/network_task.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void supervisionTask(void* parameter) {
    (void)parameter;
    Serial.println("[supervision] task started");

    for (;;) {
        const uint32_t uptimeSec = millis() / 1000;
        const uint32_t heapFree = ESP.getFreeHeap();
        NetworkStatus network;
        networkGetStatus(network);

        Serial.printf("[supervision] uptime=%lus heap_free=%u bytes mqtt=%d latence=%ums\n",
                      static_cast<unsigned long>(uptimeSec),
                      heapFree,
                      network.mqttConnected,
                      network.publishLatencyMs);

        vTaskDelay(pdMS_TO_TICKS(SUPERVISION_INTERVAL_MS));
    }
}

void supervisionTaskStart() {
    xTaskCreatePinnedToCore(
        supervisionTask,
        "supervision",
        TASK_SUPERVISION_STACK,
        nullptr,
        PRIORITY_SUPERVISION,
        nullptr,
        0);
}
