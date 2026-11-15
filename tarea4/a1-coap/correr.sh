#!/bin/bash
#
# Compila la raiz y el cliente, corre la simulacion en Cooja headless y deja en
# ../evidencia/: a1-coap.log (Mote output con tiempo simulado), a1-coap.pcap
# (tramas 802.15.4 del radio) y, si hay python3, a1-tiempos-coap.csv con una
# fila por peticion CoAP mas a1-resumen-coap.md con las estadisticas de RTT.
#
# Pensado para correr DENTRO del contenedor (usa sed de GNU). Desde el host:
#
#   docker run --rm -e CCACHE_DISABLE=1 \
#     -v ~/contiki-ng:/home/user/contiki-ng \
#     -v ~/.gradle-contiker:/home/user/.gradle \
#     -v "$PWD":/work \
#     contiker/contiki-ng /work/tarea4/a1-coap/correr.sh
#
# El volumen de ~/.gradle es opcional: evita que cada corrida vuelva a bajar
# la distribucion de gradle que pide el wrapper de Cooja.
#
# Variables opcionales: SEGUNDOS (tiempo simulado maximo, default 600),
# SEMILLA (default 123456), SIN_COMPILAR=1 para saltar el make.
#
set -u

AQUI=$(cd "$(dirname "$0")" && pwd)
EVIDENCIA="$AQUI/../evidencia"
SEGUNDOS=${SEGUNDOS:-600}
SEMILLA=${SEMILLA:-123456}

# Misma resolucion de ruta que el Makefile, por el mismo motivo.
CONTIKI=${CONTIKI:-$(ls -d "$AQUI/../../contiki-ng" "$HOME/contiki-ng" 2>/dev/null | head -1)}
if [ ! -f "$CONTIKI/Makefile.include" ]; then
  echo "No encuentro Contiki-NG. Exportar CONTIKI=/ruta/a/contiki-ng" >&2
  exit 1
fi

mkdir -p "$EVIDENCIA"

if [ -z "${SIN_COMPILAR:-}" ]; then
  echo "=== compilando root-coap y cliente-coap (TARGET=cooja) ==="
  make -C "$AQUI" distclean TARGET=cooja CONTIKI="$CONTIKI" >/dev/null 2>&1
  if ! make -C "$AQUI" TARGET=cooja CONTIKI="$CONTIKI" -j4 >"$EVIDENCIA/.make.log" 2>&1; then
    echo "  compilacion FALLO, ver $EVIDENCIA/.make.log" >&2
    tail -20 "$EVIDENCIA/.make.log" >&2
    exit 1
  fi
fi

# El .csc headless tiene que vivir junto a los fuentes: [CONFIG_DIR] es el
# directorio del propio .csc, y de ahi salen los .c de cada tipo de mote.
js="$AQUI/.headless.js"
csc="$AQUI/.headless.csc"
pcap="$EVIDENCIA/a1-coap.pcap"

# El limite va en milisegundos y como entero literal: Cooja lo saca del texto
# del script con una regex que solo acepta digitos (ver js/volcar-log.js).
sed -e "s/^TIMEOUT(.*/TIMEOUT($(( SEGUNDOS * 1000 )), finalizar());/" \
    -e "s|__PCAP__|$pcap|" \
    "$AQUI/js/volcar-log.js" > "$js"

plugin="  <plugin>\n\
    org.contikios.cooja.plugins.ScriptRunner\n\
    <plugin_config>\n\
      <scriptfile>$js<\/scriptfile>\n\
      <active>true<\/active>\n\
    <\/plugin_config>\n\
  <\/plugin>"
# CONTIKI va explicito en el comando de compilacion del .csc generado: Cooja
# compila el mote con un entorno sin $HOME, y dentro del contenedor el symlink
# del repo apunta a una ruta del host que no existe.
sed -e "s|TARGET=cooja|TARGET=cooja CONTIKI=$CONTIKI|" \
    -e "s|<randomseed>[0-9]*</randomseed>|<randomseed>$SEMILLA</randomseed>|" \
    -e "s|</simconf>|$plugin\n</simconf>|" \
    "$AQUI/a1-coap.csc" > "$csc"

echo "=== Cooja headless: hasta $SEGUNDOS s simulados, semilla $SEMILLA ==="
rm -f "$pcap"
logdir=$(mktemp -d)
(cd "$CONTIKI/tools/cooja" && \
  ./gradlew --no-daemon --quiet run \
    --args="--no-gui --random-seed=$SEMILLA --logdir=$logdir $csc") \
  > "$logdir/stdout.txt" 2>&1
salida=$?

fallo=0
if [ -f "$logdir/COOJA.testlog" ]; then
  cp "$logdir/COOJA.testlog" "$EVIDENCIA/a1-coap.log"
  echo "  -> $EVIDENCIA/a1-coap.log ($(wc -l < "$EVIDENCIA/a1-coap.log") lineas)"
  grep -c "] TX id=" "$EVIDENCIA/a1-coap.log" | sed 's/^/  peticiones TX: /'
  grep -c "] RX id=" "$EVIDENCIA/a1-coap.log" | sed 's/^/  respuestas RX: /'
  grep "^--- fin" "$EVIDENCIA/a1-coap.log" || true
else
  echo "  sin COOJA.testlog; stdout de Cooja:" >&2
  tail -30 "$logdir/stdout.txt" >&2
  fallo=1
fi
if [ -s "$pcap" ]; then
  echo "  -> $pcap ($(stat -c %s "$pcap" 2>/dev/null || wc -c < "$pcap") bytes)"
else
  echo "  sin pcap" >&2
fi

if [ $fallo -eq 0 ] && command -v python3 >/dev/null; then
  python3 "$AQUI/tiempos.py" "$EVIDENCIA/a1-coap.log" \
    --csv "$EVIDENCIA/a1-tiempos-coap.csv" --resumen "$EVIDENCIA/a1-resumen-coap.md" \
    ${PCAP_PARA_RESUMEN:+--pcap "$pcap"}
fi

rm -rf "$logdir" "$js" "$csc"
[ $salida -ne 0 ] && echo "  (gradle salio con $salida)"
[ $fallo -ne 0 ] && exit 1
echo "listo"
