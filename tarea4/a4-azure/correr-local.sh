#!/bin/bash
#
# Actividad 4 de punta a punta sin Azure: la "nube" es un broker Mosquitto.
#
#   contenedor contiker/contiki-ng (--privileged)
#     sensor-azure.native (httpd-ws, tun0 fd00::1/64, mote fd00::302:304:506:708)
#     tcp-forward.py :8080 -> [mote]:80          publicado en 127.0.0.1:8080
#     mosquitto :1883 (a2-mqtt/mosquitto.conf)   publicado en 127.0.0.1:1883
#   Mac (Node 22)
#     subscriber/cloud-subscriber.js  <- usm/tel329/cloud/+/telemetry
#     middleware/middleware-azure.js --destino local  -> usm/tel329/cloud/contiki-ng-01/telemetry
#
# Uso, desde cualquier lado: tarea4/a4-azure/correr-local.sh [segundos]   (90 por defecto)
# Deja en tarea4/evidencia/: a4-sensor-local.log, a4-mosquitto.log,
# a4-middleware-local.log, a4-subscriber-local.log.
#
set -u

AQUI=$(cd "$(dirname "$0")" && pwd)
RAIZ=$(cd "$AQUI/../.." && pwd)
EVIDENCIA="$RAIZ/tarea4/evidencia"
DURACION_S=${1:-90}
NOMBRE=${NOMBRE:-a4local}
CONTIKI_HOST=${CONTIKI_HOST:-$HOME/contiki-ng}

log() { echo "[$(date +%H:%M:%S)] $*"; }
limpiar() {
  [ -n "${MW_PID:-}" ] && kill -TERM "$MW_PID" 2>/dev/null
  [ -n "${SUB_PID:-}" ] && kill -TERM "$SUB_PID" 2>/dev/null
  wait 2>/dev/null
  docker rm -f "$NOMBRE" >/dev/null 2>&1 && log "contenedor $NOMBRE borrado"
}
trap limpiar EXIT

command -v node >/dev/null || { echo "falta node (>= 22)" >&2; exit 1; }
for d in middleware subscriber; do
  [ -d "$AQUI/$d/node_modules/mqtt" ] || (cd "$AQUI/$d" && npm install --no-audit --no-fund >/dev/null)
done

log "contenedor $NOMBRE"
docker rm -f "$NOMBRE" >/dev/null 2>&1
docker run -d --name "$NOMBRE" --privileged \
  -p 127.0.0.1:8080:8080 -p 127.0.0.1:1883:1883 -e CCACHE_DISABLE=1 \
  -v "$CONTIKI_HOST":/home/user/contiki-ng -v "$RAIZ":/work -w /work/tarea4/a4-azure/sensor \
  contiker/contiki-ng sleep infinity >/dev/null || exit 1

log "compilando el sensor (TARGET=native)"
docker exec "$NOMBRE" make CONTIKI=/home/user/contiki-ng TARGET=native >/dev/null || { echo "el sensor no compila" >&2; exit 1; }

docker exec -d "$NOMBRE" sh -c './build/native/sensor-azure.native > /work/tarea4/evidencia/a4-sensor-local.log 2>&1'
docker exec -d "$NOMBRE" python3 /work/tarea4/a4-azure/router/tcp-forward.py 8080 fd00::302:304:506:708 80
docker exec -d "$NOMBRE" sh -c 'mosquitto -c /work/tarea4/a2-mqtt/mosquitto.conf -v > /work/tarea4/evidencia/a4-mosquitto.log 2>&1'

for i in $(seq 1 30); do
  curl -sf -o /dev/null http://127.0.0.1:8080/ && nc -z 127.0.0.1 1883 >/dev/null 2>&1 && break
  sleep 1
done
curl -sf http://127.0.0.1:8080/ >/dev/null || { echo "el sensor no responde por el puente" >&2; exit 1; }
nc -z 127.0.0.1 1883 >/dev/null 2>&1 || { echo "mosquitto no escucha en 127.0.0.1:1883" >&2; exit 1; }
log "sensor y broker arriba"

node "$AQUI/subscriber/cloud-subscriber.js" > "$EVIDENCIA/a4-subscriber-local.log" 2>&1 &
SUB_PID=$!
sleep 1
SENSOR_URL=http://127.0.0.1:8080/ node "$AQUI/middleware/middleware-azure.js" --destino local \
  > "$EVIDENCIA/a4-middleware-local.log" 2>&1 &
MW_PID=$!
log "middleware (pid $MW_PID) y suscriptor (pid $SUB_PID) corriendo $DURACION_S s"
sleep "$DURACION_S"

kill -TERM "$MW_PID"; wait "$MW_PID" 2>/dev/null; unset MW_PID
sleep 1
kill -TERM "$SUB_PID"; wait "$SUB_PID" 2>/dev/null; unset SUB_PID

log "PUBLISH con PUBACK: $(grep -c 'PUBACK en' "$EVIDENCIA/a4-middleware-local.log") · recibidos: $(grep -c '^Message received' "$EVIDENCIA/a4-subscriber-local.log") · GET en el mote: $(grep -c 'GET /' "$EVIDENCIA/a4-sensor-local.log")"
tail -1 "$EVIDENCIA/a4-subscriber-local.log"
