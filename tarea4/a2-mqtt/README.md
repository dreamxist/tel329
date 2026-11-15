# A2 · RPL Lite + MQTT (y la mitad MQTT de la A3)

Una red de 6 motes Cooja: el mote 1 es el `rpl-border-router` de Contiki-NG tal
cual, y los motes 2 a 6 corren `mqtt-cliente.c`, que publica cada 5 s una
temperatura aleatoria en el tema `usm/tel329/temp` con QoS 1. El broker es
Mosquitto, en el mismo contenedor, del otro lado de `tunslip6`:

```
mote 2..6 (mqtt-cliente)  ──RPL Lite / 6LoWPAN──▶  mote 1 (border router)
                                                        │ serial (SerialSocketServer :60001)
                                                   tunslip6  ──▶ tun0 = fd00::1/64
                                                        │
                                                   mosquitto :1883   ◀── tcpdump
```

**Plan A, en Cooja, es el que quedó.** El plan B (clientes `TARGET=native`,
sin RPL) no hizo falta; está descrito al final por si el entorno cambia.

```
a2-mqtt/
├── mqtt-cliente.c     el cliente, adaptado de examples/mqtt-client
├── project-conf.h     TCP, MQTT 3.1.1, broker, tema, intervalo, logs
├── Makefile           modulo mqtt + CSMA + IPv6 + RPL Lite; targets cooja y native
├── mosquitto.conf     listener 1883 (v4+v6), anonimo, log a stderr con timestamp
├── a2-mqtt.csc        la simulacion para la GUI (6 motes + SerialSocketServer :60001)
├── js/volcar-log.js   ScriptRunner headless: Mote output -> COOJA.testlog con tiempo simulado
├── correr.sh          orquesta TODO dentro del contenedor y deja la evidencia
├── analizar.py        log -> a2-tiempos-mqtt.csv + a2-resumen-mqtt.md
├── Dockerfile         opcional: la imagen oficial + tcpdump preinstalado
├── respuesta.md       la respuesta para el notebook (A2 + tabla y analisis de A3)
└── README.md          este archivo
```

El border router no se copia: el `.csc` apunta a
`[CONTIKI_DIR]/examples/rpl-border-router/border-router.c` y lo compila con su
propio Makefile (`$(MAKE) border-router.cooja TARGET=cooja`).

## Compilar

Desde esta carpeta, con el symlink `contiki-ng` de la raíz del repo hecho (ver
README de la raíz):

```bash
make TARGET=cooja
```

En el contenedor, pasando `CONTIKI` y desactivando ccache:

```bash
docker run --rm -e CCACHE_DISABLE=1 \
  -v ~/contiki-ng:/home/user/contiki-ng \
  -v "$PWD":/work -w /work/tarea4/a2-mqtt \
  contiker/contiki-ng make TARGET=cooja CONTIKI=/home/user/contiki-ng
```

## Correr todo de una vez (headless, el camino que se usó)

Desde la **raíz del worktree**:

```bash
docker run --rm --privileged -e CCACHE_DISABLE=1 \
  -v ~/contiki-ng:/home/user/contiki-ng \
  -v tel329-gradle:/home/user/.gradle \
  -v "$PWD":/work -w /work/tarea4/a2-mqtt \
  contiker/contiki-ng ./correr.sh 300
```

- `--privileged`: tunslip6 necesita `/dev/net/tun`. Probado en OrbStack: el
  `/dev/net/tun` está, `sudo` sin clave funciona y la tun se crea.
- `tel329-gradle`: volumen con la caché de Gradle, para no bajar Cooja de nuevo
  en cada `docker run --rm`. La primera vez tarda varios minutos.
- `300`: segundos **simulados**. La simulación corre con `speedlimit 1.0` (tiempo
  real) porque mosquitto y tunslip6 viven en el reloj del host; con 6 motes la
  imagen amd64 emulada en el Mac sí alcanza el tiempo real.
