#include "sensor_data.h"

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

static SensorSnapshot g_snapshot;
static SemaphoreHandle_t g_mutex = nullptr;

void sensorDataInit() {
    g_mutex = xSemaphoreCreateMutex();
}

void sensorDataUpdate(const SensorSnapshot& snapshot) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return;
    }
    g_snapshot = snapshot;
    xSemaphoreGive(g_mutex);
}

bool sensorDataGet(SensorSnapshot& out) {
    if (g_mutex == nullptr || xSemaphoreTake(g_mutex, pdMS_TO_TICKS(100)) != pdTRUE) {
        return false;
    }
    out = g_snapshot;
    xSemaphoreGive(g_mutex);
    return true;
}
