# TEL329 · Tarea 4 — Mensajería CoAP y MQTT

Entrega jueves 19-nov-2026. Tres carpetas con código, una por actividad que lo
necesita, cada una un proyecto Contiki-NG (o Node) independiente, más la
evidencia de las corridas y la entrega armada.

```
tarea4/
├── TEL329-Tarea4-2026-v1.0.ipynb   el enunciado, sin tocar
├── respuestas.md                   las 4 actividades respondidas en un solo documento (empezar acá)
├── a1-coap/                        A1 · RPL Lite + CoAP en Cooja, y la mitad CoAP de la A3
├── a2-mqtt/                        A2 · RPL Lite + MQTT (border router + tunslip6 + Mosquitto), y la mitad MQTT de la A3
├── a4-azure/                       A4 · sensor httpd-ws + rpl-border-router + middleware Node + suscriptor Azure
├── evidencia/                      logs, pcap, CSV y resúmenes de las corridas (a1-*, a2-*, a4-*)
└── entrega/                        lo que se sube a Aula: notebook con respuestas, informe, pendientes
```

**La actividad 3 no tiene carpeta**: es la comparación de las capturas de A1 y
A2, y está armada en [respuestas.md](respuestas.md) §Actividad 3 a partir de
`evidencia/a1-resumen-coap.md` y `evidencia/a2-resumen-mqtt.md`.

Cada carpeta tiene su propio README con el diseño, las decisiones y cómo
correrla a mano o en la GUI: [a1-coap](a1-coap/README.md),
[a2-mqtt](a2-mqtt/README.md), [a4-azure](a4-azure/README.md)
(y [a4-azure/router](a4-azure/router/README.md) para el enrutador).

## Preparación

Igual que en las tareas anteriores: el symlink `contiki-ng` en la raíz del repo
(`ln -s ~/contiki-ng contiki-ng`) y la imagen oficial `contiker/contiki-ng`
para compilar y correr Cooja headless. Ver el README de la raíz. La A2 y la A4
necesitan `--privileged` porque crean una interfaz `tun`.

## Reproducir cada corrida

Desde la **raíz del worktree**, una línea por actividad:

```bash
# A1 · CoAP: compila, corre Cooja headless y deja evidencia/a1-coap.{log,pcap}, a1-tiempos-coap.csv, a1-resumen-coap.md
docker run --rm -e CCACHE_DISABLE=1 -e PCAP_PARA_RESUMEN=1 -v ~/contiki-ng:/home/user/contiki-ng -v ~/.gradle-contiker:/home/user/.gradle -v "$PWD":/work contiker/contiki-ng /work/tarea4/a1-coap/correr.sh

# A2 · MQTT: Mosquitto + Cooja headless + tunslip6 + tcpdump; deja evidencia/a2-mqtt.{log,pcap}, a2-mosquitto.log, a2-tunslip6.log, a2-tiempos-mqtt.csv, a2-resumen-mqtt.md
docker run --rm --privileged -e CCACHE_DISABLE=1 -v ~/contiki-ng:/home/user/contiki-ng -v tel329-gradle:/home/user/.gradle -v "$PWD":/work -w /work/tarea4/a2-mqtt contiker/contiki-ng ./correr.sh 300

# A3 · no corre nada: las tablas salen de los dos resúmenes de arriba (ver respuestas.md)

# A4 · sensor en el contenedor + middleware en --dry-run en el Mac (sin Azure); el paso a paso completo está en a4-azure/README.md
docker run -d --name a4sensor --privileged -p 127.0.0.1:8080:8080 -e CCACHE_DISABLE=1 -v ~/contiki-ng:/home/user/contiki-ng -v "$PWD":/work -w /work/tarea4/a4-azure/sensor contiker/contiki-ng sleep infinity && docker exec a4sensor make CONTIKI=/home/user/contiki-ng TARGET=native && docker exec -d a4sensor sh -c './build/native/sensor-azure.native > /work/tarea4/evidencia/a4-sensor.log 2>&1' && docker exec -d a4sensor python3 /work/tarea4/a4-azure/router/tcp-forward.py 8080 fd00::302:304:506:708 80 && (cd tarea4/a4-azure/middleware && npm install && SENSOR_URL=http://127.0.0.1:8080/ node middleware-azure.js --dry-run)
```

Los volúmenes de Gradle (`~/.gradle-contiker`, `tel329-gradle`) son opcionales:
evitan que cada `docker run --rm` vuelva a bajar la distribución que pide el
wrapper de Cooja. Las dos corridas de Cooja usan la semilla 123456; la A2 corre
a velocidad 1,0 porque el broker y `tunslip6` viven en el reloj del host.

## Qué está hecho y qué falta

| Actividad | Código | Respuesta escrita | Corrida + evidencia |
|---|---|---|---|
| A1 · RPL Lite + CoAP | ✅ | ✅ | ✅ log, pcap, CSV, resumen · ⚠️ falta captura GUI |
| A2 · RPL Lite + MQTT | ✅ | ✅ | ✅ logs, pcap en tun0, CSV, resumen · ⚠️ falta captura GUI |
| A3 · CoAP vs MQTT | — (no lleva) | ✅ tabla por saltos + análisis | ⚠️ pcap sin validar en Wireshark |
| A4 · Azure IoT Hub | ✅ | ✅ 4.a, 4.b, 4.c | ✅ sensor + middleware `--dry-run` en local · ❌ falta el envío real a Azure (dos cadenas de conexión) |

Todo lo pendiente está marcado **[FALTA]** en [respuestas.md](respuestas.md) y
con los pasos para cerrarlo en [entrega/Pendientes_Tarea4.md](entrega/Pendientes_Tarea4.md).

## La entrega

En [entrega/](entrega/): `TEL329-Tarea4-2026-v1.0.ipynb` es el notebook del
enunciado con una celda de respuesta debajo de cada tabla de actividades (es lo
que se sube), `Solucion_Tarea4.md` el informe autocontenido con la lista de
evidencia, `Pendientes_Tarea4.md` lo que falta y cómo cerrarlo, y
`TEL329-Tarea4-2026-v1.0.html` la conversión del notebook para leerlo sin
Jupyter.
