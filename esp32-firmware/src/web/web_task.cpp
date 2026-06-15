#include "web_task.h"

#include "../config.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void webTask(void* parameter) {
    (void)parameter;
    Serial.println("[web] task started (serveur stub)");

    for (;;) {
        Serial.println("[web] serveur HTTP non démarré");
        vTaskDelay(pdMS_TO_TICKS(WEB_INTERVAL_MS));
    }
}

void webTaskStart() {
    xTaskCreatePinnedToCore(
        webTask,
        "web",
        TASK_WEB_STACK,
        nullptr,
        PRIORITY_WEB,
        nullptr,
        1);
}
