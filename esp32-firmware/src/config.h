#pragma once

#define DEVICE_ID "ESP32-1"

#define SERIAL_BAUD 115200
#define WEB_SERVER_PORT 80

// DHT22 — broche DATA sur D4 (GPIO4)
#define PIN_DHT22 4

// DHT22 : minimum ~2 s entre deux lectures
#define SENSOR_INTERVAL_MS 2000

// MQTT — topic : campus/<MQTT_GROUP>/<DEVICE_ID>/data
#define MQTT_GROUP "groupe"

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

// Stockage offline LittleFS (etape 6)
#define STORAGE_QUEUE_PATH "/offline_queue.jsonl"
#define STORAGE_MAX_BYTES (32 * 1024)
#define STORAGE_MAX_LINE_BYTES 192
