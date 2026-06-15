# Node-RED — IoT Secure Station

Flux de supervision : ingest MQTT → validation JSON → stockage InfluxDB → dashboard → commandes.

## Démarrage

Le flux est chargé automatiquement par le conteneur `iot-node-red` (voir `docker/docker-compose.yml`,
qui monte `flows.json` et `flows_cred.json`). Il suffit de :

```bash
cd docker && docker compose up -d
```

- Éditeur Node-RED : http://localhost:1880
- Dashboard : http://localhost:1880/ui

> Les nodes `node-red-dashboard` et `node-red-contrib-influxdb` doivent être installés dans le
> conteneur (voir `docs/suivi-projet.md` → section installation).

## Convention de topics MQTT

Schéma conforme à la spec (`campus/<groupe>/<deviceID>/...`) :

| Topic | Direction | Description |
|-------|-----------|-------------|
| `campus/<groupe>/<deviceID>/data`   | ESP32 → Node-RED | Mesures capteurs (format JSON imposé) |
| `campus/<groupe>/<deviceID>/cmd`    | Node-RED → ESP32 | Commandes actionneurs |
| `campus/<groupe>/<deviceID>/alerts` | Node-RED ↔        | Alertes (ex. JSON invalide rejeté) |

Node-RED s'abonne à `campus/+/+/data` (tous groupes/devices) et publie les commandes sur
`campus/<groupe>/<deviceID>/cmd`.

## Format JSON imposé (topic `data`)

```json
{ "device": "esp32-1", "ts": 1718440000, "data": { "temp": 21.5, "humidity": 60 } }
```

- `ts` accepté en secondes ou millisecondes (le flux normalise) — utilisé comme **timestamp InfluxDB**
  pour préserver l'historique lors du rejeu offline.
- Tout message ne respectant pas ce schéma est **rejeté** et republié sur `.../alerts`.

## Identifiants

- MQTT (broker `mosquitto`) : user `nodered`
- InfluxDB (base `iot`) : user `nodered`

Mots de passe : voir `docker/.env` (non versionné). Le flux les lit via `flows_cred.json`
(non chiffré car `credentialSecret: false` dans `settings.js`).
