# Actividad 4: sensores Contiki-NG en la nube (broker MQTT local; Azure IoT Hub como variante)

Adaptación moderna y funcional del capítulo "Contiki-NG en la nube" de Kurniawan (2018), *Practical Contiki-NG*,
cuyo código llega al enunciado con erratas (`clientFrom-ConnectionString`, `JSON.stringi-fy`, `message.proper-ties`)
y dependencias muertas (`request`, `azure-event-hubs`, `websense`/`httpd-simple` copiado al proyecto).

**La nube es un broker MQTT local.** El enunciado dice que la actividad "implica la ejecución de Contiki-NG ya sea en
un host local o en la nube de Microsoft Azure" y que el código "puede ser adaptado de acuerdo al escenario o caso de
uso que determine". Se eligió el host local: Mosquitto, que ya viene en la imagen `contiker/contiki-ng`, cumple en la
cadena el mismo rol que IoT Hub, recibe la telemetría *device-to-cloud* del middleware y se la entrega al suscriptor.
El camino Azure (`azure-iot-device-mqtt` + `@azure/event-hubs`) sigue implementado como variante, pero no se ejecutó
por falta de suscripción.

```
 ┌──────────────────────── contenedor contiker/contiki-ng (--privileged) ───────────────────────┐
 │  sensor/                       router/                                                      │
 │  mote Contiki-NG      6LoWPAN  rpl-border-router + tunslip6 (Cooja)   tun0 fd00::1/64        │
 │  httpd-ws :80  ──────────────▶ o TARGET=native (tun directo)  ──────▶ tcp-forward.py :8080 ──┼──┐ -p 127.0.0.1:8080
 │  {"temp":..,"hum":..}          fd00::302:304:506:708                                         │  │
 │                                                                                              │  │
 │  mosquitto :1883 (a2-mqtt/mosquitto.conf)  ◀────────────────────────────────────────────────┼──┼─┐ -p 127.0.0.1:1883
 └──────────────────────────────────────────────────────────────────────────────────────────────┘  │ │
                                                                                                   ▼ │
   middleware/  Node 22: fetch cada 3 s ──▶ {deviceId,temperature,humidity,timestamp}               │
                --destino local: mqtt (MQTT 5, QoS 1) ── usm/tel329/cloud/contiki-ng-01/telemetry ──┤
                --destino azure: azure-iot-device-mqtt ── MQTT/TLS 8883 ──▶ Azure IoT Hub (F1)       │
                                                                                                     │
   subscriber/  cloud-subscriber.js: mqtt, usm/tel329/cloud/+/telemetry  ◀──────────────────────────┘
                azure-sensor-subscriber.js: @azure/event-hubs al built-in endpoint (variante Azure)
```

## Piezas

| Carpeta | Qué es | Decisión clave |
|---|---|---|
| `sensor/` | Mote Contiki-NG (`sensor-azure.c`) que responde a cualquier `GET` con `{"temp":15..39,"hum":80..89}` vía `random_rand()` | **HTTP, no CoAP.** El enunciado pide "servidor web" y el middleware de Kurniawan hace `GET`. Contiki-NG ya no trae `websense`, pero sí `os/net/app-layer/httpd-ws`, descendiente directo de `httpd-simple`: mismo modelo *script como protothread + `SEND_STRING`*, y el proyecto solo define `httpd_ws_get_script()`. Así la adaptación es mínima y el middleware sigue siendo un cliente HTTP con `fetch` nativo, sin librerías CoAP en Node. |
| `router/` | Instrucciones del `rpl-border-router` del árbol (Cooja + Serial Socket + `tunslip6`) y la variante `TARGET=native` verificada, más `tcp-forward.py` | Ver `router/README.md` |
| `middleware/` | `middleware-azure.js` (ESM, Node >= 22) con dos destinos: `--destino local` (o `DESTINO=local`) y `azure` (por defecto) | Sin `request` ni Express; `fetch` nativo; en local, paquete `mqtt` con **MQTT 5**, QoS 1 y `temperatureAlert` como *user property* (ver abajo); en Azure, API de promesas del SDK; credenciales solo por entorno; modo `--dry-run` |
| `subscriber/` | `cloud-subscriber.js` (broker local) y `azure-sensor-subscriber.js` (variante Azure, `@azure/event-hubs` 6) | Local: `mqtt` suscrito a `usm/tel329/cloud/+/telemetry`, imprime hora de llegada y latencia middleware → suscriptor y al salir n, mediana y máximo. Azure: `EventHubConsumerClient.subscribe` a todas las particiones desde `latestEventPosition` |
| `correr-local.sh` | La cadena completa sin Azure en un comando | Levanta el contenedor, compila el sensor, arranca sensor + puente + Mosquitto, corre suscriptor y middleware N segundos en el Mac, deja los logs en `evidencia/` y borra el contenedor |

### Por qué MQTT 5 y `temperatureAlert` como *user property*

