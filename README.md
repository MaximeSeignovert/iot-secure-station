# IoT Secure Station

**TP Master II — Système IoT sécurisé et autonome**  
*Jean Luc Carbillet | Durée : 2 à 3 jours*

> Spécification complète : [`Système Iot sécurisé et autonome.pdf`](Système%20Iot%20sécurisé%20et%20autonome.pdf)

## Contexte

Une société déploie des stations IoT dans des bâtiments techniques. Ces stations doivent :

- acquérir des données environnementales ;
- fonctionner même en cas de perte réseau ;
- communiquer avec un serveur central ;
- être configurables localement ;
- résister à des erreurs et usages malveillants.

Notre équipe conçoit le **firmware ESP32** et l'**infrastructure de supervision** complète.

## Équipe (2 personnes)

| Rôle | Responsabilités principales |
|------|----------------------------|
| **Développeur embarqué** | Firmware PlatformIO, FreeRTOS, capteurs/actionneurs, stockage offline, sécurité locale, interface web embarquée |
| **Développeur infra & supervision** | Docker, MQTT, Node-RED, base NoSQL, dashboard, documentation et démo |

### Répartition suggérée

**Développeur embarqué**
- `esp32-firmware/src/sensors/` — acquisition, filtrage, timestamps, détection d'aberrations
- `esp32-firmware/src/actuators/` — LED RGB, relais/servo, commandes
- `esp32-firmware/src/storage/` — persistance JSON en mode offline
- `esp32-firmware/src/security/` — validation JSON, protection API locale
- `esp32-firmware/src/web/` + `data/` — interface locale (live, config MQTT, commandes)
- Tâches FreeRTOS : acquisition, supervision système

**Développeur infra & supervision**
- `esp32-firmware/src/network/` — MQTT (reconnexion, QoS ≥ 1, auth)
- `docker/` — Mosquitto, Node-RED, base NoSQL
- `node-red/` — réception MQTT, stockage, dashboard, envoi de commandes
- `docs/` — architecture, rapport, captures

Les deux développeurs valident ensemble l'intégration bout en bout (MQTT, mode offline, démo).

## Objectifs

Le système embarqué doit :

1. Acquérir plusieurs capteurs
2. Commander des actionneurs
3. Publier les données via MQTT
4. Stocker localement en cas de panne réseau
5. Fournir une interface Web locale
6. Communiquer avec Node-RED
7. Implémenter des mécanismes de sécurité
8. Optimiser les ressources CPU/RAM

## Matériel cible (configuration minimale)

| Type | Composant |
|------|-----------|
| Capteur environnement | BME280 |
| Capteur interaction | Potentiomètre |
| Capteur interaction | Bouton poussoir |
| Actionneur | LED RGB |
| Actionneur | Relais ou servo SG90 |

Autres composants possibles (4 à 6 éléments max au total) : DHT22, DS18B20, BH1750, encodeur, INA219, buzzer, ventilateur PWM, écran OLED SSD1306, etc. (voir le PDF).

## Contraintes techniques (obligatoires)

### Architecture logicielle

Structure modulaire imposée :

```
esp32-firmware/src/
  main.cpp
  sensors/
  actuators/
  network/
  storage/
  web/
  security/
```

**Interdit :** code monolithique, logique métier dans `loop()`, usage de `delay()`.

### Multitâche (FreeRTOS)

Tâches obligatoires :

- acquisition capteurs
- communication réseau
- serveur web
- supervision système

### Acquisition capteurs

- filtrer les mesures
- horodater les données (timestamp)
- détecter les valeurs aberrantes

### Mode offline

Si MQTT est indisponible :

- stockage local en fichier JSON
- retransmission automatique après reconnexion

Format imposé :

```json
{
  "device": "ESP32-X",
  "ts": 0,
  "data": {
    "temp": 0,
    "humidity": 0
  }
}
```

### Interface web embarquée

Fichiers séparés dans `esp32-firmware/data/` :

- `index.html`, `app.js`, `style.css`

Fonctionnalités :

- affichage live des capteurs
- état de la connexion
- configuration MQTT
- commande des actionneurs

### MQTT

Topic : `campus/<groupe>/<deviceID>/data`

Contraintes :

- reconnexion automatique
- QoS ≥ 1
- réception des commandes serveur
- authentification MQTT

### Node-RED

Le serveur doit :

- recevoir les messages MQTT
- stocker en base NoSQL
- afficher un dashboard
- envoyer des commandes

### Sécurité minimale

- authentification MQTT
- validation des JSON entrants
- protection de l'API locale

### Optimisation

Affichage périodique de :

- heap libre
- uptime
- latence de publication

## Structure du projet

```
iot-secure-station/
├── esp32-firmware/   # Firmware PlatformIO (ESP32)
│   ├── src/          # Code modulaire (sensors, actuators, network…)
│   └── data/         # Interface web embarquée (LittleFS)
├── node-red/         # Flux de supervision
├── docs/             # Architecture, rapport, captures
├── docker/           # Stack Docker (MQTT, Node-RED, NoSQL)
└── README.md
```

## Livrables

1. Code source structuré
2. Diagramme d'architecture (`docs/architecture.drawio`)
3. Dashboard Node-RED
4. Rapport technique (max 5 pages — `docs/rapport.md`)
5. Démonstration finale

## Badges à débloquer

| Badge | Condition |
|-------|-----------|
| Sensor Engineer | acquisition fiable + filtrage |
| Network Engineer | MQTT robuste |
| Embedded Architect | multitâche propre |
| Security Engineer | validation + auth |
| Full-Stack IoT | Web + Node-RED |
| Reliability Engineer | survit aux pannes |
| Performance Engineer | optimisation mémoire |

## Prérequis

- [PlatformIO](https://platformio.org/) — firmware ESP32
- [Docker](https://www.docker.com/) + Docker Compose — infrastructure
- Matériel : ESP32, BME280, potentiomètre, bouton, LED RGB, relais ou servo

## Démarrage rapide

### 1. Firmware ESP32

```bash
cd esp32-firmware
pio run -t upload
pio run -t uploadfs
```

### 2. Infrastructure Docker

```bash
cd docker
docker compose up -d
```

### 3. Node-RED

1. Ouvrir http://localhost:1880
2. Importer `node-red/flows.json`
3. Déployer

## Documentation

- Spécification : [`Système Iot sécurisé et autonome.pdf`](Système%20Iot%20sécurisé%20et%20autonome.pdf)
- Architecture : [`docs/architecture.drawio`](docs/architecture.drawio)
- Rapport : [`docs/rapport.md`](docs/rapport.md)
- Captures : [`docs/screenshots/`](docs/screenshots/)
