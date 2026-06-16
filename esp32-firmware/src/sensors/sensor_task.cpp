#include "sensor_task.h"

#include "../config.h"
#include "dht_sensor.h"
#include "sensor_data.h"
#include "../actuators/actuators.h"

#include <Arduino.h>
#include <math.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

namespace {

template <size_t N>
struct RollingAverage {
    float values[N] = {};
    size_t count = 0;
    size_t index = 0;

    void push(float value) {
        values[index] = value;
        index = (index + 1) % N;
        if (count < N) {
            ++count;
        }
    }

    float average() const {
        if (count == 0) {
            return 0.0f;
        }

        float sum = 0.0f;
        for (size_t i = 0; i < count; ++i) {
            sum += values[i];
        }
        return sum / static_cast<float>(count);
    }
};

uint64_t epochMsNow() {
    struct tm tmNow;
    if (getLocalTime(&tmNow, 5)) {
        return static_cast<uint64_t>(time(nullptr)) * 1000ULL;
    }

    // Fallback robuste si NTP indisponible.
    return static_cast<uint64_t>(millis());
}

}  // namespace

static void sensorTask(void* parameter) {
    (void)parameter;
    Serial.printf("[sensor] DHT22=%d, potentiometre=%d, bouton=%d\n",
                  PIN_DHT22,
                  PIN_POTENTIOMETER,
                  PIN_BUTTON);

    dhtSensorInit();
    pinMode(PIN_BUTTON, INPUT_PULLUP);
    analogReadResolution(12);

    SensorSnapshot snapshot;
    SensorSnapshot lastValid = {};
    bool hasValid = false;
    RollingAverage<SENSOR_FILTER_WINDOW> tempAverage;
    RollingAverage<SENSOR_FILTER_WINDOW> humAverage;

    for (;;) {
        float temperature = 0.0f;
        float humidity = 0.0f;
        const bool ok = dhtSensorRead(temperature, humidity);

        snapshot.timestamp = epochMsNow();
        snapshot.valid = ok;
        snapshot.outlier = false;
        snapshot.buttonPressed = digitalRead(PIN_BUTTON) == LOW;
        const int potRaw = analogRead(PIN_POTENTIOMETER);
        snapshot.potentiometerPct =
            constrain((static_cast<float>(potRaw) / 4095.0f) * 100.0f, 0.0f, 100.0f);

        if (ok) {
            bool isOutlier = false;
            if (hasValid) {
                if (fabsf(temperature - lastValid.temperature) > SENSOR_TEMP_SPIKE_MAX ||
                    fabsf(humidity - lastValid.humidity) > SENSOR_HUM_SPIKE_MAX) {
                    isOutlier = true;
                }
            }

            if (!isOutlier) {
                tempAverage.push(temperature);
                humAverage.push(humidity);
                snapshot.temperature = tempAverage.average();
                snapshot.humidity = humAverage.average();
                lastValid = snapshot;
                hasValid = true;
            } else if (hasValid) {
                snapshot.temperature = lastValid.temperature;
                snapshot.humidity = lastValid.humidity;
                snapshot.outlier = true;
            }

            Serial.printf("[sensor] temp=%.1f C hum=%.1f %% pot=%.0f%% btn=%d%s\n",
                          snapshot.temperature,
                          snapshot.humidity,
                          snapshot.potentiometerPct,
                          snapshot.buttonPressed,
                          snapshot.outlier ? " outlier" : "");
        } else if (hasValid) {
            snapshot.temperature = lastValid.temperature;
            snapshot.humidity = lastValid.humidity;
            Serial.println("[sensor] erreur lecture — dernière valeur conservée");
        } else {
            Serial.println("[sensor] erreur lecture — vérifie le câblage (VCC, DATA, GND)");
        }

        sensorDataUpdate(snapshot);
        if (snapshot.valid) {
            actuatorsApplyTemperatureRule(snapshot.temperature, true);
        }
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
