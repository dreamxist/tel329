#!/usr/bin/env bash
# Chequeo rapido de sintaxis y tipos contra los headers reales de Contiki-NG.
# No enlaza ni genera firmware: para eso, make TARGET=cooja.
set -u

C="${CONTIKI:-$(ls -d "$HOME/contiki-ng" "$HOME/Downloads/contiki-ng" 2>/dev/null | head -1)}"
if [ -z "$C" ]; then
  echo "No encontre contiki-ng. Usa: CONTIKI=/ruta ./verificar-sintaxis.sh"
  exit 1
fi

R="$(cd "$(dirname "$0")" && pwd)"
INC="-I$C/os -I$C/os/sys -I$C/os/net -I$C/os/lib -I$C/os/dev -I$C/os/storage"
INC="$INC -I$C/arch/platform/cooja -I$C/arch/platform/cooja/dev -I$C/os/services"
BASE="-DCONTIKI=1 -DCONTIKI_TARGET_COOJA=1 -DNETSTACK_CONF_WITH_NULLNET=1 -DROUTING_CONF_NULLROUTING=1"

fallos=0
for spec in "actividad1-broadcast-boton/broadcast-boton.c|-DMAC_CONF_WITH_CSMA=1" \
            "actividad2-unicast-etimer/unicast-etimer.c|-DMAC_CONF_WITH_CSMA=1" \
            "actividad3-multicast-logico/multicast-logico.c|-DMAC_CONF_WITH_CSMA=1" \
            "actividad4-tsch-manual/tsch-manual.c|-DMAC_CONF_WITH_TSCH=1"; do
  f="${spec%%|*}"; mac="${spec##*|}"
  salida=$(cc -fsyntax-only -std=c99 -Wall $INC $BASE $mac "$R/$f" 2>&1)
  if [ -z "$salida" ]; then
    echo "OK   $(basename "$f")"
  else
    echo "FALLA $(basename "$f")"
    echo "$salida" | head -12
    fallos=$((fallos + 1))
  fi
done

echo
[ "$fallos" -eq 0 ] && echo "las 4 actividades pasan" || echo "$fallos con hallazgos"
exit "$fallos"