En IoT Hub `temperatureAlert` es una **propiedad de aplicación**: viaja fuera del cuerpo JSON y el suscriptor la lee
aparte (`event.properties`). MQTT 3.1.1 no tiene dónde ponerla salvo dentro del JSON, lo que cambia el cuerpo; MQTT 5
(OASIS 2019, §3.3.2.3.7) trae *User Property* en el `PUBLISH`, que es exactamente ese lugar. Mosquitto 2.0 habla
MQTT 5, así que el cuerpo queda idéntico al de Azure más `timestamp`, y la alerta va como `userProperties:
{temperatureAlert: "true"|"false"}`, con `contentType: application/json`. El middleware publica con QoS 1 y registra el
`PUBACK` de cada mensaje con su `packet id` y el tiempo hasta recibirlo.

### Detalles del sensor (`sensor/project-conf.h`)

`httpd-ws` corre sobre TCP, que Contiki-NG trae apagado (`UIP_CONF_TCP 1`). Además exige que su buffer de entrada
sea mayor o igual que la ventana TCP, y con el MSS por defecto de native (~1200 B) no compila; se fijan
`UIP_CONF_TCP_MSS` y `UIP_CONF_RECEIVE_WINDOW` en 128 B (tamaño realista de mote) y los buffers de `httpd-ws` al
mismo tamaño, lo que además evita el `-Werror=format-truncation` que GCC moderno dispara en `httpd-ws.c:255`.
El `Content-Type` se fuerza a `application/json` desde `httpd_ws_get_script()`.

## Cómo correr

Todo lo de Contiki y el broker van dentro del contenedor; Node corre en el Mac. Desde la raíz del worktree.

### Cadena local (la respuesta de la entrega)

```bash
tarea4/a4-azure/correr-local.sh 90       # 90 s de corrida; deja los cuatro logs a4-*-local / a4-mosquitto en evidencia/
```

Es lo mismo que, a mano:

```bash
# 1. sensor + puente + broker en el contenedor
docker run -d --name a4local --privileged -p 127.0.0.1:8080:8080 -p 127.0.0.1:1883:1883 -e CCACHE_DISABLE=1 \
  -v ~/contiki-ng:/home/user/contiki-ng -v "$PWD":/work -w /work/tarea4/a4-azure/sensor \
  contiker/contiki-ng sleep infinity
docker exec a4local make CONTIKI=/home/user/contiki-ng TARGET=native
docker exec -d a4local sh -c './build/native/sensor-azure.native > /work/tarea4/evidencia/a4-sensor-local.log 2>&1'
docker exec -d a4local python3 /work/tarea4/a4-azure/router/tcp-forward.py 8080 fd00::302:304:506:708 80
docker exec -d a4local sh -c 'mosquitto -c /work/tarea4/a2-mqtt/mosquitto.conf -v > /work/tarea4/evidencia/a4-mosquitto.log 2>&1'
curl -s -i http://127.0.0.1:8080/        # -> HTTP/1.0 200 OK ... {"temp":..,"hum":..}

# 2. suscriptor (terminal A)
cd tarea4/a4-azure/subscriber && npm install && node cloud-subscriber.js

# 3. middleware (terminal B)
cd tarea4/a4-azure/middleware && npm install
SENSOR_URL=http://127.0.0.1:8080/ node middleware-azure.js --destino local   # o DESTINO=local

# 4. limpiar
docker rm -f a4local
```

### Variante Azure (implementada, no ejecutada por falta de suscripción)

Igual que arriba sin Mosquitto, y con las dos cadenas del portal (ver "Azure en 5 minutos"):

```bash
cd tarea4/a4-azure/middleware
cp .env.example .env                      # pegar AZURE_DEVICE_CONNECTION_STRING; SENSOR_URL=http://127.0.0.1:8080/
SENSOR_URL=http://127.0.0.1:8080/ node middleware-azure.js --dry-run          # sin Azure: imprime lo que mandaría
set -a; source .env; set +a; node middleware-azure.js                          # con Azure (--destino azure por defecto)

cd ../subscriber
EVENTHUB_COMPATIBLE_CONNECTION_STRING='Endpoint=sb://...;EntityPath=...' node azure-sensor-subscriber.js
```

Node no carga `.env` solo: se usa `set -a; source .env; set +a` (o `node --env-file=.env middleware-azure.js`, que
Node 22 soporta). `.env` está en `.gitignore`; solo se versiona `.env.example`.

## Evidencia local (`tarea4/evidencia/`)

