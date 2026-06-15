#include <Arduino.h>
#include <WiFi.h>

#include "config.h"
#include "actuators/actuators.h"
#include "network/network_task.h"
#include "security/security.h"
#include "sensors/sensor_data.h"
#include "sensors/sensor_task.h"
#include "storage/storage.h"
#include "supervision/supervision_task.h"
#include "web/web_task.h"

void setup() {
    Serial.begin(SERIAL_BAUD);
    Serial.println();
    Serial.println("IoT Secure Station - boot");

    sensorDataInit();
    actuatorsInit();
    storageInit();
    securityInit();

    WiFi.mode(WIFI_STA);
    WiFi.persistent(false);

    sensorTaskStart();
    networkTaskStart();
    webTaskStart();
    supervisionTaskStart();

    Serial.println("[main] FreeRTOS tasks running");
}

void loop() {
    vTaskDelay(portMAX_DELAY);
}
