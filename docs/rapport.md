# Rapport — IoT Secure Station

## 1. Contexte

Projet de station IoT sécurisée pour la collecte de données capteurs et la supervision distante.

## 2. Architecture

Voir `architecture.drawio` pour le schéma détaillé.

## 3. Composants

- **ESP32** : acquisition capteurs, actuateurs, interface web embarquée
- **MQTT** : bus de messages entre firmware et supervision
- **Node-RED** : orchestration, tableaux de bord, alertes
- **Docker** : déploiement reproductible de l'infrastructure

## 4. Sécurité

- Authentification MQTT
- Chiffrement TLS (à configurer en production)
- Validation des entrées côté firmware

## 5. Captures d'écran

Placer les captures dans `docs/screenshots/`.
