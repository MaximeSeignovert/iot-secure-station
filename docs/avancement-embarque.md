# Avancement — Développeur embarqué

**Projet :** IoT Secure Station (TP Master II)  
**Rôle :** Firmware ESP32 — PlatformIO, FreeRTOS, DHT22, MQTT, web embarquée  
**Matériel :** ESP32 DevKit (CP2102, COM8 en local), DHT22 sur D4  
**Dernière mise à jour :** 2026-06-15

---

## Étape en cours : 8 — Démo bout en bout

| Tâche | Statut |
|-------|--------|
| Validation MQTT + Node-RED live | ⬜ |
| Test mode offline → flush | ⬜ |
| Captures / rapport | ⬜ |

---

## Étapes terminées

| # | Étape | Détail |
|---|-------|--------|
| 0 | Chaîne outil | PlatformIO 6.1.18, pilote CP2102, flash OK |
| 1 | FreeRTOS | 4 tâches : sensor, network, web, supervision — `loop()` vide |
| 2 | DHT22 | GPIO4 (D4), ~29 °C / ~40 % humidité validé en série |
| 4 | WiFi + MQTT | Connexion WiFi OK (2,4 GHz), publication JSON, topic `campus/groupe/ESP32-1/data` |
| 5 | Interface web embarquée | Serveur HTTP + LittleFS, API REST, dashboard live |
| 6 | Stockage offline JSON | File d'attente LittleFS, enqueue si MQTT down, flush auto à la reconnexion |
| 7 | Sécurité + Node-RED | Auth API, validation JSON, config MQTT NVS, flux Node-RED `campus/#` |

## Reporté

| # | Étape | Note |
|---|-------|------|
| 3 | LED seuil température | Câblage non fonctionnel — reporté |

---

## Roadmap restante

| # | Étape | Statut |
|---|-------|--------|
| 8 | Démo bout en bout | ⬜ Prochaine |

---

## Étape 7 — Sécurité + Node-RED (détail)

| Tâche | Statut |
|-------|--------|
| Auth API locale (`X-Api-Token`) sur POST | ✅ |
| Validation JSON actionneurs (whitelist) | ✅ |
| Validation JSON config MQTT | ✅ |
| Persistance broker/port/user/pass en NVS | ✅ |
| `POST /api/config` + formulaire web | ✅ |
| Flux Node-RED `campus/#` | ✅ |

**Auth :** définir `API_TOKEN` dans `secrets.h` (voir `secrets.h.example`). Header requis : `X-Api-Token`.

**NVS :** namespace `iotcfg` — surcharge les valeurs `secrets.h` après enregistrement via l'interface web.

**Node-RED :** `docker compose up` puis http://localhost:1880 — flux importé depuis `node-red/flows.json`.

---

## Étape 6 — Stockage offline JSON (détail)

| Tâche | Statut |
|-------|--------|
| `storageInit()` — montage LittleFS + mutex | ✅ |
| `storageEnqueue()` — persistance si MQTT indisponible | ✅ |
| `storageFlush()` — retransmission auto après reconnexion | ✅ |
| Intégration `network_task.cpp` | ✅ |
| Mutex partagé web + storage | ✅ |

**Fichier LittleFS :** `/offline_queue.jsonl` (une ligne JSON par mesure, format identique MQTT)

**Comportement :**
- MQTT déconnecté ou publication échouée → `storageEnqueue(snapshot)`
- Reconnexion MQTT → `storageFlush()` puis publication courante
- Fichier plein (32 Ko) → suppression de l'entrée la plus ancienne
- Ligne corrompue → ignorée à la lecture

**Logs série attendus :** `[storage] enqueue OK`, `[storage] flush OK — N message(s) republie(s)`

---

## Étape 5 — Interface web (détail)

