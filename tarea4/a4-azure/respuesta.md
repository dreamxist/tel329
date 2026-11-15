# Actividad 4 - Respuestas

Código en `tarea4/a4-azure/` y evidencia en `tarea4/evidencia/a4-*`. **La nube de esta entrega es un broker MQTT local, no Azure.** El enunciado dice que la actividad "implica la ejecución de Contiki-NG ya sea en un host local o en la nube de Microsoft Azure" y que el código "puede ser adaptado de acuerdo al escenario o caso de uso que determine"; se tomó el host local. Mosquitto 2.0.11, que ya viene en la imagen `contiker/contiki-ng`, cumple en la cadena el mismo rol que IoT Hub: recibe la telemetría *device-to-cloud* del middleware y se la entrega al suscriptor. La cadena sensor → enrutador → middleware → broker → suscriptor corrió completa durante 90 s (`a4-azure/correr-local.sh`). El camino Azure (`azure-iot-device-mqtt` hacia IoT Hub y `@azure/event-hubs` en el suscriptor) queda como **variante implementada, no ejecutada por falta de suscripción**; los pasos para correrla están en `a4-azure/README.md` §"Azure en 5 minutos".

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
                --destino azure: azure-iot-device-mqtt ── MQTT/TLS 8883 ──▶ Azure IoT Hub (variante) │
                                                                                                     │
   subscriber/  cloud-subscriber.js: mqtt, usm/tel329/cloud/+/telemetry  ◀──────────────────────────┘
                azure-sensor-subscriber.js: @azure/event-hubs al built-in endpoint (variante Azure)
```

## 4.b ¿Cuál es el resultado presentado por el sensor o mote?

**Planteamiento.** El sensor es un mote Contiki-NG que corre un servidor HTTP y responde a cualquier `GET` con un JSON `{"temp":T,"hum":H}`, con `T = 15 + random_rand() % 25` y `H = 80 + random_rand() % 10`, igual que el `websense-Cloud` de Kurniawan. La diferencia es cómo se consigue el servidor: Contiki-NG ya no trae `websense` ni `httpd-simple` como módulo reutilizable (sólo queda una copia privada dentro de `examples/rpl-border-router/webserver`), pero sí trae `os/net/app-layer/httpd-ws`, su descendiente directo: el proyecto define `httpd_ws_get_script()` y devuelve un protothread que escribe con `SEND_STRING`, exactamente el patrón de `generate_routes()` del enunciado.

```c
static
PT_THREAD(generate_json(struct httpd_ws_state *s))
{
  PSOCK_BEGIN(&s->sout);
  {
    unsigned temperature = 15 + (random_rand() % 25);
    unsigned humidity = 80 + (random_rand() % 10);
    snprintf(json_buf, sizeof(json_buf), "{\"temp\":%u,\"hum\":%u}", temperature, humidity);
    LOG_INFO("GET %s -> %s\n", s->filename, json_buf);
  }
  SEND_STRING(&s->sout, json_buf, strlen(json_buf));
  PSOCK_END(&s->sout);
}

httpd_ws_script_t
httpd_ws_get_script(struct httpd_ws_state *s)
{
  s->content_type = json_content_type;   /* "application/json" */
  return generate_json;
}
```

Por eso se eligió HTTP sobre CoAP: el enunciado habla de servidor web, el middleware original hace `GET`, y así el tramo Node queda con `fetch` nativo sin librerías CoAP. Hubo que habilitar TCP (`UIP_CONF_TCP 1`), fijar MSS y ventana TCP en 128 B y alinear los buffers de `httpd-ws` (sin eso no compila: la cabecera exige buffer >= ventana y GCC moderno dispara `-Werror=format-truncation` en `httpd-ws.c:255`), y forzar `Content-Type: application/json`. El mote se compiló en el contenedor para `TARGET=native`, donde la plataforma crea el `tun0` (`fd00::1/64`) y el nodo queda en `fd00::302:304:506:708`.

**Evidencia** (`evidencia/a4-sensor.log`, `evidencia/a4-curl.txt`, `evidencia/a4-sensor-local.log`). Al arrancar, el mote reporta `Routing: RPL Lite`, `Net: tun6`, link-layer `0102.0304.0506.0708` y `tun0` con `fd00::1/64`. Desde el contenedor, `ping6 -c 3 fd00::302:304:506:708` responde los tres paquetes (0 % de pérdida, RTT 0,07 a 0,77 ms) y tres `GET http://[fd00::302:304:506:708]/` devuelven `HTTP 200 OK`, `Server: Contiki`, `Content-Type: application/json` con cuerpos `{"temp":24,"hum":80}`, `{"temp":24,"hum":81}` y `{"temp":16,"hum":86}`; el mismo `GET` desde el Mac vía el puente TCP devolvió `{"temp":33,"hum":80}`. Del lado del mote, cada petición queda registrada como `[INFO: Sensor ] GET / -> {"temp":..,"hum":..}`. En la corrida de la cadena completa el mote atendió 32 `GET` (`a4-sensor-local.log`): dos del chequeo de arranque de `correr-local.sh` y 30 del middleware, uno cada 3 s, cada uno con el mismo JSON que el middleware publicó después. El enrutador `rpl-border-router` del árbol también compila para native en el contenedor (`evidencia/a4-border-router-build.log`, exit 0); la topología con Cooja + Serial Socket + `tunslip6` queda documentada en `a4-azure/router/README.md` pero no se corrió (requiere GUI).

