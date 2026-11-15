# Actividad 4: sensores Contiki-NG en Azure IoT Hub

Adaptación moderna y funcional del capítulo "Contiki-NG en la nube" de Kurniawan (2018), *Practical Contiki-NG*,
cuyo código llega al enunciado con erratas (`clientFrom-ConnectionString`, `JSON.stringi-fy`, `message.proper-ties`)
y dependencias muertas (`request`, `azure-event-hubs`, `websense`/`httpd-simple` copiado al proyecto). Las cuatro
piezas quedan listas; lo único que falta es pegar las dos cadenas de conexión de Azure (ver "Azure en 5 minutos").

```
 ┌──────────────────────── contenedor contiker/contiki-ng (--privileged) ───────────────────────┐
 │  sensor/                       router/                                                      │
 │  mote Contiki-NG      6LoWPAN  rpl-border-router + tunslip6 (Cooja)   tun0 fd00::1/64        │
 │  httpd-ws :80  ──────────────▶ o TARGET=native (tun directo)  ──────▶ tcp-forward.py :8080 ──┼──┐
 │  {"temp":..,"hum":..}          fd00::302:304:506:708                                         │  │
 └──────────────────────────────────────────────────────────────────────────────────────────────┘  │ docker -p 127.0.0.1:8080
                                                                                                   ▼
   middleware/  Node 22: fetch cada 3 s ──▶ {deviceId,temperature,humidity} + prop. temperatureAlert
                azure-iot-device + azure-iot-device-mqtt  ── MQTT/TLS 8883 ──▶  Azure IoT Hub (F1)
                                                                                      │ built-in endpoint
                                                                                      │ (Event Hubs-compatible, AMQP)
   subscriber/  Node 22: @azure/event-hubs EventHubConsumerClient  ◀──────────────────┘
                imprime cada mensaje recibido
```

## Piezas

| Carpeta | Qué es | Decisión clave |
|---|---|---|
| `sensor/` | Mote Contiki-NG (`sensor-azure.c`) que responde a cualquier `GET` con `{"temp":15..39,"hum":80..89}` vía `random_rand()` | **HTTP, no CoAP.** El enunciado pide "servidor web" y el middleware de Kurniawan hace `GET`. Contiki-NG ya no trae `websense`, pero sí `os/net/app-layer/httpd-ws`, descendiente directo de `httpd-simple`: mismo modelo *script como protothread + `SEND_STRING`*, y el proyecto solo define `httpd_ws_get_script()`. Así la adaptación es mínima y el middleware sigue siendo un cliente HTTP con `fetch` nativo, sin librerías CoAP en Node. |
| `router/` | Instrucciones del `rpl-border-router` del árbol (Cooja + Serial Socket + `tunslip6`) y la variante `TARGET=native` verificada, más `tcp-forward.py` | Ver `router/README.md` |
| `middleware/` | `middleware-azure.js` (ESM, Node >= 22) | Sin `request` ni Express; `fetch` nativo; API de promesas del SDK; credenciales solo por entorno; modo `--dry-run` |
| `subscriber/` | `azure-sensor-subscriber.js` (ESM) con `@azure/event-hubs` 6 | `EventHubConsumerClient.subscribe` a todas las particiones desde `latestEventPosition` |

### Detalles del sensor (`sensor/project-conf.h`)

`httpd-ws` corre sobre TCP, que Contiki-NG trae apagado (`UIP_CONF_TCP 1`). Además exige que su buffer de entrada
sea mayor o igual que la ventana TCP, y con el MSS por defecto de native (~1200 B) no compila; se fijan
`UIP_CONF_TCP_MSS` y `UIP_CONF_RECEIVE_WINDOW` en 128 B (tamaño realista de mote) y los buffers de `httpd-ws` al
mismo tamaño, lo que además evita el `-Werror=format-truncation` que GCC moderno dispara en `httpd-ws.c:255`.
El `Content-Type` se fuerza a `application/json` desde `httpd_ws_get_script()`.

## Cómo correr

Todo lo de Contiki va dentro del contenedor; Node corre en el Mac. Desde la raíz del worktree:

```bash
# 1. sensor (compila, corre con tun y publica un puente TCP en 127.0.0.1:8080)
docker run -d --name a4sensor --privileged -p 127.0.0.1:8080:8080 -e CCACHE_DISABLE=1 \
  -v ~/contiki-ng:/home/user/contiki-ng -v "$PWD":/work -w /work/tarea4/a4-azure/sensor \
  contiker/contiki-ng sleep infinity
docker exec a4sensor make CONTIKI=/home/user/contiki-ng TARGET=native
docker exec -d a4sensor sh -c './build/native/sensor-azure.native > /work/tarea4/evidencia/a4-sensor.log 2>&1'
docker exec -d a4sensor python3 /work/tarea4/a4-azure/router/tcp-forward.py 8080 fd00::302:304:506:708 80
curl -s -i http://127.0.0.1:8080/        # -> HTTP/1.0 200 OK ... {"temp":..,"hum":..}

# 2. middleware
cd tarea4/a4-azure/middleware && npm install
cp .env.example .env                      # pegar AZURE_DEVICE_CONNECTION_STRING; SENSOR_URL=http://127.0.0.1:8080/
SENSOR_URL=http://127.0.0.1:8080/ node middleware-azure.js --dry-run          # sin Azure: imprime lo que mandaría
set -a; source .env; set +a; node middleware-azure.js                          # con Azure

# 3. suscriptor (otra terminal)
cd tarea4/a4-azure/subscriber && npm install
EVENTHUB_COMPATIBLE_CONNECTION_STRING='Endpoint=sb://...;EntityPath=...' node azure-sensor-subscriber.js

# 4. limpiar
docker rm -f a4sensor
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
| `a4-subscriber-sinvar.log` | El suscriptor arranca y falla con mensaje claro sin `EVENTHUB_COMPATIBLE_CONNECTION_STRING` |

Lo que **no** está verificado es el tramo contra Azure (envío real por MQTT y recepción en el suscriptor): requiere
las dos cadenas de conexión. Los comandos exactos están arriba (pasos 2 y 3 sin `--dry-run`).

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

Cooja con GUI (la variante A del router queda documentada, no corrida), mensajes cloud-to-device, y autenticación X.509
(el enunciado pide llave simétrica).