| Archivo | Qué muestra |
|---|---|
| `a4-sensor.log` | Arranque del mote native (tun0 `fd00::1/64`, RPL Lite, link-layer `0102.0304.0506.0708`) y cada `GET` atendido con su JSON |
| `a4-curl.txt` | `ifconfig tun0`, `ping6` al mote (0 % pérdida), tres `GET http://[fd00::302:304:506:708]/` con `200 OK` + `Content-Type: application/json`, y el `curl` desde el Mac vía el puente |
| `a4-middleware-dryrun.log` | Middleware real contra el sensor real: cuatro lecturas en 11 s, mensaje armado con `deviceId/temperature/humidity` y `temperatureAlert` que cambia a `true` con `temp=35` |
| `a4-border-router-build.log` | `make TARGET=native` del `rpl-border-router` del árbol, exit 0 |
| `a4-subscriber-sinvar.log` | El suscriptor Azure arranca y falla con mensaje claro sin `EVENTHUB_COMPATIBLE_CONNECTION_STRING` |
| `a4-sensor-local.log` | Corrida de punta a punta (`correr-local.sh 90`): arranque del mote y los 32 `GET` atendidos (2 del chequeo de arranque, 30 del middleware) |
| `a4-mosquitto.log` | El broker en `-v`: suscriptor y middleware conectados en MQTT 5 (`p5`), y por cada dato `Received PUBLISH` del middleware, `Sending PUBLISH` al suscriptor, `Sending PUBACK` al middleware y `Received PUBACK` del suscriptor (30 de cada uno) |
| `a4-middleware-local.log` | 30 lecturas en 87 s y 30 `PUBLISH` QoS 1 (*packet id* 59099–59128) con su `PUBACK` y el tiempo hasta él (mediana 1,26 ms, máx 11,07 ms); 12 con `temperatureAlert=true` |
| `a4-subscriber-local.log` | Los 30 mensajes recibidos, en orden e idénticos a los publicados, con hora de llegada, `temperatureAlert` como *user property* y latencia; al final `n=30 min=1 mediana=1 media=2.20 max=11` (ms) |

El camino Azure (envío a IoT Hub y recepción en `azure-sensor-subscriber.js`) está implementado pero **no se ejecutó
por falta de suscripción**; los comandos exactos están en "Variante Azure" y el portal en la sección siguiente.

## Azure en 5 minutos (portal, nivel gratis F1)

Fuentes: Microsoft Learn, "Create an IoT hub" (<https://learn.microsoft.com/azure/iot-hub/create-hub>, 2025-06),
"Register and connect an IoT device" (<https://learn.microsoft.com/azure/iot-hub/create-connect-device?tabs=portal>,
2026-09) y "Understand the built-in endpoint" (<https://learn.microsoft.com/azure/iot-hub/iot-hub-devguide-messages-read-builtin>, 2025-05).

1. Entrar a <https://portal.azure.com> > **+ Create a resource** > categoría **Internet of Things** > **IoT Hub**.
2. Pestaña **Basics**: elegir suscripción, **Resource group** (*Create new*, p. ej. `tel329`), **IoT hub name** único (p. ej. `tel329-pzuniga`), **Region** cercana (`Brazil South` o `East US`).
3. En **Tier** elegir **Free** (F1: 8.000 mensajes/día, 500 dispositivos; solo un hub F1 por suscripción y no se puede subir de nivel después). Dejar **Daily message limit** en su único valor.
4. **Next: Networking** y **Next: Management** con los valores por defecto (**Public access**, **Permission model** con *Shared access policies* permitido, 2 particiones en F1); **Review + create** > **Create**; esperar y **Go to resource**.
5. En el hub, menú **Device management** > **Devices** > **+ Add Device**.
6. **Device ID** `contiki-ng-01`, **Authentication type** **Symmetric key**, dejar marcado **Auto-generate keys** y **Enable connection to IoT Hub** en *Enable*; **Save**.
7. Abrir el dispositivo recién creado y copiar **Primary connection string** (formato `HostName=<hub>.azure-devices.net;DeviceId=contiki-ng-01;SharedAccessKey=...`); va en `AZURE_DEVICE_CONNECTION_STRING` de `middleware/.env`.
8. Volver al hub, menú **Hub settings** > **Built-in endpoints**.
9. En la sección **Event Hub compatible endpoint**, con **Shared access policy** = `service` (o `iothubowner`), copiar el campo **Event Hub-compatible endpoint** completo (formato `Endpoint=sb://...servicebus.windows.net/;SharedAccessKeyName=...;SharedAccessKey=...;EntityPath=...`); va en `EVENTHUB_COMPATIBLE_CONNECTION_STRING` del suscriptor. El consumer group `$Default` ya existe.
10. Correr suscriptor y middleware como en "Cómo correr"; en el portal, **Overview** > gráfico *Device to cloud messages* debe subir. Al terminar la tarea, borrar el resource group para no dejar recursos.

## Fuera de alcance

Cooja con GUI (la variante A del router queda documentada, no corrida), la corrida contra Azure IoT Hub (variante
implementada, sin suscripción), mensajes cloud-to-device, TLS y autenticación en el broker local (la red es una
simulación cerrada) y autenticación X.509 en Azure (el enunciado pide llave simétrica).
