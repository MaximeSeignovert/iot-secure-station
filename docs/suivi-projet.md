# Document de travail — Infra & supervision

> Suivi de la partie **infra & supervision** (Docker, MQTT, Node-RED, InfluxDB, docs, bonus Grafana).
> Le firmware ESP32 est désormais intégré à la convention MQTT côté infra
> (`campus/<groupe>/<deviceID>/data` et `.../cmd`, QoS 1).

Dernière mise à jour : 2026-06-16

---

## 1. Architecture

```
┌──────────┐   campus/<g>/<dev>/data    ┌────────────┐                ┌────────────┐
│  ESP32   │ ─────────(QoS 1)─────────► │ Mosquitto  │ ──ingest────►  │  Node-RED  │
│ firmware │ ◄────────campus/.../cmd─── │  (broker)  │ ◄──commandes── │            │
└──────────┘                            │ 1883 / 9001│                │ valide JSON│
     ▲  buffer offline (LittleFS)       └────────────┘                │ + stocke   │
     │  rejeu au retour réseau                                        └─────┬──────┘
                                                                            │ write (ts d'origine)
                                                              ┌─────────────▼─────────────┐
                                                              │   InfluxDB 1.x  (base iot) │
                                                              └─────────────┬─────────────┘
                                                       Dashboard Node-RED   │   [Grafana — plus tard]
                                                       http://…:1880/ui ◄───┘
```

- **Mosquitto** : transport temps réel, authentifié (ne stocke pas l'historique).
- **Node-RED** : ingest `campus/+/+/data` → validation du schéma → écriture InfluxDB → dashboard + envoi de commandes.
- **InfluxDB** : historique (base NoSQL temporelle, ouvre la voie à Grafana sans refonte).

## 2. Conventions

### Topics MQTT
| Topic | Direction | Rôle |
|-------|-----------|------|
| `campus/<groupe>/<deviceID>/data`   | ESP32 → serveur | mesures |
| `campus/<groupe>/<deviceID>/cmd`    | serveur → ESP32 | commandes actionneurs |
| `campus/<groupe>/<deviceID>/alerts` | serveur ↔       | alertes / JSON rejeté |

### Schéma JSON imposé (topic `data`)
```json
{ "device": "esp32-1", "ts": 1718440000, "data": { "temp": 21.5, "humidity": 60 } }
```
`ts` en secondes **ou** millisecondes (le flux normalise). Tout message non conforme est rejeté et
republié sur `.../alerts`.

### Stockage InfluxDB
- base : `iot` · measurement : `sensors`
- tag : `device` · fields : champs numériques de `data` (`temp`, `humidity`, …)
- **timestamp = `ts` du message** (pas l'heure d'arrivée) → historique correct même après un rejeu offline.

## 3. Identifiants, ports, URLs

| Service | URL / Port | User | Mot de passe |
|---------|-----------|------|--------------|
| Mosquitto MQTT | `localhost:1883` | `esp32`, `nodered` | voir ci-dessous |
| Mosquitto WS   | `localhost:9001` | idem | — |
| Node-RED       | http://localhost:1880 | — | — |
| Dashboard      | http://localhost:1880/ui | — | — |
| InfluxDB       | http://localhost:8086 (base `iot`) | `admin`, `nodered` | voir `docker/.env` |

**Mots de passe** : dans `docker/.env` (non versionné). Côté MQTT, le fichier `docker/mosquitto/config/passwd`
contient les hash (versionné). Côté Node-RED, `node-red/flows_cred.json` contient les identifiants en
clair (`credentialSecret: false` dans `settings.js`) — acceptable pour le TP, à externaliser en prod.

## 4. Lancer & tester

```bash
# Démarrer toute la stack
cd docker && docker compose up -d

# (1ère fois uniquement) installer les nodes Node-RED dans /data puis redémarrer
docker exec iot-node-red sh -c "cd /data && npm install node-red-dashboard node-red-contrib-influxdb"
docker compose restart node-red

# Simuler l'ESP32 (sans firmware)
./docker/test/simulate-esp32.sh loop      # flux continu
./docker/test/simulate-esp32.sh once      # un message valide
./docker/test/simulate-esp32.sh invalid   # JSON invalide (doit être rejeté)

# Vérifier le stockage
curl -s -G "http://localhost:8086/query?db=iot" \
  --data-urlencode "q=SELECT * FROM sensors ORDER BY time DESC LIMIT 5" -u nodered:nodered-secret
```

### Test du mode offline (démo robustesse)
1. Lancer `simulate-esp32.sh loop` → mesures qui arrivent dans InfluxDB.
2. `docker compose stop mosquitto` → le firmware doit bufferiser localement (côté ESP32).
3. `docker compose start mosquitto` → vérifier que les mesures de la coupure réapparaissent **avec
   leur `ts` d'origine**, sans trou ni doublon.

## 5. État d'avancement

### Objectifs du projet
| # | Objectif | Côté | État |
|---|----------|------|------|
| 1 | Acquisition capteurs | firmware | ✅ (DHT22 + filtrage + aberrants) |
| 2 | Commande actionneurs | firmware | ✅ (LED + relais) |
| 3 | Publication MQTT | firmware + infra | ✅ (QoS 1) |
| 4 | Stockage offline JSON | firmware | ✅ |
| 5 | Interface web locale | firmware | ✅ |
| 6 | Communication Node-RED | **infra** | ✅ broker + flux + stockage + dashboard |
| 7 | Sécurité | **infra** + firmware | ✅ baseline |
| 8 | Optimisation CPU/RAM | firmware | ✅ (heap/uptime/latence publication) |

### Détail infra
| Élément | État |
|---------|------|
| Mosquitto + auth (1883/9001, `allow_anonymous false`) | ✅ |
| InfluxDB 1.x (base `iot`, auth) | ✅ |
| Flux Node-RED : ingest → validation → stockage → dashboard → commandes | ✅ |
| Dashboard Node-RED (jauges temp/humidité, état, boutons LED) | ✅ |
| Validation JSON + republication alertes | ✅ |
| Simulateur ESP32 (`docker/test/simulate-esp32.sh`) | ✅ |
| Convention de topics documentée | ✅ |
| **Bonus Grafana** | ❌ à faire |

### Badges (état technique)
- ✅ **Security Engineer** (auth MQTT + validation JSON + API locale protégée)
- ✅ **Full-Stack IoT** (web embarquée + Node-RED + commandes)
- ✅ **Reliability Engineer** (buffer offline + rejeu MQTT)
- ✅ **Network Engineer** (reco + QoS 1 + topic data/cmd)

## 6. Reste à faire

**Infra**
- [ ] Bonus **Grafana** : ajouter le service au compose, datasource InfluxDB `iot`, dashboard
      (mesures + métrique de robustesse) et une **alerte** (absence de données / valeur aberrante).
- [ ] Enrichir le dashboard Node-RED si besoin (graphe historique `ui_chart`).
- [ ] Compléter `docs/architecture.drawio` et `docs/rapport.md`.

**Intégration finale**
- [ ] Vérifier sur matériel réel la correspondance exacte des broches (capteurs/actionneurs).
- [ ] Capturer une démo complète bout en bout (online/offline/commandes).
