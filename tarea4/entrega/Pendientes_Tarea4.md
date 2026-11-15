# Pendientes para dejar lista la entrega — Tarea 4

Estado al 15-nov-2026 (entrega jueves 19-nov-2026).

## Ya completado

- Actividad 1 (RPL Lite + CoAP): código en `tarea4/a1-coap/`, corrida headless con 129 POST confirmables y 129 respuestas 2.04, log, pcap, CSV y resumen en `tarea4/evidencia/a1-*`.
- Actividad 2 (RPL Lite + MQTT): código en `tarea4/a2-mqtt/`, corrida headless con border router + `tunslip6` + Mosquitto, 273 PUBLISH QoS 1 y 273 PUBACK, logs, pcap en `tun0`, CSV y resumen en `tarea4/evidencia/a2-*`.
- Actividad 3: tabla comparativa CoAP vs MQTT por saltos y análisis, en `tarea4/respuestas.md` y en el notebook.
- Actividad 4: sensor (`httpd-ws`, `TARGET=native`), enrutador (`rpl-border-router` compilado para native), middleware y suscriptor escritos y probados en local (`--dry-run` contra el sensor real; suscriptor validando sus variables). Evidencia en `tarea4/evidencia/a4-*`.
- El notebook `TEL329-Tarea4-2026-v1.0.ipynb` de esta carpeta conserva las 22 celdas del enunciado y agrega una celda markdown de respuesta debajo de cada tabla de actividades; es JSON válido.

## Lo que falta, y cómo cerrarlo

### 1. Envío real a Azure IoT Hub (actividad 4.a y 4.c)

Es el único pendiente que cambia una respuesta. Hace falta un IoT Hub (nivel gratis F1 alcanza) y dos cadenas del portal; el paso a paso está en `tarea4/a4-azure/README.md` §"Azure en 5 minutos". En corto:

1. Portal Azure → **IoT Hub** → crear (tier **Free**) → **Devices** → **+ Add Device** con ID `contiki-ng-01` y llave simétrica autogenerada → abrir el dispositivo y copiar **Primary connection string**. Esa es `AZURE_DEVICE_CONNECTION_STRING` (formato `HostName=<hub>.azure-devices.net;DeviceId=contiki-ng-01;SharedAccessKey=...`).
2. En el hub → **Hub settings** → **Built-in endpoints** → sección **Event Hub compatible endpoint**, policy `service` → copiar el campo **Event Hub-compatible endpoint** completo. Esa es `EVENTHUB_COMPATIBLE_CONNECTION_STRING` (formato `Endpoint=sb://...;SharedAccessKeyName=service;SharedAccessKey=...;EntityPath=<hub>`).
3. Levantar el sensor en el contenedor (desde la raíz del worktree):

   ```bash
   docker run -d --name a4sensor --privileged -p 127.0.0.1:8080:8080 -e CCACHE_DISABLE=1 \
     -v ~/contiki-ng:/home/user/contiki-ng -v "$PWD":/work -w /work/tarea4/a4-azure/sensor \
     contiker/contiki-ng sleep infinity
   docker exec a4sensor make CONTIKI=/home/user/contiki-ng TARGET=native
   docker exec -d a4sensor sh -c './build/native/sensor-azure.native > /work/tarea4/evidencia/a4-sensor.log 2>&1'
   docker exec -d a4sensor python3 /work/tarea4/a4-azure/router/tcp-forward.py 8080 fd00::302:304:506:708 80
   curl -s -i http://127.0.0.1:8080/
   ```

4. Suscriptor primero (terminal A), para que esté escuchando cuando lleguen los mensajes:

   ```bash
   cd tarea4/a4-azure/subscriber && npm install
   EVENTHUB_COMPATIBLE_CONNECTION_STRING='<cadena del paso 2>' \
     node azure-sensor-subscriber.js | tee ../../evidencia/a4-subscriber-azure.log
   ```

5. Middleware (terminal B):

   ```bash
   cd tarea4/a4-azure/middleware && npm install
   AZURE_DEVICE_CONNECTION_STRING='<cadena del paso 1>' SENSOR_URL=http://127.0.0.1:8080/ \
     node middleware-azure.js | tee ../../evidencia/a4-middleware-azure.log
   ```

6. Dejar correr un minuto; capturar el gráfico **Overview → Device to cloud messages** del portal como `tarea4/evidencia/a4-portal-d2c.png`. Después `docker rm -f a4sensor` y borrar el resource group.
7. Reemplazar en `tarea4/respuestas.md` (4.a y 4.c) y en la celda de la actividad 4 del notebook los bloques `[FALTA: cadena de conexión de Azure]` por las salidas reales de los dos logs, y agregar los tres archivos nuevos a la lista de evidencia de `Solucion_Tarea4.md`.