## 4.a ¿Cuál es el resultado presentado por la interfaz middleware?

**Planteamiento.** `middleware/middleware-azure.js` (Node 22, ESM) reemplaza al `middleware-azure.js` del libro: se quita `request` (deprecado) y Express (no participaba del flujo) y se usa `fetch` nativo con timeout. Cada 3 s consulta `SENSOR_URL`, valida que `temp` y `hum` sean números y arma `{"deviceId","temperature","humidity"}` con la propiedad `temperatureAlert = temperature > 30`. El destino se elige con `--destino local|azure` (o `DESTINO`):

- **`local` (la corrida de esta entrega).** Publica con el paquete npm `mqtt` en `MQTT_URL` (por defecto `mqtt://127.0.0.1:1883`), tema `usm/tel329/cloud/contiki-ng-01/telemetry`, QoS 1, y registra el `PUBACK` de cada mensaje con su *packet id* y el tiempo hasta recibirlo. Al cuerpo se agrega `timestamp` (ISO 8601, hora de publicación), que el suscriptor usa para medir la latencia. **Se usa MQTT 5 y `temperatureAlert` va como *user property*** (OASIS MQTT 5.0 §3.3.2.3.7): en IoT Hub la alerta es una propiedad de aplicación que viaja fuera del cuerpo, y MQTT 3.1.1 no tiene dónde ponerla salvo dentro del JSON; MQTT 5 sí, y Mosquitto 2.0 lo habla, así que el cuerpo queda como el de Azure y la alerta en el mismo lugar lógico.
- **`azure` (variante, por defecto en el código).** El SDK `azure-iot-device` + `azure-iot-device-mqtt` con su API de promesas (`Client.fromConnectionString(cs, Mqtt)`, `await client.open()`, `await client.sendEvent(message)`), la alerta como `message.properties`, la cadena de conexión sólo por entorno y el modo `--dry-run`, sin cambios respecto de la versión anterior.

```js
const client = await mqtt.connectAsync(mqttUrl, { protocolVersion: 5, clientId: `middleware-${deviceId}`, clean: true });
// por cada lectura del sensor:
const data = JSON.stringify({ deviceId, temperature, humidity, timestamp: new Date().toISOString() });
const t0 = performance.now();
const packet = await client.publishAsync(topic, data, {
  qos: 1,
  properties: { contentType: 'application/json', userProperties: { temperatureAlert: alert } },
});
console.log(`[mqtt] PUBLISH mid=${packet.messageId} ${data} temperatureAlert=${alert} -> PUBACK en ${(performance.now() - t0).toFixed(2)} ms`);
```

**Evidencia** (`evidencia/a4-middleware-local.log`, `evidencia/a4-mosquitto.log`). El middleware abrió la sesión con `Conectado a mqtt://127.0.0.1:1883 (MQTT 5) como middleware-contiki-ng-01; tema usm/tel329/cloud/contiki-ng-01/telemetry; consultando http://127.0.0.1:8080/ cada 3000 ms` y en 87 s hizo 30 lecturas del sensor y 30 `PUBLISH` QoS 1 (*packet id* 59099 a 59128), los 30 con su `PUBACK`. Cada lectura deja un par de líneas como:

```
[sensor] http://127.0.0.1:8080/ -> {"temp":32,"hum":85}
[mqtt] PUBLISH mid=59100 {"deviceId":"contiki-ng-01","temperature":32,"humidity":85,"timestamp":"2026-11-15T17:26:38.164Z"} temperatureAlert=true -> PUBACK en 1.58 ms
```

12 lecturas superaron los 30 °C y salieron con `temperatureAlert=true`; las otras 18, con `false`. Del otro lado, Mosquitto registra la misma secuencia mensaje por mensaje: `New client connected ... as middleware-contiki-ng-01 (p5, c1, k60)` (protocolo 5), y por cada dato `Received PUBLISH from middleware-contiki-ng-01 (d0, q1, r0, m59099, 'usm/tel329/cloud/contiki-ng-01/telemetry', ... (98 bytes))`, `Sending PUBLISH to cloud-subscriber-...`, `Sending PUBACK to middleware-contiki-ng-01 (m59099, rc0)` y `Received PUBACK from cloud-subscriber-...`: 30 de cada uno. El tiempo `PUBLISH → PUBACK` medido por el middleware fue de 0,73 a 11,07 ms, mediana 1,26 ms (tabla en 4.c).

