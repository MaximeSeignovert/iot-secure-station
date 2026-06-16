# Rapport — IoT Secure Station

## 1. Contexte

Station IoT sécurisée basée sur ESP32, avec collecte capteurs, pilotage actionneurs, résilience réseau et supervision centralisée.

## 2. Architecture

- Firmware modulaire `esp32-firmware/src/` organisé par domaines (`sensors`, `actuators`, `network`, `storage`, `web`, `security`, `supervision`)
- FreeRTOS avec 4 tâches dédiées : acquisition, réseau MQTT, web local, supervision système
- Backend de supervision : Mosquitto + Node-RED + InfluxDB via Docker

## 3. Fonctionnalités implémentées

- Acquisition capteurs : DHT22 + potentiomètre + bouton
- Filtrage et robustesse : moyenne glissante, détection de pics anormaux, fallback dernière mesure valide
- MQTT : publication `campus/<groupe>/<deviceID>/data`, abonnement `.../cmd`, QoS 1
- Mode offline : buffer JSONL LittleFS et rejeu automatique à reconnexion
- Interface web embarquée : live capteurs, état connexion, config MQTT (NVS), commandes actionneurs
- Actionneurs : LED et relais commandables localement (API) et à distance (MQTT)

## 4. Sécurité

- Authentification MQTT côté broker (Mosquitto)
- Validation des JSON entrants (config et commandes)
- Protection des routes POST de l'API locale via `X-Api-Token`
- Stockage des secrets projet hors git (`secrets.h`, `.env`)

## 5. Observabilité

- Logs périodiques : uptime, heap libre, latence de publication MQTT
- Dashboard Node-RED : mesures capteurs et commandes
- Rejets JSON invalides remontés sur topic `.../alerts`

## 6. Limites connues / suite

- Finaliser le câblage matériel réel selon la station finale
- Compléter les preuves de démo (captures/vidéo)
- Bonus Grafana à intégrer (hors périmètre immédiat)