Las cadenas **no se versionan**: van en `.env` (ignorado) o en la línea de comando; `.env.example` es lo único que entra al repo.

### 2. Publicar la adaptación de la actividad 4 (decisión de Pancho)

El enunciado pide publicar el código adaptado en GitHub o equivalente para la evaluación. El repo `dreamxist/tel329` es **privado**. Tres salidas, elegir una antes del jueves:

- **Hacer público el repo** `dreamxist/tel329` (un clic en Settings → Danger zone; el repo no tiene secretos, `.env` está ignorado) y poner en el notebook la URL `https://github.com/dreamxist/tel329/tree/main/tarea4/a4-azure`.
- **Repo aparte** público solo con `tarea4/a4-azure/` (`git subtree split` o copiar la carpeta) y enlazar ese.
- **Adjuntar un zip** de `tarea4/a4-azure/` (sin `node_modules/` ni `build/`) junto al notebook en Aula, si la profesora acepta "equivalente".

En cualquiera de los tres, la URL o el nombre del zip va en la celda de respuesta de la actividad 4, donde hoy dice `[FALTA]`.

### 3. Capturas gráficas de Cooja (actividades 1 y 2)

Las corridas fueron headless, así que no hay capturas del Visualizer, Timeline, Log Listener ni Radio Logger. No cambian ningún número, pero el enunciado dice "visualice sus resultados en el simulador Cooja o Terminal" y una imagen cierra la duda. Se necesita Cooja con GUI (Linux con Java o el contenedor `contiker` con X11; en el Mac no hay):

- Actividad 1: abrir `tarea4/a1-coap/a1-coap.csc`. Trae *Visualizer*, *Log Listener* filtrado por `A1`, *Timeline* y un *Radio Logger* configurado con "6LoWPAN Analyzer with PCAP" que escribe `../evidencia/a1-coap-gui.pcap`. Semilla 123456, `speedlimit` 20. Capturar el DODAG en el Visualizer y una ronda `TX`/`REG`/`RX` en el Log Listener → `evidencia/a1-cooja-*.png`.
- Actividad 2: abrir `tarea4/a2-mqtt/a2-mqtt.csc`; el *Serial Socket (SERVER)* del mote 1 arranca solo en el puerto 60001. En otra terminal: `mosquitto -c tarea4/a2-mqtt/mosquitto.conf -v`, `sudo ~/contiki-ng/tools/serial-io/tunslip6 -a 127.0.0.1 -p 60001 fd00::1/64` y `mosquitto_sub -h ::1 -t 'usm/tel329/temp' -v`. Capturar el Log Listener (filtro `A2,`) con `CONNECTED`/`PUB`/`PUBACK` y la terminal de `mosquitto_sub` → `evidencia/a2-cooja-*.png`.

### 4. Validar los pcap con Wireshark (actividad 3)

Los dos pcap están y se validaron por estructura (Python para `a1-coap.pcap`; `tcpdump -v` para `a2-mqtt.pcap`), pero no se abrieron en Wireshark ni `tshark` porque no están instalados. Para cerrar:

- `a1-coap.pcap` (linktype 195, tramas 802.15.4 **sin FCS**): en Wireshark, *Preferences → Protocols → IEEE 802.15.4 → FCS format: None*, o en línea de comando `tshark -o wpan.fcs_format:0 -r tarea4/evidencia/a1-coap.pcap -Y coap`. 6LoWPAN y CoAP se decodifican solos; una captura de pantalla de un `POST` con su `2.04` y el tiempo delta → `evidencia/a1-wireshark.png`.
- `a2-mqtt.pcap` (capturado en `tun0`, IPv6/TCP): `tshark -r tarea4/evidencia/a2-mqtt.pcap -Y mqtt`; captura de un `PUBLISH` y su `PUBACK` → `evidencia/a2-wireshark.png`.

### 5. Separar protocolo de camino (opcional)

La comparación de la actividad 3 mezcla CoAP terminado en la raíz con MQTT terminado en un broker detrás de `tunslip6`. Está dicho con todas las letras en el análisis, así que no bloquea la entrega; si se quisiera aislar el costo del protocolo, habría que correr el servidor CoAP también en el host detrás del border router (por ejemplo `coap-cli` o `libcoap` escuchando en `fd00::1`) y repetir la tabla. No está hecho.

## Alcance de la comparación

Una corrida por protocolo, semilla 123456, UDGM con éxito 1,0, seis motes, broker en la misma máquina. Sirve como resultado observado del experimento, no como comparación estadística general; repetir con varias semillas sería una ampliación opcional.

## Ubicación

Todo vive en el repo `tel329`, rama `tarea4`, carpeta `tarea4/`: código por actividad (`a1-coap/`, `a2-mqtt/`, `a4-azure/`), evidencia (`evidencia/`), respuestas consolidadas (`respuestas.md`) y la entrega (`entrega/`).