- `tcpdump` no viene en `contiker/contiki-ng`: el script lo instala con `apt`
  (necesita red) o se usa la imagen del `Dockerfile` (`docker build -t tel329-a2 .`).

Qué hace `correr.sh`, en orden: arranca `mosquitto -v`; genera un `.csc` headless
junto a los fuentes (igual que en la Tarea 3: `[CONFIG_DIR]` es el directorio del
`.csc`, `CONTIKI` va explícito porque Cooja compila sin `$HOME`, y el
`ScriptRunner` se inyecta con `js/volcar-log.js`); lanza Cooja `--no-gui`; espera
con `netstat` a que el `SerialSocketServer` del mote 1 abra `:60001` (no con una
conexión de prueba: el plugin acepta un solo cliente y una sonda puede dejar a
tunslip6 afuera); lanza `tunslip6 -a 127.0.0.1 -p 60001 fd00::1/64`; espera
`tun0`; arranca `tcpdump -i tun0 port 1883`; espera a que Cooja termine por el
`TIMEOUT` del script; apaga todo y corre `analizar.py`.

Deja en `../evidencia/`:

| archivo | qué es |
|---|---|
| `a2-mqtt.log` | Mote output completo, `<us simulados> n<id> <msg>` |
| `a2-mosquitto.log` | el broker en `-v`: CONNECT/CONNACK, cada PUBLISH y PUBACK |
| `a2-tunslip6.log` | tunslip6 + la consola del border router (sale por el mismo serial) |
| `a2-mqtt.pcap` / `a2-mqtt-pcap.txt` | la captura en tun0 y su volcado `tcpdump -tt -nn -v` |
| `a2-tiempos-mqtt.csv` | una fila por PUBLISH: cliente, seq, mid, t_publish, t_puback, rtt, saltos |
| `a2-resumen-mqtt.md` | n, media, mediana, p95, mín, máx por cliente, por saltos y global |

## Resultados (corrida de 300 s, semilla 123456)

273 PUBLISH con QoS 1 y 273 PUBACK, entre 51 y 58 por cliente, 0 desconexiones,
0 PINGREQ (el keep alive nunca vence porque se publica cada 5 s). RTT medido en
el mote entre `mqtt_publish()` y `MQTT_EVENT_PUBACK`, en ms de reloj simulado:

| cliente | saltos | n | media (ms) | mediana (ms) | p95 (ms) | min (ms) | max (ms) | sin PUBACK |
|---|---|---|---|---|---|---|---|---|
| 2 | 1 | 56 | 169.1 | 59.8 | 95.1 | 19.3 | 6238.3 | 0 |
| 3 | 1 | 58 | 61.5 | 61.8 | 98.8 | 23.3 | 133.3 | 0 |
| 4 | 2 | 53 | 152.1 | 136.6 | 237.4 | 67.6 | 277.6 | 0 |
| 5 | 2 | 55 | 184.1 | 179.0 | 249.9 | 85.6 | 284.6 | 0 |
| 6 | 3 | 51 | 213.2 | 211.8 | 290.8 | 132.8 | 381.8 | 0 |
| **global** | | 273 | 154.2 | 130.6 | 243.7 | 19.3 | 6238.3 | 0 |

El único RTT fuera de escala (6.238 ms, primera publicación del mote 2) es una
retransmisión TCP: el segmento con el PUBACK llegó al mote con `tcp: bad
checksum` y lo repuso el RTO del broker, inflado por un handshake lento. El
detalle y la comparación con CoAP están en [respuesta.md](respuesta.md); las
tablas completas en `../evidencia/a2-resumen-mqtt.md` y el CSV fila por fila en
`../evidencia/a2-tiempos-mqtt.csv`.

## Correr con la GUI

Abrir `a2-mqtt.csc` en Cooja (en Linux con Java; en el Mac no hay). Al cargar
compila los dos tipos de mote. El plugin *Serial Socket (SERVER)* del mote 1 ya
viene en el puerto 60001 y arranca solo; después, en otra terminal:

