# Tarea 4 · Actividad 1 — RPL Lite + CoAP en Cooja (y la mitad CoAP de la Actividad 3)

Una red WSN simulada en Cooja: **un nodo raíz** que es a la vez raíz del DODAG
(RPL Lite) y **servidor CoAP** con un solo recurso, `sensores/temp`, y **cinco
clientes** que se unen al DODAG y notifican ese recurso cada 10 s con una
temperatura aleatoria (`random_rand()`), por **POST confirmable**. Dos de los
cinco clientes están a **dos saltos** de la raíz, para que RPL tenga algo que
hacer.

```
a1-coap/
├── root-coap.c            raíz del DODAG + servidor CoAP (nodo 1)
├── cliente-coap.c         cliente CoAP que sensa y notifica (nodos 2..6)
├── resources/res-temp.c   el recurso sensores/temp: POST/PUT registra, GET devuelve la última
├── project-conf.h         niveles de log y COAP_MAX_OPEN_TRANSACTIONS
├── Makefile               CSMA + IPv6 + RPL Lite + módulo coap
├── a1-coap.csc            la simulación para la GUI (con RadioLogger → pcap)
├── js/volcar-log.js       script headless: vuelca el Mote output y escribe el pcap
├── correr.sh              compila + corre headless + genera CSV y resumen (en el contenedor)
├── tiempos.py             del log al CSV de tiempos y al resumen por cliente
└── respuesta.md           la respuesta a la actividad 1 y la tabla/análisis de la 3
```

La evidencia queda en [`../evidencia/`](../evidencia/): `a1-coap.log`,
`a1-coap.pcap`, `a1-tiempos-coap.csv` y `a1-resumen-coap.md`.

## Diseño

**Topología** (UDGM, alcance 50 m, interferencia 100 m):

```
          n3 (0,40)          n6 (0,80)
            |                  |      n6 solo alcanza a n3 → 2 saltos
 n4 ---- [n1 raíz] ---- n2 ---- n5
(-40,0)   (0,0)      (40,0)  (80,0)   n5 solo alcanza a n2 → 2 saltos
```

n2, n3 y n4 están a 40 m de la raíz (1 salto). n5 y n6 están a 80 m, fuera de
alcance de la raíz, y a 40 m de n2 y n3 respectivamente: su único camino es por
el padre RPL.

**Por qué POST desde el cliente y no Observe desde la raíz.** El enunciado
pide que los clientes *notifiquen* un recurso. Con Observe (RFC 7641) la raíz
tendría que registrarse en cada cliente y los clientes mantener el estado del
observador; cada notificación igual sería un mensaje del cliente a la raíz.
El POST confirmable es la forma más directa de "notificar" en el modelo
petición/respuesta de CoAP (RFC 7252 §5), y como la respuesta viene
*piggybacked* en el ACK (§5.2.1) la ida y vuelta mide el tiempo de respuesta
del servidor sin mensajes extra. Se eligió CON y no NON justamente para tener
esa respuesta y poder medir.

**Lo que hace cada parte:**

- `root-coap.c`: `NETSTACK_ROUTING.root_start()` y
  `coap_activate_resource(&res_temp, "sensores/temp")`. El motor CoAP (proceso
  `coap_engine`, UDP 5683) lo arranca `contiki-main.c` al incluir el módulo
  `coap` en el Makefile; la aplicación solo enlaza el recurso.
- `resources/res-temp.c`: el handler de POST/PUT lee el payload
  `id=<n> seq=<k> temp=<xx.x>`, lee el **TTL** del datagrama IPv6 que todavía
  está en `uip_buf` (`UIP_IP_BUF->ttl`) y de ahí saca los **saltos**:
  `UIP_TTL (64) − ttl + 1`. Registra la línea `REG ...` y responde
  **2.04 Changed** con `ok hops=<n>` en el payload. El GET devuelve la última
  lectura, por si se consulta desde `coap-cli`.
- `cliente-coap.c`: espera `NETSTACK_ROUTING.node_is_reachable()`, toma la
  dirección de la raíz con `get_root_ipaddr()` (nada hardcodeado) y usa la API
  **no bloqueante** `coap_send_request()` (`coap-callback-api.h`) en vez de
  `COAP_BLOCKING_REQUEST`: el callback llega con la respuesta y permite
  imprimir `TX` y `RX` como líneas separadas, a las que Cooja antepone el
  tiempo simulado. Una sola petición en vuelo por cliente: si la anterior aún
  no terminó (CoAP la retransmite hasta `COAP_MAX_RETRANSMIT` = 4 veces), la
  ronda se salta y queda registrada como `SKIP`.
