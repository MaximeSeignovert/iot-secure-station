#include "sensor_task.h"

#include "../config.h"
#include "dht_sensor.h"
#include "sensor_data.h"

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

static void sensorTask(void* parameter) {
    (void)parameter;
    Serial.println("[sensor] DHT22 sur GPIO4 (D4)");

    dhtSensorInit();

    SensorSnapshot snapshot;
    SensorSnapshot lastValid = {};
    bool hasValid = false;

    for (;;) {
        float temperature = 0.0f;
        float humidity = 0.0f;
        const bool ok = dhtSensorRead(temperature, humidity);

        snapshot.timestamp = millis();
        snapshot.valid = ok;

        if (ok) {
            snapshot.temperature = temperature;
            snapshot.humidity = humidity;
            lastValid = snapshot;
            hasValid = true;

            Serial.printf("[sensor] temp=%.1f C  hum=%.1f %%\n",
                          snapshot.temperature,
                          snapshot.humidity);
        } else if (hasValid) {
            snapshot.temperature = lastValid.temperature;
            snapshot.humidity = lastValid.humidity;
            Serial.println("[sensor] erreur lecture — dernière valeur conservée");
        } else {
            Serial.println("[sensor] erreur lecture — vérifie le câblage (VCC, DATA, GND)");
        }

        sensorDataUpdate(snapshot);
        vTaskDelay(pdMS_TO_TICKS(SENSOR_INTERVAL_MS));
    }
}

void sensorTaskStart() {
    xTaskCreatePinnedToCore(
        sensorTask,
        "sensor",
        TASK_SENSOR_STACK,
        nullptr,
        PRIORITY_SENSOR,
        nullptr,
        1);
}
