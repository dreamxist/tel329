#!/bin/bash
#
# Actividad 2 + mitad MQTT de la Actividad 3, de una sola corrida y sin GUI:
#
#   mosquitto (broker, fd00::1:1883)
#     ^ tun0
#   tunslip6 -a 127.0.0.1 -p 60001 fd00::1/64
#     ^ socket serial del mote 1 (SerialSocketServer, Cooja headless)
#   Cooja: mote 1 = rpl-border-router, motes 2..6 = mqtt-cliente.c
#
# Pensado para correr DENTRO del contenedor, que necesita --privileged por el
# /dev/net/tun. Desde el host, en la raiz del worktree:
#
#   docker run --rm --privileged -e CCACHE_DISABLE=1 \
#     -v ~/contiki-ng:/home/user/contiki-ng \
#     -v tel329-gradle:/home/user/.gradle \
#     -v "$PWD":/work -w /work/tarea4/a2-mqtt \
#     contiker/contiki-ng ./correr.sh [segundos_simulados]
#
# Deja en ../evidencia/: a2-mqtt.log (Mote output con tiempo simulado),
# a2-mosquitto.log, a2-tunslip6.log (incluye la consola del border router),
# a2-mqtt.pcap + a2-mqtt-pcap.txt (tcpdump en tun0), a2-tiempos-mqtt.csv y
# a2-resumen-mqtt.md (analizar.py).
#
set -u

AQUI=$(cd "$(dirname "$0")" && pwd)
EVIDENCIA="$AQUI/../evidencia"
DURACION_S=${1:-300}          # tiempo simulado; con speedlimit 1.0 es ~real
SEMILLA=${SEMILLA:-123456}
PUERTO_SERIAL=60001
PREFIJO=fd00::1/64

CONTIKI=${CONTIKI:-$(ls -d "$AQUI/../../contiki-ng" "$HOME/contiki-ng" 2>/dev/null | head -1)}
if [ ! -f "$CONTIKI/Makefile.include" ]; then
  echo "No encuentro Contiki-NG. Exportar CONTIKI=/ruta/a/contiki-ng" >&2
  exit 1
fi

log() { echo "[$(date +%H:%M:%S)] $*"; }

# --- 0. herramientas -------------------------------------------------------
if ! command -v mosquitto >/dev/null; then
  echo "falta mosquitto en la imagen" >&2; exit 1
fi
if ! command -v tcpdump >/dev/null; then
  log "instalando tcpdump (no viene en contiker/contiki-ng)"
  sudo apt-get update -qq >/dev/null 2>&1 && sudo apt-get install -y -qq tcpdump >/dev/null 2>&1 \
    || log "sin tcpdump: la corrida sigue, no habra pcap"
fi
TUNSLIP6="$CONTIKI/tools/serial-io/tunslip6"
if [ ! -x "$TUNSLIP6" ]; then
  log "compilando tunslip6"
  make -C "$CONTIKI/tools/serial-io" tunslip6 >/dev/null || { echo "tunslip6 no compila" >&2; exit 1; }
fi
# ~/.gradle puede venir de un volumen creado como root.
[ -d "$HOME/.gradle" ] && sudo chown -R "$(id -u):$(id -g)" "$HOME/.gradle" 2>/dev/null

TRABAJO=$(mktemp -d)
mkdir -p "$EVIDENCIA"
PIDS=()
limpiar() {
  log "cerrando"
  [ -n "${TCPDUMP_PID:-}" ] && sudo kill -INT "$TCPDUMP_PID" 2>/dev/null && sleep 1
  [ -n "${TUNSLIP_PID:-}" ] && sudo kill "$TUNSLIP_PID" 2>/dev/null
  [ -n "${COOJA_PID:-}" ] && kill "$COOJA_PID" 2>/dev/null
  [ -n "${MOSQ_PID:-}" ] && kill "$MOSQ_PID" 2>/dev/null
  sudo chown -R "$(id -u):$(id -g)" "$TRABAJO" 2>/dev/null
}
trap limpiar EXIT

# --- 1. broker ---------------------------------------------------------------
mosquitto -c "$AQUI/mosquitto.conf" -v > "$TRABAJO/mosquitto.log" 2>&1 &
MOSQ_PID=$!
for i in $(seq 1 15); do
  netstat -ltn 2>/dev/null | grep -q ":1883 " && break
  sleep 1
done
if ! kill -0 "$MOSQ_PID" 2>/dev/null || ! netstat -ltn 2>/dev/null | grep -q ":1883 "; then
  cat "$TRABAJO/mosquitto.log" >&2; echo "mosquitto no arranco" >&2; exit 1
fi
log "mosquitto escuchando en :1883 (pid $MOSQ_PID)"

# --- 2. .csc headless ----------------------------------------------------------
# Igual que la Tarea 3: el .csc generado tiene que vivir junto a los fuentes
# porque [CONFIG_DIR] es el directorio del propio .csc; CONTIKI va explicito
# en el comando de compilacion porque Cooja compila sin $HOME; y el TIMEOUT
# del script va como entero literal en milisegundos.
JS="$AQUI/.headless.js"
CSC="$AQUI/.headless.csc"
sed "s/^TIMEOUT(.*/TIMEOUT($(( DURACION_S * 1000 )), log.testOK());/" "$AQUI/js/volcar-log.js" > "$JS"
PLUGIN="  <plugin>\n\
    org.contikios.cooja.plugins.ScriptRunner\n\
    <plugin_config>\n\
      <scriptfile>$JS<\/scriptfile>\n\
      <active>true<\/active>\n\
    <\/plugin_config>\n\
  <\/plugin>"