## 4.c ¿Cuál es el resultado presentado por el suscriptor de Azure IoT Hub?

**Planteamiento.** `subscriber/cloud-subscriber.js` cumple el rol de `azure-sensor-subscriber.js` con el broker local: se conecta con `mqtt` (MQTT 5) a `MQTT_URL`, se suscribe con QoS 1 a `usm/tel329/cloud/+/telemetry` (el `+` toma cualquier `deviceId`, como el suscriptor de IoT Hub recibe de todos los dispositivos del hub) e imprime, por cada mensaje, la hora de llegada, el tema, el `deviceId` sacado del tema, el cuerpo JSON, las *user properties* (`temperatureAlert`) y la latencia middleware → suscriptor, que es la hora de llegada menos el `timestamp` del cuerpo. Middleware y suscriptor corren en el mismo Mac, así que los dos lados leen el mismo reloj y no hay desfase que corregir; la resolución es de 1 ms porque `timestamp` va en ISO 8601 con milisegundos. Al salir imprime n, mínimo, mediana, media y máximo.

```js
await client.subscribeAsync('usm/tel329/cloud/+/telemetry', { qos: 1 });
client.on('message', (topic, payload, packet) => {
  const llegada = Date.now();
  const body = JSON.parse(payload.toString());
  latencias.push(llegada - Date.parse(body.timestamp));
  console.log(`Message received ${new Date(llegada).toISOString()} (topic ${topic}, deviceId=${topic.split('/')[3]}, latencia ${llegada - Date.parse(body.timestamp)} ms):`);
  console.log(payload.toString());
  console.log(`properties: ${JSON.stringify(packet.properties?.userProperties)}`);
});
```

La variante Azure, `subscriber/azure-sensor-subscriber.js`, reemplaza el paquete archivado `azure-event-hubs` por `@azure/event-hubs` 6: un `EventHubConsumerClient` con el consumer group `$Default` y la cadena "Event Hub-compatible endpoint" se suscribe a todas las particiones del endpoint integrado `messages/events` desde `latestEventPosition` (el `startAfterTime: Date.now()` del libro). Sin la cadena sale con código 1 y un mensaje que dice cuál falta (`evidencia/a4-subscriber-sinvar.log`).

**Evidencia** (`evidencia/a4-subscriber-local.log`). El suscriptor arrancó con `Suscrito a usm/tel329/cloud/+/telemetry en mqtt://127.0.0.1:1883 (MQTT 5, QoS 1); esperando mensajes...` y recibió los 30 mensajes, en orden y con el mismo cuerpo, byte a byte, que el middleware reporta como publicados. Cada uno se ve así:

```
Message received 2026-11-15T17:26:38.166Z (topic usm/tel329/cloud/contiki-ng-01/telemetry, deviceId=contiki-ng-01, latencia 2 ms):
{"deviceId":"contiki-ng-01","temperature":32,"humidity":85,"timestamp":"2026-11-15T17:26:38.164Z"}
properties: {"temperatureAlert":"true"}
```

Latencias de la corrida (90 s, un mensaje cada 3 s, middleware y suscriptor en el Mac, broker en el contenedor detrás del reenvío de puertos de OrbStack):

| tramo | n | mín | mediana | media | máx |
|---|---|---|---|---|---|
| middleware → suscriptor (llegada − `timestamp`, resolución 1 ms) | 30 | 1 ms | 1 ms | 2,2 ms | 11 ms |
| middleware `PUBLISH` → `PUBACK` (`performance.now()`) | 30 | 0,73 ms | 1,26 ms | 2,00 ms | 11,07 ms |

La mediana de 1 ms dice que entregar un dato a través del broker cuesta lo mismo que la ida y vuelta al broker: Mosquitto reenvía el `PUBLISH` al suscriptor antes de mandar el `PUBACK` al middleware (en ese orden aparecen en `a4-mosquitto.log`), así que las dos columnas miden casi el mismo camino. Las cuatro muestras de 5 ms o más (6 ms en el primer mensaje, 5, 11 y 5 ms después) coinciden una a una con los `PUBACK` lentos del middleware, o sea que son del host (planificador, reenvío de puertos de OrbStack, emulación amd64 de la imagen) y no del protocolo. Contra IoT Hub este tramo cruzaría Internet con TLS y pasaría por el endpoint compatible con Event Hubs, así que estos números son un piso del tramo nube, no una estimación de Azure.

**Publicación.** El enunciado pide publicar la adaptación en GitHub o equivalente. El código está publicado en el repo público `dreamxist/tel329`, carpeta `tarea4/a4-azure/`: [github.com/dreamxist/tel329/tree/main/tarea4/a4-azure](https://github.com/dreamxist/tel329/tree/main/tarea4/a4-azure).