- La temperatura es `150 + random_rand() % 300` décimas de grado, uniforme en
  [15.0, 44.9] °C.

## Compilar

Desde esta carpeta, con la imagen oficial (en el Mac no hay toolchain):

```bash
cd <raíz del repo>
docker run --rm -e CCACHE_DISABLE=1 \
  -v ~/contiki-ng:/home/user/contiki-ng \
  -v "$PWD":/work -w /work/tarea4/a1-coap \
  contiker/contiki-ng make TARGET=cooja CONTIKI=/home/user/contiki-ng
```

Produce `build/cooja/root-coap.cooja` y `build/cooja/cliente-coap.cooja`. Con
un Contiki-NG local alcanza `make TARGET=cooja` (el Makefile resuelve la ruta
por el symlink `contiki-ng` de la raíz del repo, ver README de la raíz).

## Correr

### Headless, de una vez (lo que generó la evidencia)

```bash
cd <raíz del repo>
docker run --rm -e CCACHE_DISABLE=1 -e PCAP_PARA_RESUMEN=1 \
  -v ~/contiki-ng:/home/user/contiki-ng \
  -v ~/.gradle-contiker:/home/user/.gradle \
  -v "$PWD":/work \
  contiker/contiki-ng /work/tarea4/a1-coap/correr.sh
```

`correr.sh` compila, arma un `.csc` headless a partir de `a1-coap.csc`
(inyecta `CONTIKI=` en el comando de compilación de cada tipo de mote y un
`ScriptRunner` con `js/volcar-log.js`), corre `./gradlew run --args="--no-gui
..."` en `tools/cooja` y deja en `../evidencia/`:

| Archivo | Qué es |
|---|---|
| `a1-coap.log` | el Mote output completo, `<tiempo_us> n<id> <mensaje>` |
| `a1-coap.pcap` | todas las tramas 802.15.4 del radio, con el tiempo simulado |
| `a1-tiempos-coap.csv` | una fila por petición: cliente, seq, t_envio_ms, t_respuesta_ms, rtt_ms, saltos, código, mid, bytes, temp |
| `a1-resumen-coap.md` | n, media, mediana, p95, mín, máx por cliente y global |

La corrida se corta sola cuando los cinco clientes tienen 25 respuestas o a
los `SEGUNDOS` simulados (default 600). Variables: `SEGUNDOS`, `SEMILLA`
(default 123456), `SIN_COMPILAR=1`. El volumen `~/.gradle-contiker` es
opcional: evita que cada corrida vuelva a bajar gradle (el wrapper de Cooja
pide 9.6.1 y el host tenía otra versión).

Cosas que el script resuelve y conviene saber si se corre a mano: Cooja
`--no-gui` aborta con `No plugin controlling simulation` si nadie maneja la
simulación (el `ScriptRunner` es ese alguien); el `.csc` generado tiene que
vivir **en esta carpeta** porque `[CONFIG_DIR]` es el directorio del propio
`.csc`; y el límite de `TIMEOUT(...)` del script tiene que ser un entero
literal en ms, Cooja lo saca del texto con una regex.

### En la GUI

Abrir `a1-coap.csc` en Cooja. Trae *Visualizer*, *Log Listener* filtrado por
`A1`, *Timeline* y un **Radio Logger** ya configurado con el analizador
*6LoWPAN Analyzer with PCAP* y salida en `../evidencia/a1-coap-gui.pcap`.
Para reproducir la corrida headless: semilla 123456, `speedlimit` 20.

## El pcap en headless: qué permite Cooja hoy

