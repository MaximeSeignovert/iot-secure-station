# Document de travail — Infra & supervision

> Suivi de la partie **infra & supervision** (Docker, MQTT, Node-RED, InfluxDB, docs, bonus Grafana).
> Le firmware ESP32 (capteurs, actionneurs, FreeRTOS, web embarqué, buffer offline) est tenu par le
> coéquipier ; seul le **comportement MQTT** de `esp32-firmware/src/network/` est à caler ensemble.

Dernière mise à jour : 2026-06-15

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
| 1 | Acquisition capteurs | firmware | ❌ |
| 2 | Commande actionneurs | firmware | ❌ |
| 3 | Publication MQTT | infra/network | ❌ (client firmware à écrire) |
| 4 | Stockage offline JSON | firmware | ❌ |
| 5 | Interface web locale | firmware | ❌ |
| 6 | Communication Node-RED | **infra** | ✅ broker + flux + stockage + dashboard |
| 7 | Sécurité | **infra** + firmware | 🟡 auth MQTT ✅, validation JSON ✅, API locale ❌ (firmware) |
| 8 | Optimisation CPU/RAM | firmware | ❌ |

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

### Badges
- ✅ **Security Engineer** (auth MQTT + validation JSON) — en bonne voie
- ✅ **Full-Stack IoT** (côté serveur : Node-RED + dashboard) — partie infra OK
- 🟡 **Reliability Engineer** — persistance broker en place ; reste la démo offline avec le firmware

## 6. Reste à faire

**Infra (moi)**
- [ ] Bonus **Grafana** : ajouter le service au compose, datasource InfluxDB `iot`, dashboard
      (mesures + métrique de robustesse) et une **alerte** (absence de données / valeur aberrante).
- [ ] Enrichir le dashboard Node-RED si besoin (graphe historique `ui_chart`).
- [ ] Compléter `docs/architecture.drawio` et `docs/rapport.md` (captures dans `docs/screenshots/`).

**Coordination avec le firmware (tâche `network/`)**
- [ ] Le client MQTT ESP32 doit : s'authentifier (`esp32` / mot de passe), publier en **QoS ≥ 1** sur
      `campus/<g>/<dev>/data`, s'abonner à `campus/<g>/<dev>/cmd`, et respecter le **schéma JSON**.
- [ ] Valider ensemble le **format des commandes** (`/cmd`) attendu par le firmware
      (actuellement le flux envoie `{ "device", "cmd", "value" }`).
- [ ] Démo **offline** de bout en bout (buffer firmware ↔ rejeu ↔ historique InfluxDB).
