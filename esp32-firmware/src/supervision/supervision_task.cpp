#include "supervision_task.h"

#include "../config.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void supervisionTask(void* parameter) {
    (void)parameter;
    Serial.println("[supervision] task started");

    for (;;) {
        const uint32_t uptimeSec = millis() / 1000;
        const uint32_t heapFree = ESP.getFreeHeap();

        Serial.printf("[supervision] uptime=%lus heap_free=%u bytes\n",
                      static_cast<unsigned long>(uptimeSec),
                      heapFree);

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