```bash
mosquitto -c mosquitto.conf -v
sudo ~/contiki-ng/tools/serial-io/tunslip6 -a 127.0.0.1 -p 60001 fd00::1/64
mosquitto_sub -h ::1 -t 'usm/tel329/temp' -v
```

El *Log Listener* viene filtrado por `A2,`: se ven `START`, `CONNECTED`, los
`PUB` y los `PUBACK` de cada cliente.

## Qué se ve

En el Mote output cada cliente deja dos líneas por publicación, con el reloj del
mote en ms:

```
A2,PUB,<id>,<seq>,<mid>,<t_ms>,<rank_dag>,<padre>
A2,PUBACK,<id>,<mid>,<t_ms>
```

`<padre>` es el último byte de la dirección del padre RPL preferido, que en Cooja
es el ID del mote padre: de ahí `analizar.py` saca los saltos (padre 1 = raíz = 1
salto). El pcap lo confirma por otro lado: el *hop limit* con que cada paquete
llega a tun0 es `64 - saltos`.

En `a2-mosquitto.log` se ve cada `Received PUBLISH from tel329-a2-mote<N> (d0, q1,
r0, m<mid>, 'usm/tel329/temp', ... (38 bytes))` seguido de `Sending PUBACK`.

Topología (UDGM, alcance 50 m): 2 y 3 a un salto de la raíz; 4 y 5 a dos (vía 2 y
3); 6 a tres (vía 4). Con la semilla 123456 RPL eligió exactamente esos padres.

## Decisiones que conviene saber

- **QoS 1 y no 0**: el PUBACK es lo que permite medir ida y vuelta desde el mote.
  Con QoS 0 solo se mediría publish → llegada al broker, con dos relojes distintos.
- **MQTT 3.1.1** (`MQTT_CONF_VERSION`): Contiki-NG usa 3.1 por defecto; el
  enunciado cita el estándar OASIS 3.1.1.
- **Segmento TCP de 64 B** (`A2_MAX_TCP_SEGMENT`, el ejemplo usa 32): un PUBLISH
  completo pesa 59-61 B y así va en un solo segmento y en una sola trama
  802.15.4 sin fragmentar en 6LoWPAN. Con 32 cada publicación serían dos
  segmentos y dos idas y vueltas.
- **Keep alive 15 s** (3 × intervalo, como el ejemplo). Como se publica cada 5 s
  nunca llega a vencer: no hay PINGREQ en la captura, y eso también es un dato.
- **Sin suscripción**: el cliente solo publica; el `mosquitto_sub` de la GUI es
  para mirar.
- **`log_dest stderr`** en mosquitto: stdout va con buffer cuando se redirige.
- **`speedlimit 1.0`** también desde el script (`sim.setSpeedLimit`), no solo en
  el `.csc`.

## Plan B (no usado): clientes nativos

Si Cooja headless no pudiera abrir el socket serial o la tun no se pudiera crear,
el mismo `mqtt-cliente.c` compila con `make TARGET=native` y corre como proceso
Linux. La plataforma native (`arch/cpu/native/net/tun6-net.c`) crea su propia
interfaz tun (`-t <dev>`, por defecto `tun0`) con el prefijo `--prefix`
(`fd00::1/64` por defecto, o sea el host queda como `fd00::1`) y asume un border
router en `PREFIX::1`, así que el proceso hablaría directo con mosquitto. Para 5
clientes habría que correr cinco procesos con `-t tun1 ... -t tun5`, y además
fijar a cada uno una dirección MAC distinta (`PLATFORM_CONF_MAC_ADDR` en tiempo
de compilación: la plataforma native usa `01:02:03:04:05:06:07:08` fijo, de ahí
que todos salgan como `fd00::302:304:506:708` y `Node ID: 1800`), o se pisarían
entre sí. Y lo central: **RPL no interviene**. No hay radio, no hay 6LoWPAN ni
DODAG; cada cliente está a un salto IP del broker por una tun local. Serviría para
mostrar MQTT, no la integración RPL + MQTT que pide la actividad. Por eso se
insistió en el plan A, que funcionó.
