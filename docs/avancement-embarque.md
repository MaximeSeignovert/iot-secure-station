# Avancement — Développeur embarqué

**Projet :** IoT Secure Station (TP Master II)  
**Rôle :** Firmware ESP32 — PlatformIO, FreeRTOS, capteurs, actionneurs, MQTT, web embarquée  
**Dernière mise à jour :** 2026-06-16

---

## État global

| Domaine | État |
|--------|------|
| Architecture modulaire + FreeRTOS | ✅ |
| Acquisition (DHT22 + filtrage + aberrants) | ✅ |
| Capteurs interaction (potentiomètre + bouton) | ✅ |
| Actionneurs (LED + relais) | ✅ |
| MQTT publication QoS 1 | ✅ |
| MQTT réception commandes serveur (`.../cmd`) | ✅ |
| Mode offline LittleFS + rejeu | ✅ |
| Interface web embarquée | ✅ |
| Sécurité minimale (auth API + validation JSON) | ✅ |
| Optimisation (heap + uptime + latence pub) | ✅ |

---

## Points implémentés

- 4 tâches FreeRTOS dédiées : `sensor`, `network`, `web`, `supervision`
- `loop()` bloquante sans logique métier, aucun `delay()`
- Acquisition DHT22 avec :
  - horodatage (epoch ms via NTP, fallback `millis()`)
  - moyenne glissante (`SENSOR_FILTER_WINDOW`)
  - rejet des pics anormaux (seuils temp/hum)
- Capteurs interaction :
  - potentiomètre sur `GPIO34` (normalisé en pourcentage)
  - bouton sur `GPIO27` (pull-up, état pressé/relâché)
- Actionneurs :
  - LED (`GPIO2`) et relais (`GPIO26`)
  - commandes supportées : `led_on`, `led_off`, `relay_on`, `relay_off`
- MQTT :
  - topic data `campus/<groupe>/<deviceID>/data`
  - abonnement commandes `campus/<groupe>/<deviceID>/cmd`
  - publication et abonnement en QoS 1
  - compatibilité commande JSON format `action` et format `cmd/value`
- Offline :
  - persist JSONL dans `/offline_queue.jsonl`
  - flush automatique à la reconnexion MQTT
- Web embarquée :
  - statut WiFi/MQTT + QoS + latence publication
  - capteurs live (temp, humidité, potentiomètre, bouton)
  - commande actionneurs locale
  - config MQTT persistée en NVS

---

## API REST embarquée

| Route | Méthode | Description |
|-------|---------|-------------|
| `/api/status` | GET | État WiFi/MQTT, uptime, heap, latence, état actionneurs |
| `/api/sensors` | GET | Temp, humidité, potentiomètre, bouton, outlier |
| `/api/config` | GET | Broker, port, user, topic MQTT |
| `/api/config` | POST | Sauvegarde config MQTT en NVS (`X-Api-Token`) |
| `/api/actuators` | POST | Commande actionneurs (`{ "action": "..." }`) |

---

## Reste à faire côté embarqué

- Validation matérielle finale sur banc (GPIO réels de la station)
- Démo bout en bout avec infra (commandes Node-RED + test offline réseau)
