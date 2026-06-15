# Avancement — Développeur embarqué

**Projet :** IoT Secure Station (TP Master II)  
**Rôle :** Firmware ESP32 — PlatformIO, FreeRTOS, DHT22, MQTT, web embarquée  
**Matériel :** ESP32 DevKit (CP2102, COM8 en local), DHT22 sur D4  
**Dernière mise à jour :** 2026-06-15

---

## Étape en cours : 4 — WiFi + MQTT

| Tâche | Statut |
|-------|--------|
| Connexion WiFi (`secrets.h`) | ✅ code prêt — renseigner vos identifiants locaux |
| Publication MQTT JSON | ✅ |
| Topic `campus/<groupe>/ESP32-1/data` | ✅ |
| Coordination Mosquitto Docker (collègue infra) | ⬜ |

**Fichiers modifiés :** `network/network_task.cpp`, `secrets.h` (local, non versionné)

**Limitation connue :** PubSubClient publie en QoS 0 uniquement. Le sujet exige QoS ≥ 1 — migration possible vers `256dpi/arduino-mqtt` si le correcteur le vérifie.

---

## Étapes terminées

| # | Étape | Détail |
|---|-------|--------|
| 0 | Chaîne outil | PlatformIO 6.1.18, pilote CP2102, flash OK |
| 1 | FreeRTOS | 4 tâches : sensor, network, web, supervision — `loop()` vide |
| 2 | DHT22 | GPIO4 (D4), ~29 °C / ~40 % humidité validé en série |

## Reporté

| # | Étape | Note |
|---|-------|------|
| 3 | LED seuil température | Câblage non fonctionnel — reporté |

---

## Roadmap restante

| # | Étape | Statut |
|---|-------|--------|
| 4 | WiFi + MQTT | ✅ code — validation sur matériel en cours |
| 5 | Interface web embarquée (`data/`) | ⬜ |
| 6 | Stockage offline JSON (LittleFS) | ⬜ |
| 7 | Sécurité + intégration Node-RED | ⬜ |
| 8 | Démo bout en bout | ⬜ |

---

## Structure firmware

```
esp32-firmware/src/
  main.cpp
  config.h              ← DEVICE_ID, broches, constantes publiques
  secrets.h.example     ← modèle WiFi/MQTT (copier en secrets.h)
  sensors/              ← DHT22 + tâche acquisition ✅
  network/              ← MQTT à implémenter (étape 4)
  web/                  ← stub
  storage/              ← stub
  security/             ← stub
  supervision/          ← heap + uptime ✅
  actuators/            ← stub
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
& $pio device monitor --port COM8
```

> Décommenter `upload_port` / `monitor_port` dans `platformio.ini` ou passer `--upload-port COM8`.

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
| Network Engineer | ⬜ (code OK, test bout en bout à faire) |
| Reliability Engineer | ⬜ |
| Full-Stack IoT | ⬜ |

---

## Journal

| Date | Action |
|------|--------|
| 2026-06-15 | Étapes 0–2 validées, DHT22 opérationnel |
| 2026-06-15 | Nettoyage repo pour étape 4 (LED retirée, secrets.h.example) |
| 2026-06-15 | Implémentation WiFi + MQTT dans `network_task.cpp` |
