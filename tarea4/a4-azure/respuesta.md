# Actividad 4 - Respuestas (borrador)

Código y evidencia en `tarea4/a4-azure/` y `tarea4/evidencia/a4-*`. La cadena completa sensor → enrutador →
middleware quedó verificada localmente dentro del contenedor `contiker/contiki-ng`; el tramo contra Azure IoT Hub
está implementado y listo, pero su corrida depende de las cadenas de conexión del portal, que se marcan como
`[FALTA: cadena de conexión de Azure]`.

## 4.b ¿Cuál es el resultado presentado por el sensor o mote?

**Planteamiento.** El sensor es un mote Contiki-NG que corre un servidor HTTP y responde a cualquier `GET` con un
JSON `{"temp":T,"hum":H}`, con `T = 15 + random_rand() % 25` y `H = 80 + random_rand() % 10`, igual que el
`websense-Cloud` de Kurniawan. La diferencia es cómo se consigue el servidor: Contiki-NG ya no trae `websense` ni
`httpd-simple` como módulo reutilizable (sólo queda una copia privada dentro de `examples/rpl-border-router/webserver`),
pero sí trae `os/net/app-layer/httpd-ws`, su descendiente directo: el proyecto define `httpd_ws_get_script()` y
devuelve un protothread que escribe con `SEND_STRING`, exactamente el patrón de `generate_routes()` del enunciado.
Por eso se eligió HTTP sobre CoAP: el enunciado habla de servidor web, el middleware original hace `GET`, y así el
tramo Node queda con `fetch` nativo sin librerías CoAP. Hubo que habilitar TCP (`UIP_CONF_TCP 1`), fijar MSS y
ventana TCP en 128 B y alinear los buffers de `httpd-ws` (sin eso no compila: la cabecera exige buffer >= ventana y
GCC moderno dispara `-Werror=format-truncation` en `httpd-ws.c:255`), y forzar `Content-Type: application/json`.
El mote se compiló en el contenedor para `TARGET=native`, donde la plataforma crea el `tun0` (`fd00::1/64`) y el nodo
queda en `fd00::302:304:506:708`.

**Evidencia** (`evidencia/a4-sensor.log`, `evidencia/a4-curl.txt`). Al arrancar, el mote reporta `Routing: RPL Lite`,
`Net: tun6`, link-layer `0102.0304.0506.0708` y `tun0` con `fd00::1/64`. Desde el contenedor, `ping6 -c 3
fd00::302:304:506:708` responde los tres paquetes (0 % de pérdida, RTT 0,07 a 0,77 ms) y tres `GET
http://[fd00::302:304:506:708]/` devuelven `HTTP 200 OK`, `Server: Contiki`, `Content-Type: application/json` con
cuerpos `{"temp":24,"hum":80}`, `{"temp":24,"hum":81}` y `{"temp":16,"hum":86}`; el mismo `GET` desde el Mac vía el
puente TCP devolvió `{"temp":33,"hum":80}`. Del lado del mote, cada petición queda registrada como
`[INFO: Sensor ] GET / -> {"temp":..,"hum":..}`. El enrutador `rpl-border-router` del árbol también compila para
native en el contenedor (`evidencia/a4-border-router-build.log`, exit 0); la topología con Cooja + Serial Socket +
`tunslip6` queda documentada en `router/README.md` pero no se corrió (requiere GUI).

## 4.a ¿Cuál es el resultado presentado por la interfaz middleware?

**Planteamiento.** `middleware/middleware-azure.js` (Node 22, ESM) reemplaza al `middleware-azure.js` del libro: se
quita `request` (deprecado) y Express (no participaba del flujo), se usa `fetch` nativo con timeout, y el SDK
`azure-iot-device` + `azure-iot-device-mqtt` actual con su API de promesas (`Client.fromConnectionString(cs, Mqtt)`,
`await client.open()`, `await client.sendEvent(message)`). Cada 3 s consulta `SENSOR_URL`, valida que `temp` y `hum`
sean números, arma `{"deviceId","temperature","humidity"}` (el `deviceId` se toma de la propia cadena de conexión),
agrega la propiedad de aplicación `temperatureAlert = temperature > 30` y lo envía al IoT Hub por MQTT. La cadena de
conexión y la URL del sensor vienen sólo de variables de entorno (`.env.example` versionado, `.env` no). El modo
`--dry-run` hace todo menos conectar a Azure e imprime el mensaje que mandaría, lo que permite probar sensor →
middleware sin credenciales.

