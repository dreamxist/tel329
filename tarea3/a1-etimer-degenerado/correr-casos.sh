#!/bin/bash
#
# Corre los cuatro casos de la Actividad 1 en Cooja headless y deja un log por
# caso en ../evidencia/.
#
# Pensado para correr DENTRO del contenedor (usa sed de GNU). Desde el host:
#
#   docker run --rm -e CCACHE_DISABLE=1 \
#     -v ~/contiki-ng:/home/user/contiki-ng \
#     -v "$PWD":/work \
#     contiker/contiki-ng /work/tarea3/a1-etimer-degenerado/correr-casos.sh
#
# Opcional: pasar los casos como argumentos para correr solo algunos.
#
#   ./correr-casos.sh base
#   ./correr-casos.sh 1e-5 1e-10
#
set -u

AQUI=$(cd "$(dirname "$0")" && pwd)
EVIDENCIA="$AQUI/../evidencia"

# Misma resolucion de ruta que el Makefile, por el mismo motivo.
CONTIKI=${CONTIKI:-$(ls -d "$AQUI/../../contiki-ng" "$HOME/contiki-ng" 2>/dev/null | head -1)}
if [ ! -f "$CONTIKI/Makefile.include" ]; then
  echo "No encuentro Contiki-NG. Exportar CONTIKI=/ruta/a/contiki-ng" >&2
  exit 1
fi

CASOS=${*:-"base 1e-5 1e-10 1e-20"}

# Segundos de tiempo SIMULADO por caso. El caso base necesita margen para que
# el DODAG converja y entren varias rondas de 10 s; los degenerados se cortan
# antes por cantidad de lineas, asi que no vale la pena pedirles mas.
segundos_de() {
  case "$1" in
    base) echo 180 ;;
    *)    echo 90 ;;
  esac
}

mkdir -p "$EVIDENCIA"
fallos=0

for caso in $CASOS; do
  echo "=== caso $caso ==="

  # make NO detecta el cambio de CFLAGS: sin distclean queda el binario viejo.
  make -C "$AQUI" distclean TARGET=cooja CONTIKI="$CONTIKI" >/dev/null 2>&1
  if ! make -C "$AQUI" TARGET=cooja CONTIKI="$CONTIKI" CASO="$caso" -j4 >/dev/null; then
    echo "  compilacion FALLO" >&2
    fallos=$((fallos + 1))
    continue
  fi

  # El .csc headless tiene que vivir junto a los fuentes: [CONFIG_DIR] es el
  # directorio del propio .csc, y de ahi salen los .c de cada tipo de mote.
  js="$AQUI/.headless-$caso.js"
  csc="$AQUI/.headless-$caso.csc"
  # El limite va en milisegundos y como entero literal: Cooja lo saca del texto
  # del script con una regex que solo acepta digitos (ver js/volcar-log.js).
  sed "s/^TIMEOUT(.*/TIMEOUT($(( $(segundos_de "$caso") * 1000 )), log.testOK());/" \
      "$AQUI/js/volcar-log.js" > "$js"

  plugin="  <plugin>\n\
    org.contikios.cooja.plugins.ScriptRunner\n\
    <plugin_config>\n\
      <scriptfile>$js<\/scriptfile>\n\
      <active>true<\/active>\n\
    <\/plugin_config>\n\
  <\/plugin>"
  # CONTIKI va explicito en el comando de compilacion del .csc generado.
  # Cooja compila el mote con un entorno sin $HOME, asi que de los candidatos
  # del Makefile solo le sirve el symlink relativo -- y dentro del contenedor
  # ese symlink apunta a una ruta del host que no existe. Pasarlo a mano evita
  # depender de como esten montados los volumenes.
  sed -e "s|TARGET=cooja|TARGET=cooja CONTIKI=$CONTIKI|" \
      -e "s/CASO=base/CASO=$caso/" \
      -e "s|</simconf>|$plugin\n</simconf>|" \
      "$AQUI/rpl-udp.csc" > "$csc"

  logdir=$(mktemp -d)
  (cd "$CONTIKI/tools/cooja" && \
    ./gradlew --no-daemon --quiet run \
      --args="--no-gui --random-seed=123456 --logdir=$logdir $csc") \
    > "$logdir/stdout.txt" 2>&1
  salida=$?

  if [ -f "$logdir/COOJA.testlog" ]; then
    cp "$logdir/COOJA.testlog" "$EVIDENCIA/a1-$caso.log"
    echo "  -> $EVIDENCIA/a1-$caso.log ($(wc -l < "$EVIDENCIA/a1-$caso.log") lineas)"
    grep -m1 "intervalo = " "$EVIDENCIA/a1-$caso.log" || true
  else
    echo "  sin COOJA.testlog; stdout de Cooja:" >&2
    tail -20 "$logdir/stdout.txt" >&2
    fallos=$((fallos + 1))
  fi

  rm -rf "$logdir" "$js" "$csc"
  [ $salida -ne 0 ] && echo "  (gradle salio con $salida)"
done

if [ $fallos -gt 0 ]; then
  echo "casos con problemas: $fallos" >&2
  exit 1
fi
echo "listo"
