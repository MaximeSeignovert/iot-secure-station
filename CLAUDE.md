# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project

**IoT Secure Station** — a Master II academic project (TP) building a secure, autonomous IoT station: an ESP32 firmware that acquires sensor data, drives actuators, and reports to a central supervision stack (MQTT + Node-RED + NoSQL) over Docker. The graded specification lives in `README.md` and the PDF it references; treat `README.md` as the source of truth for requirements and the badge/grading criteria.

**Current state:** scaffold only. `src/main.cpp` is a boilerplate stub, the `src/` subdirectories (`sensors/`, `actuators/`, `network/`, `storage/`, `web/`, `security/`) contain only `.gitkeep`, `node-red/flows.json` has just an empty tab, and `docs/rapport.md` is a skeleton. Most work is greenfield implementation against the README spec.

## Commands

### Firmware (PlatformIO, from `esp32-firmware/`)
```bash
pio run                 # Build firmware
pio run -t upload       # Flash firmware over serial
pio run -t uploadfs     # Flash the data/ web UI to LittleFS (separate from code!)
pio device monitor      # Serial monitor @ 115200 baud
```
Target: `esp32dev`, Arduino framework, **LittleFS** filesystem. Libs: ArduinoJson v7, PubSubClient (MQTT).

### Infrastructure (from `docker/`)
```bash
docker compose up -d    # Start Mosquitto (1883/9001) + InfluxDB (8086) + Node-RED (1880)
docker compose down
docker compose logs -f mosquitto
./test/simulate-esp32.sh loop   # fake ESP32 publisher to exercise the whole chain
```
- **Mosquitto** is authenticated (`allow_anonymous false`); config in `docker/mosquitto/config/` (`mosquitto.conf` + hashed `passwd`). MQTT users: `esp32`, `nodered` (plaintext passwords in `docker/.env`, gitignored).
- **InfluxDB 1.x**, database `iot`, measurement `sensors` (tag `device`, numeric fields). Auto-created from env vars in `docker/.env`.
- **Node-RED** at http://localhost:1880, dashboard at `/ui`. The flow lives in `node-red/flows.json` (+ `flows_cred.json` for credentials, plaintext because `credentialSecret: false` in `settings.js`). Both are bind-mounted **read-write**, so UI edits persist to the repo; a container recreate reloads them. The nodes `node-red-dashboard` and `node-red-contrib-influxdb` are installed into the `/data` volume (see `docs/suivi-projet.md`).

> Full infra status, conventions and remaining work: **`docs/suivi-projet.md`**.

## Architecture & hard constraints

The grading enforces a specific design. When implementing, respect these (see README "Contraintes techniques"):

- **Modular structure is mandatory.** Each concern goes in its `src/` subdirectory. **No monolithic code, no business logic in `loop()`, no `delay()`.**
- **FreeRTOS multitasking required** — separate tasks for: sensor acquisition, network communication, web server, system supervision. Use task notifications/queues, not blocking delays.
- **Sensor acquisition** must filter readings, attach timestamps, and detect aberrant values.
- **Offline mode**: when MQTT is down, buffer readings to a local JSON file (LittleFS) and replay them automatically on reconnect. The on-wire JSON shape is fixed:
  ```json
  { "device": "ESP32-X", "ts": 0, "data": { "temp": 0, "humidity": 0 } }
  ```
- **MQTT**: topic `campus/<groupe>/<deviceID>/data`, auto-reconnect, QoS ≥ 1, authenticated, and able to receive server→device commands. (Note: `node-red/README.md` documents a different `station/...` topic scheme — reconcile these before wiring the flow.)
- **Embedded web UI** lives in `esp32-firmware/data/` as separate `index.html` / `app.js` / `style.css` (served from LittleFS): live sensor display, connection status, MQTT config, actuator commands.
- **Security baseline**: MQTT auth, validation of all incoming JSON, protected local API.
- **Observability**: periodically print free heap, uptime, and publish latency.

### Data flow
ESP32 (sensors → filter/timestamp → publish, or buffer offline) → MQTT (Mosquitto) → Node-RED (store in NoSQL, dashboard, send commands back) → optional Grafana for time-series history and alerting (bonus).

## Deliverables (`docs/`)
`architecture.drawio` (diagram), `rapport.md` (≤ 5 pages technical report), `screenshots/`. Keep these updated as features land — they are graded.