**Evidencia local** (`evidencia/a4-middleware-dryrun.log`). Corrido contra el sensor real durante 11 s:
`Modo --dry-run ... deviceId=contiki-ng-01 sensor=http://127.0.0.1:8080/ cada 3000 ms`, seguido de cuatro pares de
líneas `[sensor] ... -> {"temp":26,"hum":89}` / `[dry-run] enviaría a IoT Hub:
{"deviceId":"contiki-ng-01","temperature":26,"humidity":89} properties={temperatureAlert:false}`; en la tercera
lectura (`temp=35`) la propiedad pasa a `temperatureAlert:true`, lo que confirma el umbral.

**Evidencia contra Azure.** `[FALTA: cadena de conexión de Azure]`. Cuando esté, correr desde
`tarea4/a4-azure/middleware`:

```bash
AZURE_DEVICE_CONNECTION_STRING='HostName=<hub>.azure-devices.net;DeviceId=contiki-ng-01;SharedAccessKey=<clave>' \
SENSOR_URL=http://127.0.0.1:8080/ node middleware-azure.js | tee ../../evidencia/a4-middleware-azure.log
```

y pegar aquí la salida esperada: `Conectado a IoT Hub por MQTT como contiki-ng-01 ...` y luego una línea
`[iothub] enviado: {...} temperatureAlert=...` por cada lectura, más el gráfico *Device to cloud messages* del portal.

## 4.c ¿Cuál es el resultado presentado por el suscriptor de Azure IoT Hub?

**Planteamiento.** `subscriber/azure-sensor-subscriber.js` reemplaza el paquete archivado `azure-event-hubs` por
`@azure/event-hubs` 6. El IoT Hub expone la telemetría en su endpoint integrado `messages/events`, compatible con
Event Hubs (AMQP 5671), así que un `EventHubConsumerClient` con el consumer group `$Default` y la cadena "Event
Hub-compatible endpoint" se suscribe a todas las particiones desde `latestEventPosition` (equivalente al
`startAfterTime: Date.now()` del libro) e imprime, por cada evento, la partición, el `iothub-connection-device-id`
de las propiedades de sistema, el cuerpo JSON y las propiedades de aplicación (`temperatureAlert`). La cadena viene
sólo de `EVENTHUB_COMPATIBLE_CONNECTION_STRING`; si falta o si se pega por error la cadena del dispositivo
(`HostName=...`), el programa termina con un mensaje que indica qué cadena corresponde.

**Evidencia local** (`evidencia/a4-subscriber-sinvar.log`). Sin la variable, el programa arranca y sale con código 1
y el mensaje `Falta EVENTHUB_COMPATIBLE_CONNECTION_STRING (IoT Hub > Built-in endpoints > Event Hub compatible
endpoint)`; con una cadena de tipo dispositivo responde `La cadena no parece la del endpoint compatible con Event
Hubs ...`.

**Evidencia contra Azure.** `[FALTA: cadena de conexión de Azure]`. Cuando esté, correr desde
`tarea4/a4-azure/subscriber`, con el middleware enviando en otra terminal:

```bash
EVENTHUB_COMPATIBLE_CONNECTION_STRING='Endpoint=sb://<ns>.servicebus.windows.net/;SharedAccessKeyName=service;SharedAccessKey=<clave>;EntityPath=<hub>' \
node azure-sensor-subscriber.js | tee ../../evidencia/a4-subscriber-azure.log
```

y pegar aquí la salida esperada: `Suscrito al IoT Hub (consumer group $Default); esperando mensajes...` y luego, cada
3 s, `Message received (partition N, deviceId=contiki-ng-01):` con `{"deviceId":"contiki-ng-01","temperature":T,
"humidity":H}` y `properties: {"temperatureAlert":"true|false"}`, que debe coincidir uno a uno con lo que el
middleware reporta como enviado.