sed -e "s|TARGET=cooja|TARGET=cooja CONTIKI=$CONTIKI|" \
    -e "s/<randomseed>[0-9]*</<randomseed>$SEMILLA</" \
    -e "s|</simconf>|$PLUGIN\n</simconf>|" \
    "$AQUI/a2-mqtt.csc" > "$CSC"

# --- 3. Cooja --------------------------------------------------------------------
log "lanzando Cooja headless ($DURACION_S s simulados, semilla $SEMILLA)"
(cd "$CONTIKI/tools/cooja" && \
  ./gradlew --no-daemon --quiet run \
    --args="--no-gui --contiki=$CONTIKI --random-seed=$SEMILLA --logdir=$TRABAJO $CSC") \
  > "$TRABAJO/cooja-stdout.txt" 2>&1 &
COOJA_PID=$!

# El SerialSocketServer abre el puerto al cargar la simulacion, o sea despues
# de compilar los dos tipos de mote: puede tardar varios minutos la primera vez.
log "esperando el socket serial del mote 1 en :$PUERTO_SERIAL"
for i in $(seq 1 900); do
  # netstat y no una conexion de prueba: el SerialSocketServer acepta UN
  # cliente y una sonda que conecta y corta puede dejar afuera a tunslip6.
  if netstat -ltn 2>/dev/null | grep -q ":$PUERTO_SERIAL "; then break; fi
  if ! kill -0 "$COOJA_PID" 2>/dev/null; then
    echo "Cooja termino antes de abrir el socket; cola de su salida:" >&2
    tail -40 "$TRABAJO/cooja-stdout.txt" >&2
    exit 1
  fi
  sleep 1
done
log "socket serial arriba"

# --- 4. tunslip6 + tcpdump ------------------------------------------------------
sudo "$TUNSLIP6" -a 127.0.0.1 -p $PUERTO_SERIAL $PREFIJO > "$TRABAJO/tunslip6.log" 2>&1 &
TUNSLIP_PID=$!
for i in $(seq 1 60); do
  ifconfig tun0 2>/dev/null | grep -q "fd00::1" && break
  sleep 1
done
if ! ifconfig tun0 2>/dev/null | grep -q "fd00::1"; then
  echo "tun0 no aparecio; tunslip6 dijo:" >&2; cat "$TRABAJO/tunslip6.log" >&2
  exit 1
fi
log "tun0 con fd00::1 (tunslip6 pid $TUNSLIP_PID)"

if command -v tcpdump >/dev/null; then
  sudo tcpdump -i tun0 -Z root -w "$TRABAJO/a2-mqtt.pcap" port 1883 > "$TRABAJO/tcpdump.log" 2>&1 &
  TCPDUMP_PID=$!
  log "tcpdump en tun0 (pid $TCPDUMP_PID)"
fi

# --- 5. correr y recoger -------------------------------------------------------
wait "$COOJA_PID"
SALIDA=$?
unset COOJA_PID
log "Cooja termino (gradle salio con $SALIDA)"
limpiar
trap - EXIT

if [ ! -f "$TRABAJO/COOJA.testlog" ]; then
  echo "sin COOJA.testlog; cola de la salida de Cooja:" >&2
  tail -40 "$TRABAJO/cooja-stdout.txt" >&2
  exit 1
fi

cp "$TRABAJO/COOJA.testlog" "$EVIDENCIA/a2-mqtt.log"
cp "$TRABAJO/mosquitto.log" "$EVIDENCIA/a2-mosquitto.log"
cp "$TRABAJO/tunslip6.log" "$EVIDENCIA/a2-tunslip6.log"
if [ -s "$TRABAJO/a2-mqtt.pcap" ]; then
  cp "$TRABAJO/a2-mqtt.pcap" "$EVIDENCIA/a2-mqtt.pcap"
  tcpdump -r "$EVIDENCIA/a2-mqtt.pcap" -tt -nn -v > "$EVIDENCIA/a2-mqtt-pcap.txt" 2>/dev/null
fi
rm -rf "$TRABAJO" "$JS" "$CSC"

log "lineas en a2-mqtt.log: $(wc -l < "$EVIDENCIA/a2-mqtt.log"); PUB: $(grep -c 'A2,PUB,' "$EVIDENCIA/a2-mqtt.log"); PUBACK: $(grep -c 'A2,PUBACK,' "$EVIDENCIA/a2-mqtt.log")"
python3 "$AQUI/analizar.py" "$EVIDENCIA/a2-mqtt.log" "$EVIDENCIA/a2-tiempos-mqtt.csv" "$EVIDENCIA/a2-resumen-mqtt.md" \
  ${PCAP_TXT:-"$EVIDENCIA/a2-mqtt-pcap.txt"}
log "listo"
