# Node-RED — IoT Secure Station

## Import des flux

1. Démarrer Node-RED (`docker compose up` ou instance locale)
2. Ouvrir http://localhost:1880
3. Menu → Import → sélectionner `flows.json`
4. Déployer

## Topics MQTT (à configurer)

| Topic | Direction | Description |
|-------|-----------|-------------|
| `campus/<groupe>/<device>/data` | ESP32 → Node-RED | Données capteurs (JSON) |
| `campus/<groupe>/<device>/cmd` | Node-RED → ESP32 | Commandes actionneurs (futur) |
