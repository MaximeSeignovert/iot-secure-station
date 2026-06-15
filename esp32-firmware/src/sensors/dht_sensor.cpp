#include "dht_sensor.h"

#include "../config.h"

#include <DHT.h>

static DHT g_dht(PIN_DHT22, DHT22);

bool dhtSensorInit() {
    g_dht.begin();
    return true;
}

static bool isReadingValid(float temperatureC, float humidityPct) {
    return !isnan(temperatureC)
        && !isnan(humidityPct)
        && temperatureC >= -40.0f
        && temperatureC <= 80.0f
        && humidityPct >= 0.0f
        && humidityPct <= 100.0f;
}

bool dhtSensorRead(float& temperatureC, float& humidityPct) {
    temperatureC = g_dht.readTemperature();
    humidityPct = g_dht.readHumidity();
    return isReadingValid(temperatureC, humidityPct);
}