El plugin `RadioLogger` tiene exportación a pcap (`Analyzer → 6LoWPAN Analyzer
with PCAP`, y en el `.csc` `<analyzers name="6lowpan-pcap" />` +
`<pcap_file>`). Pero **es un `VisPlugin` y Cooja headless ya no lo arranca**:
desde el cambio "Avoid starting the AWT thread in headless mode"
(`tools/cooja/doc/ChangeLog.md`, PR contiki-ng/cooja#261) los plugins que
extienden `VisPlugin` se saltan en `--no-gui`. Además, aun en la GUI, la
escritura al pcap ocurre en `analyzePacket()`, que se llama cuando la tabla
renderiza la fila, no cuando llega el paquete.

La salida fue hacer lo mismo que `PcapExporter.java` desde el script del
`ScriptRunner`, que corre en Nashorn con interop Java completo:
`js/volcar-log.js` se engancha a
`sim.getRadioMedium().getRadioTransmissionTriggers()` con un `BiConsumer`, y en
cada transmisión terminada toma
`getLastConnection().getSource().getLastPacketTransmitted().getPacketData()` y
escribe el registro pcap con el mismo encabezado que Cooja (magic
`a1b2c3d4`, linktype **195** = `LINKTYPE_IEEE802_15_4`, timestamp = inicio de
la transmisión en tiempo simulado). Resultado: el pcap sale headless, con
exactamente las mismas tramas que vería el Radio Logger.

Para abrirlo en Wireshark: las tramas de Cooja **no traen FCS**, así que hay
que decirle `Preferences → Protocols → IEEE 802.15.4 → FCS format: None` (o
`-o wpan.fcs_format:0` en `tshark`). La fragmentación 6LoWPAN y el CoAP se
decodifican solos.

## Qué se ve

En el Mote output (filtro `A1`), en orden:

1. La raíz anuncia que es raíz y servidor, y su dirección global.
2. Cada cliente dice `todavia sin ruta a la raiz` hasta que RPL le da padre;
   RPL en INFO deja ver los DIO/DAO.
3. Después, por cada ronda, tres líneas que son la medición:
   `TX id=2 seq=1 mid=... len=22 'id=2 seq=1 temp=27.3'` en el cliente,
   `REG id=2 seq=1 temp=27.3 hops=1 ttl=64 mid=...` en la raíz y
   `RX id=2 seq=1 code=2.04 mid=... rtt_ms=... 'ok hops=1'` en el cliente.
   Los de dos saltos muestran `hops=2 ttl=63`.
4. Cada 10 envíos, un `resumen id=<n> enviadas= ok= timeout= saltadas=`.

## Resultados medidos

Corrida headless, semilla 123456, cortada por el script cuando los cinco
clientes llegaron a 25 respuestas: **282,4 s simulados**. La raíz creó el DAG
a los 0,38 s; los clientes a un salto tuvieron ruta a los 20–25 s y los de dos
saltos a los 37–42 s (RPL Lite necesita que n2/n3 tengan padre antes de
anunciarse a n5/n6).

**129 POST confirmables, 129 respuestas 2.04 Changed, 0 timeouts, 0 rondas
saltadas, 0 retransmisiones CoAP** (no hay ningún `Retransmitting` en el log;
el medio UDGM con `success_ratio` 1.0 no pierde tramas y las colisiones las
resuelve CSMA en la capa de enlace).

| cliente | saltos | n | media ms | mediana ms | p95 ms | mín ms | máx ms |
|---|---|---|---|---|---|---|---|
| n2 | 1 | 26 | 51,1 | 47,3 | 81,8 | 13,5 | 87,8 |
| n3 | 1 | 26 | 43,0 | 47,3 | 60,8 | 17,8 | 64,8 |
| n4 | 1 | 27 | 51,0 | 46,8 | 89,8 | 17,8 | 90,8 |
| n5 | 2 | 25 | 94,2 | 95,4 | 137,4 | 44,4 | 138,4 |
| n6 | 2 | 25 | 99,8 | 102,4 | 139,4 | 55,4 | 141,4 |
| **global** | 1–2 | 129 | 67,2 | 61,8 | 121,4 | 13,5 | 141,4 |

El RTT es la diferencia entre el tiempo simulado de la línea `RX` y el de la
línea `TX` del mismo (cliente, seq); coincide con el `rtt_ms` que calcula el
mote con `clock_time()` (diferencia máxima 0,6 ms, por el redondeo a ms del
reloj del mote). Descompuesto con la línea `REG` de la raíz, a un salto la
subida mediana es 14–33 ms y la bajada 19–29 ms, cada una entre 5,6 y 45,6 ms;
a dos saltos, 45–52 ms de subida y 47–48 ms de bajada.

**El pcap** tiene 820 tramas 802.15.4: 428 de datos y 392 ACK de enlace (5
bytes). Las POST ocupan 81–90 bytes de trama (payload CoAP de 20–21 bytes,
cabecera CoAP de 4 bytes + token/opciones Uri-Path y Content-Format, UDP e
IPv6 comprimidos por 6LoWPAN IPHC, y 8 bytes más cuando la trama la reenvía un
padre y la dirección origen ya no se puede elidir de la MAC); las respuestas
2.04 piggybacked ocupan 56–64 bytes; el resto son DIO (73 bytes), DIS y DAO
de RPL. No hay fragmentación 6LoWPAN: todo cabe en una trama de 127 bytes.

El análisis completo está en [respuesta.md](respuesta.md).
