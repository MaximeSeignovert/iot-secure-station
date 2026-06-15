#!/usr/bin/env bash
# Simulateur ESP32 — publie des mesures au format imposé sur MQTT.
# Permet de tester toute la chaîne de supervision SANS le firmware.
#
# Usage :
#   ./simulate-esp32.sh                 # publie en boucle toutes les 2s
#   ./simulate-esp32.sh once            # publie un seul message
#   ./simulate-esp32.sh invalid         # publie un JSON invalide (test validation)
#
# Prérequis : la stack tourne (docker compose up -d). On publie via le
# conteneur mosquitto, donc pas besoin de mosquitto_pub sur l'hôte.

set -euo pipefail

DEVICE="${DEVICE:-esp32-1}"
GROUP="${GROUP:-g1}"
USER_MQTT="${USER_MQTT:-esp32}"
PASS_MQTT="${PASS_MQTT:-esp32-secret}"
TOPIC="campus/${GROUP}/${DEVICE}/data"
CONTAINER="${CONTAINER:-iot-mosquitto}"

pub() {
  docker exec "$CONTAINER" mosquitto_pub \
    -h localhost -u "$USER_MQTT" -P "$PASS_MQTT" -q 1 -t "$TOPIC" -m "$1"
}

make_msg() {
  # ts en secondes epoch (le flux Node-RED gère s ou ms)
  local ts temp hum
  ts=$(date +%s)
  temp=$(awk -v min=18 -v max=28 'BEGIN{srand(); printf "%.1f", min+rand()*(max-min)}')
  hum=$(awk -v min=40 -v max=70 'BEGIN{srand(); printf "%.1f", min+rand()*(max-min)}')
  printf '{"device":"%s","ts":%s,"data":{"temp":%s,"humidity":%s}}' \
    "$DEVICE" "$ts" "$temp" "$hum"
}

case "${1:-loop}" in
  once)
    msg="$(make_msg)"; echo "→ $TOPIC : $msg"; pub "$msg" ;;
  invalid)
    msg='{"device":"esp32-1","oops":true}'
    echo "→ (invalide) $TOPIC : $msg"; pub "$msg" ;;
  loop)
    echo "Publication en boucle sur $TOPIC (Ctrl-C pour arrêter)…"
    while true; do
      msg="$(make_msg)"; echo "→ $msg"; pub "$msg"; sleep 2
    done ;;
  *)
    echo "Usage: $0 [once|invalid|loop]" >&2; exit 1 ;;
esac
