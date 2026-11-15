# Pendientes para dejar lista la entrega — Tarea 4

Estado al 15-nov-2026 (entrega jueves 19-nov-2026).

## Ya completado

- Actividad 1 (RPL Lite + CoAP): código en `tarea4/a1-coap/`, corrida headless con 129 POST confirmables y 129 respuestas 2.04, log, pcap, CSV y resumen en `tarea4/evidencia/a1-*`.
- Actividad 2 (RPL Lite + MQTT): código en `tarea4/a2-mqtt/`, corrida headless con border router + `tunslip6` + Mosquitto, 273 PUBLISH QoS 1 y 273 PUBACK, logs, pcap en `tun0`, CSV y resumen en `tarea4/evidencia/a2-*`.
- Actividad 3: tabla comparativa CoAP vs MQTT por saltos y análisis, en `tarea4/respuestas.md` y en el notebook. Los dos pcap se validaron con `tshark` 3.6.2 en un contenedor efímero: `a1-coap.pcap` decodifica 179 `POST` y 179 `2.04` (129 MID distintos de cada uno, las 129 peticiones del log; lo demás son reenvíos a dos saltos) y `a2-mqtt.pcap` decodifica 5 `CONNECT`, 5 `CONNACK`, 273 `PUBLISH` y 273 `PUBACK`. Salida en `tarea4/evidencia/a1-tshark.txt` y `a2-tshark.txt`.
- Actividad 4: la nube es un broker MQTT local (Mosquitto en el contenedor), en vez de Azure IoT Hub; el enunciado admite "un host local o la nube de Microsoft Azure" y que el código "puede ser adaptado de acuerdo al escenario o caso de uso". Cadena completa sensor (`httpd-ws`, `TARGET=native`) → puente TCP → middleware `--destino local` (MQTT 5, QoS 1) → Mosquitto → `cloud-subscriber.js`, corrida 90 s con `tarea4/a4-azure/correr-local.sh`: 30 lecturas, 30 `PUBLISH` con `PUBACK`, 30 mensajes recibidos idénticos; latencia middleware → suscriptor mediana 1 ms, máx 11 ms. Evidencia en `tarea4/evidencia/a4-*`. El camino Azure queda como variante implementada, no ejecutada por falta de suscripción; si algún día se quiere correr, el paso a paso está en `tarea4/a4-azure/README.md` §"Variante Azure" y §"Azure en 5 minutos".
- Publicación de la actividad 4: el repo `dreamxist/tel329` es público y el notebook enlaza `https://github.com/dreamxist/tel329/tree/main/tarea4/a4-azure`.
- El notebook `TEL329-Tarea4-2026-v1.0.ipynb` de esta carpeta conserva las 22 celdas del enunciado y agrega una celda markdown de respuesta debajo de cada tabla de actividades; es JSON válido.

## Lo que falta, y cómo cerrarlo

### 1. Capturas gráficas de Cooja (actividades 1 y 2)

Las corridas fueron headless, así que no hay capturas del Visualizer, Timeline, Log Listener ni Radio Logger. No cambian ningún número, pero el enunciado dice "visualice sus resultados en el simulador Cooja o Terminal" y una imagen cierra la duda. Se necesita Cooja con GUI (Linux con Java o el contenedor `contiker` con X11; en el Mac no hay):

- Actividad 1: abrir `tarea4/a1-coap/a1-coap.csc`. Trae *Visualizer*, *Log Listener* filtrado por `A1`, *Timeline* y un *Radio Logger* configurado con "6LoWPAN Analyzer with PCAP" que escribe `../evidencia/a1-coap-gui.pcap`. Semilla 123456, `speedlimit` 20. Capturar el DODAG en el Visualizer y una ronda `TX`/`REG`/`RX` en el Log Listener → `evidencia/a1-cooja-*.png`.
- Actividad 2: abrir `tarea4/a2-mqtt/a2-mqtt.csc`; el *Serial Socket (SERVER)* del mote 1 arranca solo en el puerto 60001. En otra terminal: `mosquitto -c tarea4/a2-mqtt/mosquitto.conf -v`, `sudo ~/contiki-ng/tools/serial-io/tunslip6 -a 127.0.0.1 -p 60001 fd00::1/64` y `mosquitto_sub -h ::1 -t 'usm/tel329/temp' -v`. Capturar el Log Listener (filtro `A2,`) con `CONNECTED`/`PUB`/`PUBACK` y la terminal de `mosquitto_sub` → `evidencia/a2-cooja-*.png`.

### 2. Separar protocolo de camino (opcional)

La comparación de la actividad 3 mezcla CoAP terminado en la raíz con MQTT terminado en un broker detrás de `tunslip6`. Está dicho con todas las letras en el análisis, así que no bloquea la entrega; si se quisiera aislar el costo del protocolo, habría que correr el servidor CoAP también en el host detrás del border router (por ejemplo `coap-cli` o `libcoap` escuchando en `fd00::1`) y repetir la tabla. No está hecho.

## Alcance de la comparación

Una corrida por protocolo, semilla 123456, UDGM con éxito 1,0, seis motes, broker en la misma máquina. Sirve como resultado observado del experimento, no como comparación estadística general; repetir con varias semillas sería una ampliación opcional.

## Ubicación

Todo vive en el repo `tel329`, rama `tarea4`, carpeta `tarea4/`: código por actividad (`a1-coap/`, `a2-mqtt/`, `a4-azure/`), evidencia (`evidencia/`), respuestas consolidadas (`respuestas.md`) y la entrega (`entrega/`).
