#pragma once

#define DEVICE_ID "esp32-1"

#define SERIAL_BAUD 115200
#define WEB_SERVER_PORT 80

// DHT22 — broche DATA sur D4 (GPIO4)
#define PIN_DHT22 4
#define PIN_POTENTIOMETER 34
#define PIN_BUTTON 27

// Actionneurs
#define PIN_LED 5
#define PIN_RELAY 26
#define LED_ACTIVE_HIGH 1
#define RELAY_ACTIVE_HIGH 1

// Seuil LED automatique (température °C)
#define TEMP_LED_THRESHOLD_DEFAULT 30.0f
#define TEMP_LED_THRESHOLD_MIN 10.0f
#define TEMP_LED_THRESHOLD_MAX 50.0f

// DHT22 : minimum ~2 s entre deux lectures
#define SENSOR_INTERVAL_MS 2000
#define SENSOR_FILTER_WINDOW 5
#define SENSOR_TEMP_SPIKE_MAX 4.0f
#define SENSOR_HUM_SPIKE_MAX 15.0f

// MQTT — topic : campus/<MQTT_GROUP>/<DEVICE_ID>/data
#define MQTT_GROUP "g1"

// Tailles de pile (bytes)
#define TASK_SENSOR_STACK 4096
#define TASK_NETWORK_STACK 8192
#define TASK_WEB_STACK 10240
#define TASK_SUPERVISION_STACK 4096

// Priorités FreeRTOS (plus élevé = plus prioritaire)
#define PRIORITY_SUPERVISION 1
#define PRIORITY_SENSOR 2
#define PRIORITY_WEB 2
#define PRIORITY_NETWORK 3

// Intervalles (ms)
#define NETWORK_INTERVAL_MS 5000
#define WEB_HANDLE_INTERVAL_MS 50
#define WEB_INTERVAL_MS 10000
#define SUPERVISION_INTERVAL_MS 10000
#define NTP_SYNC_INTERVAL_MS 3600000UL

// Stockage offline LittleFS (etape 6)
#define STORAGE_QUEUE_PATH "/offline_queue.jsonl"
#define STORAGE_MAX_BYTES (32 * 1024)
#define STORAGE_MAX_LINE_BYTES 256