| Tâche | Statut |
|-------|--------|
| Serveur HTTP (`web_task.cpp`) | ✅ |
| Fichiers statiques LittleFS (`data/`) | ✅ |
| API `/api/status` — WiFi, MQTT, uptime, heap | ✅ |
| API `/api/sensors` — temp, humidité live | ✅ |
| API `/api/config` — config MQTT (lecture seule) | ✅ |
| API `/api/actuators` — POST validé (stub) | ✅ |
| Dashboard HTML/CSS/JS | ✅ |

**Accès :** `http://<IP_ESP32>/` (même réseau WiFi que l'ESP32)

**Limitation :** config MQTT modifiable via `secrets.h` + reflash (persistance NVS prévue étape 7).

---

## Structure firmware

```
esp32-firmware/src/
  main.cpp
  config.h              ← DEVICE_ID, broches, constantes publiques
  secrets.h.example     ← modèle WiFi/MQTT (copier en secrets.h)
  sensors/              ← DHT22 + tâche acquisition ✅
  network/              ← WiFi + MQTT + flush offline ✅
  web/                  ← serveur HTTP + API REST ✅
  storage/              ← file d'attente JSON LittleFS ✅
  security/             ← validation JSON, auth API, config NVS ✅
  supervision/          ← heap + uptime ✅
  actuators/            ← stub
esp32-firmware/data/
  index.html, app.js, style.css  ← interface web ✅
```

---

## Câblage DHT22

| DHT22 | ESP32 |
|-------|-------|
| VCC | 3V3 |
| DATA | D4 (GPIO4) |
| GND | GND |

---

## Commandes utiles

```powershell
cd "c:\ESGI\M2\IOT secu\iot-secure-station\esp32-firmware"
$pio = "$env:USERPROFILE\.platformio\penv\Scripts\pio.exe"

& $pio device list
& $pio run
& $pio run -t upload --upload-port COM8
& $pio run -t uploadfs --upload-port COM8   # ← obligatoire pour l'interface web
& $pio device monitor --port COM8
```

> Décommenter `upload_port` / `monitor_port` dans `platformio.ini` ou passer `--upload-port COM8`.

---

## API REST embarquée

| Route | Méthode | Description |
|-------|---------|-------------|
| `/api/status` | GET | WiFi, MQTT, uptime, heap |
| `/api/sensors` | GET | Température + humidité DHT22 |
| `/api/config` | GET | Broker, port, topic MQTT |
| `/api/config` | POST | Enregistrer config MQTT en NVS (auth requise) |
| `/api/actuators` | POST | Commande actionneur (JSON `{ "action": "..." }`, auth requise) |

---

## Format MQTT attendu

```json
{
  "device": "ESP32-1",
  "ts": 0,
  "data": { "temp": 29.3, "humidity": 40.0 }
}
```

Topic : `campus/<MQTT_GROUP>/ESP32-1/data`

---

## Badges

| Badge | Statut |
|-------|--------|
| Embedded Architect | ✅ |
| Sensor Engineer | ✅ |
| Network Engineer | ⬜ (WiFi OK, MQTT à valider bout en bout) |
| Security Engineer | ✅ |
| Reliability Engineer | ✅ |
| Full-Stack IoT | ⬜ (web OK, démo Node-RED en attente) |

---

## Journal

| Date | Action |
|------|--------|
| 2026-06-15 | Étapes 0–2 validées, DHT22 opérationnel |
| 2026-06-15 | Nettoyage repo pour étape 4 (LED retirée, secrets.h.example) |
| 2026-06-15 | Implémentation WiFi + MQTT dans `network_task.cpp` |
| 2026-06-15 | Diagnostic WiFi 5 GHz → résolu (hotspot 2,4 GHz) |
| 2026-06-15 | Étape 5 : serveur web embarqué, API REST, dashboard `data/` |
| 2026-06-15 | Étape 6 : stockage offline JSON LittleFS, flush MQTT automatique |
| 2026-06-15 | Étape 7 : auth API, config MQTT NVS, validation JSON, flux Node-RED |
