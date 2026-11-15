# Solución — Tarea 4: Mensajería CoAP y MQTT

**Asignatura:** TEL329 Redes de Sensores · 2026  
**Autor:** Francisco Zúñiga  
**Repositorio:** `tel329`, rama `tarea4`, carpeta `tarea4/` (código en `a1-coap/`, `a2-mqtt/`, `a4-azure/`; evidencia en `evidencia/`)  
**Entorno:** Contiki-NG (develop) en la imagen `contiker/contiki-ng` sobre OrbStack; Cooja headless con semilla 123456; Mosquitto 2.0.11; Node 22  
**Entrega:** jueves 19-nov-2026

Informe autocontenido: las cuatro actividades con sus números, el código clave en bloques y la lista de archivos de evidencia con su ruta. El código completo está en el repo. Lo pendiente está marcado **[FALTA]** y detallado en `Pendientes_Tarea4.md`.

---

## Actividad 1 — Red WSN con RPL Lite y CoAP

### Qué se construyó

Una red de seis Cooja motes sobre la pila de Contiki-NG que vimos en la Lección 8 (aplicación CoAP sobre UDP/uIP, IPv6 con RPL, adaptación 6LoWPAN y MAC CSMA sobre IEEE 802.15.4). El nodo 1 arranca como raíz del DODAG con `NETSTACK_ROUTING.root_start()` y es además el servidor CoAP: activa un único recurso, `sensores/temp`, cuyo handler de POST/PUT registra lo que llega y responde `2.04 Changed`. Los nodos 2 a 6 son los clientes: se unen al DODAG, esperan a que `NETSTACK_ROUTING.node_is_reachable()` sea verdadero, toman la dirección de la raíz con `get_root_ipaddr()` y cada 10 segundos envían un POST **confirmable** a `coap://[raíz]:5683/sensores/temp` con el texto `id=<nodo> seq=<k> temp=<xx.x>`, donde la temperatura sale de `random_rand()` (`150 + random_rand() % 300` décimas de grado, es decir uniforme en 15,0–44,9 °C). La topología (UDGM, alcance 50 m) deja a n2, n3 y n4 a 40 m de la raíz y a n5 y n6 a 80 m, de modo que estos dos solo llegan a través de n2 y n3: la red tiene clientes a uno y a dos saltos y RPL es el que arma el camino.

```
          n3 (0,40)          n6 (0,80)
            |                  |      n6 solo alcanza a n3 → 2 saltos
 n4 ---- [n1 raíz] ---- n2 ---- n5
(-40,0)   (0,0)      (40,0)  (80,0)   n5 solo alcanza a n2 → 2 saltos
```

El código adaptado está en `tarea4/a1-coap/` (`root-coap.c`, `cliente-coap.c`, `resources/res-temp.c`, `Makefile`, `project-conf.h`) y parte de los ejemplos `examples/coap/coap-example-server` y `coap-example-client` de Contiki-NG, con tres cambios de fondo. Primero, el servidor del ejemplo no era raíz RPL ni el cliente conocía al servidor más que por una dirección fija (`SERVER_EP "coap://[fe80::202:2:2:2]"`); acá el endpoint es la raíz que RPL ya conoce, así que no hay direcciones hardcodeadas y el mismo binario sirve para los cinco clientes. Segundo, en vez de `COAP_BLOCKING_REQUEST`, que deja al protothread clavado hasta la respuesta, se usa la API no bloqueante `coap_send_request()` de `coap-callback-api.h`: el callback recibe el estado `COAP_REQUEST_STATUS_RESPONSE` con el mensaje (o `TIMEOUT` si se agotaron las retransmisiones), y eso permite imprimir el envío y la respuesta como líneas separadas, que Cooja marca con el tiempo simulado. Tercero, el recurso lee el hop limit del datagrama IPv6 que todavía está en `uip_buf` cuando corre el handler (`UIP_IP_BUF->ttl`) y de ahí deduce los saltos (`64 − ttl + 1`, porque uIP sale con `UIP_TTL = 64` y cada reenvío descuenta uno); lo devuelve en el payload de la respuesta para que el cliente también lo registre.

El corazón del cliente (`cliente-coap.c`):

```c
coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
coap_set_header_uri_path(request, URI_TEMP);          /* "sensores/temp" */
coap_set_header_content_format(request, TEXT_PLAIN);
coap_set_payload(request, (uint8_t *)payload, strlen(payload));

t_envio = clock_time();
if(coap_send_request(&req_state, &server_ep, request, respuesta_cb)) {
  pendiente = 1;
  LOG_INFO("TX id=%u seq=%" PRIu32 " mid=%u len=%u '%s'\n",
           node_id, seq, request->mid, (unsigned)strlen(payload), payload);
}
```

y el handler del recurso en la raíz (`resources/res-temp.c`):

```c
uint8_t ttl = UIP_IP_BUF->ttl;
unsigned saltos = (unsigned)(UIP_TTL - ttl) + 1;
...
LOG_INFO("REG %s hops=%u ttl=%u mid=%u total=%" PRIu32 "\n",
         ultima, saltos, ttl, request->mid, total_recibidas);
snprintf((char *)buffer, preferred_size, "ok hops=%u", saltos);
coap_set_payload(response, buffer, strlen((char *)buffer));
coap_set_status_code(response, CHANGED_2_04);
```

Se eligió que el cliente notifique con POST y no que la raíz observe (RFC 7641) porque el enunciado pide que los clientes *notifiquen*, y en el modelo petición/respuesta de CoAP (RFC 7252 §5; Lección 10, "Modelo de Petición/Respuesta") la forma directa de empujar un dato al servidor es una petición POST o PUT. Se eligió confirmable y no non-confirmable porque con CON el servidor responde con la respuesta *piggybacked* en el ACK (RFC 7252 §5.2.1) y esa ida y vuelta es exactamente la que mide la Actividad 3; con NON no habría respuesta que medir. El motor CoAP (proceso `coap_engine`, UDP 5683, RFC 7252 §6.1) lo levanta `contiki-main.c` al incluir el módulo `coap` en el Makefile, igual que en los ejemplos.

### Qué se ve

Corrida headless con semilla 123456 (`a1-coap/correr.sh`, log completo en `tarea4/evidencia/a1-coap.log`). La raíz crea el DAG a los 0,38 s (`created DAG with instance ID 0, DAG ID fd00::201:1:1:1, rank 128`). Los clientes a un salto obtienen ruta y mandan su primer POST a los 20,7 s (n4), 22,7 s (n2) y 24,6 s (n3); los de dos saltos a los 36,9 s (n5) y 42,3 s (n6), porque primero tienen que oír un DIO de un nodo que ya tenga padre. Desde ahí cada ronda deja tres líneas, por ejemplo:

```
20695000 n4 [INFO: A1cli ] TX id=4 seq=1 mid=17114 len=20 'id=4 seq=1 temp=24.2'
20712624 n1 [INFO: A1raiz] REG id=4 seq=1 temp=24.2 hops=1 ttl=64 mid=17114 total=1
20719824 n4 [INFO: A1cli ] RX id=4 seq=1 code=2.04 mid=17114 rtt_ms=25 'ok hops=1'
```

y para n5 y n6 el mismo patrón con `hops=2 ttl=63`. En 282,4 s simulados se enviaron 129 POST (25 a 27 por cliente) y llegaron 129 respuestas `2.04 Changed`: ningún timeout, ninguna ronda saltada por petición en vuelo y ninguna retransmisión CoAP. El `.csc` para la GUI (`a1-coap.csc`) muestra lo mismo en el *Log Listener* filtrado por `A1`, el DODAG en el *Visualizer* y el tráfico en el *Timeline* y el *Radio Logger*.

**Evidencia:** `evidencia/a1-coap.log` (Mote output, 2467 líneas), `evidencia/a1-coap.pcap` (820 tramas 802.15.4), `evidencia/a1-tiempos-coap.csv` y `evidencia/a1-resumen-coap.md`. **[FALTA]** captura de pantalla de la GUI (Visualizer/Timeline/Radio Logger): la corrida se hizo headless; `a1-coap.csc` queda listo para abrirlo y sacarla.

---

## Actividad 2 — Red WSN con RPL Lite y MQTT

### Qué se construyó

Se implementó en Cooja una WSN de seis motes: el mote 1 es el `rpl-border-router` de Contiki-NG sin modificar, raíz del DODAG, y los motes 2 a 6 corren `mqtt-cliente.c`, una adaptación de `examples/mqtt-client` que publica cada 5 s en un único tema, `usm/tel329/temp`, una temperatura aleatoria obtenida con `random_rand()` (entre 15,0 y 34,9 °C, en décimas para no usar punto flotante). El payload es JSON: `{"id":<mote>,"seq":<n>,"temp":<xx.x>,"t":<ms de reloj>}`. El broker es Mosquitto 2.0.11, que corre en el host (el contenedor Docker) y escucha en `fd00::1:1883`; entre la red 6LoWPAN y el host está `tunslip6`, conectado al puerto serial del border router a través del *Serial Socket Server* de Cooja (puerto 60001), que crea la interfaz `tun0` con el prefijo `fd00::/64` y se lo anuncia al border router.

```
mote 2..6 (mqtt-cliente)  ──RPL Lite / 6LoWPAN──▶  mote 1 (border router)
                                                        │ serial (SerialSocketServer :60001)
                                                   tunslip6  ──▶ tun0 = fd00::1/64
                                                        │
                                                   mosquitto :1883   ◀── tcpdump
```

Del ejemplo se conservó la máquina de estados (registro → esperar ruta por defecto → `mqtt_connect` → publicar) y se quitó lo que no aportaba: IBM Watson, LEDs, botón, extensiones por plataforma, suscripción y el *ping* al padre. Lo que se cambió a propósito: el cliente usa **MQTT 3.1.1** (`MQTT_CONF_VERSION`; Contiki usa 3.1 por defecto) porque es el estándar OASIS que cita el enunciado; publica con **QoS 1** para recibir un `PUBACK` por cada mensaje, que es lo que permite medir ida y vuelta en la Actividad 3; el `client id` es `tel329-a2-mote<N>`; y el tamaño máximo de segmento TCP se subió de 32 a **64 bytes**, de modo que un `PUBLISH` completo (59 a 61 bytes: cabecera fija, tema, *packet id* y payload) viaje en un solo segmento y en una sola trama 802.15.4 sin fragmentación 6LoWPAN. El *keep alive* queda en 15 s (tres periodos de publicación, como en el ejemplo). Cada cliente imprime en el *Mote output* una línea `A2,PUB,…` al llamar a `mqtt_publish()` y una `A2,PUBACK,…` al recibir el evento `MQTT_EVENT_PUBACK`, con el `packet id` para emparejarlas, más el *rank* y el padre RPL del momento.

La configuración que define el experimento (`a2-mqtt/project-conf.h`):

```c
#define UIP_CONF_TCP 1                                   /* MQTT va sobre TCP; uIP lo trae apagado */
#define MQTT_CONF_VERSION MQTT_PROTOCOL_VERSION_3_1_1    /* el estándar OASIS que cita el enunciado */
#define A2_BROKER_IP   "fd00::1"                         /* el host del otro lado de tunslip6 */
#define A2_BROKER_PORT 1883
#define A2_TOPIC       "usm/tel329/temp"
#define A2_INTERVALO_S 5
#define A2_KEEP_ALIVE_S (3 * A2_INTERVALO_S)
#define A2_MAX_TCP_SEGMENT 64                            /* un PUBLISH completo en un segmento y una trama */
```

y la publicación en `mqtt-cliente.c`:

```c
temp_x10 = 150 + (random_rand() % 200);                  /* [15.0, 34.9] C en decimas */
len = snprintf(app_buffer, APP_BUFFER_SIZE,
               "{\"id\":%u,\"seq\":%u,\"temp\":%u.%u,\"t\":%lu}",
               node_id, seq_nr_value, temp_x10 / 10, temp_x10 % 10,
               (unsigned long)clock_time());
st = mqtt_publish(&conn, &mid, pub_topic, (uint8_t *)app_buffer, len,
                  MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);
printf("A2,PUB,%u,%u,%u,%lu,%u,%u\n", node_id, seq_nr_value, mid,
       (unsigned long)clock_time(), rank_dag(), padre_id());
/* ... y en mqtt_event(): */
case MQTT_EVENT_PUBACK:
  printf("A2,PUBACK,%u,%u,%lu\n", node_id, *((uint16_t *)data),
         (unsigned long)clock_time());
```

La topología se eligió para que la red tenga profundidad y no todos los clientes estén a un salto: con alcance UDGM de 50 m, los motes 2 y 3 están a 30 m de la raíz (1 salto), 4 y 5 a 60 m (2 saltos, vía 2 y 3) y 6 a 90 m (3 saltos, vía 4). RPL Lite, en modo *non-storing*, formó exactamente ese DODAG: la raíz aprende las rutas descendentes con los DAO y encamina hacia abajo con cabecera de ruteo por fuente (SRH), cosa que se ve en el log (`read SRH, path len 1`).

Todo corre con un solo `docker run --privileged` de la imagen oficial `contiker/contiki-ng` y el script `a2-mqtt/correr.sh`: arranca Mosquitto, lanza Cooja sin GUI con el `.csc`, espera a que el socket serial del mote 1 esté abierto, lanza `tunslip6`, captura con `tcpdump` en `tun0` y al terminar vuelca los logs y calcula los tiempos. La simulación corre a velocidad 1,0 (tiempo real) porque el broker y `tunslip6` viven en el reloj del host.

### Qué se ve

En `a2-mosquitto.log` los cinco clientes se conectan (`New client connected from fd00::20N:N:N:N as tel329-a2-mote<N> (p2, c1, k15)`: protocolo 3.1.1, *clean session*, *keep alive* 15) y a partir de ahí cada 5 s aparece el par `Received PUBLISH from tel329-a2-mote<N> (d0, q1, r0, m<mid>, 'usm/tel329/temp', ... (38 bytes))` / `Sending PUBACK`. En `a2-mqtt.log` (el *Mote output* con tiempo simulado en µs) están las líneas `A2,PUB` y `A2,PUBACK` de cada cliente, junto con la formación del DODAG (DIO, DAO, DAO-ACK). En `a2-tunslip6.log` está la consola del border router (`Routing: RPL Lite`, el prefijo recibido, su dirección global). Y en `a2-mqtt.pcap` está el tráfico TCP/MQTT tal como llega al host: los *hop limit* con que llegan los paquetes (63, 62 y 61) confirman los 1, 2 y 3 saltos de la topología.

| cliente | START (s) | primer CONNECTED (s) | primer PUB (s) | desconexiones |
|---|---|---|---|---|
| 2 | 0,2 | 22,0 | 22,5 | 0 |
| 3 | 0,9 | 12,3 | 12,4 | 0 |
| 4 | 0,3 | 34,7 | 35,1 | 0 |
| 5 | 0,7 | 24,8 | 25,2 | 0 |
| 6 | 0,2 | 47,7 | 48,0 | 0 |

| origen (mote) | paquetes hacia el broker | hop limit visto en tun0 | saltos = 64 − hlim |
|---|---|---|---|
| fd00::202:2:2:2 | 121 | 63 | 1 |
| fd00::203:3:3:3 | 122 | 63 | 1 |
| fd00::204:4:4:4 | 114 | 62 | 2 |
| fd00::205:5:5:5 | 116 | 62 | 2 |
| fd00::206:6:6:6 | 109 | 61 | 3 |

En 299 s simulados: 273 `PUBLISH` QoS 1 y 273 `PUBACK` (entre 51 y 58 por cliente), 0 desconexiones, 0 `PINGREQ`/`PINGRESP`.

**Evidencia:** `evidencia/a2-mqtt.log`, `a2-mosquitto.log`, `a2-tunslip6.log`, `a2-mqtt.pcap` y su volcado `a2-mqtt-pcap.txt` (901 paquetes TCP al/desde :1883), `a2-tiempos-mqtt.csv`, `a2-resumen-mqtt.md`. **[FALTA]** captura de pantalla de la GUI (`a2-mqtt.csc` abre con el *Serial Socket* ya en el puerto 60001 y el *Log Listener* filtrado por `A2,`).

---

## Actividad 3 — Captura de tráfico y tiempos de respuesta: CoAP vs MQTT

### Cómo se capturó

Las dos mitades usan la misma fuente primaria: el *Mote output* de Cooja, que antepone a cada línea el instante simulado en microsegundos. En CoAP el tiempo de respuesta de cada petición es la diferencia entre la línea `RX` y la línea `TX` del mismo par (cliente, seq) — del `POST` confirmable a su respuesta `2.04` piggybacked en el ACK — y la empareja `a1-coap/tiempos.py` (→ `evidencia/a1-tiempos-coap.csv`, una fila por petición). En MQTT es la diferencia entre `A2,PUBACK` y `A2,PUB` del mismo `packet id` — de `mqtt_publish()` al evento `MQTT_EVENT_PUBACK` — y la empareja `a2-mqtt/analizar.py` (→ `evidencia/a2-tiempos-mqtt.csv`). Los saltos salen en CoAP del hop limit que lee la raíz (`64 − ttl + 1`) y en MQTT de la cadena de padres RPL que reporta cada cliente, cotejada con el hop limit del pcap en `tun0`.

Las capturas de tráfico son distintas por naturaleza del camino. En CoAP el tráfico nunca sale de la red de radio, así que la captura es del radio: `evidencia/a1-coap.pcap`, 820 tramas IEEE 802.15.4 (linktype 195), de 4,28 s a 282,36 s. Cooja trae esa exportación en el *Radio Logger* ("6LoWPAN Analyzer with PCAP"), pero ese plugin es un `VisPlugin` y la versión actual no lo arranca en `--no-gui`, así que `js/volcar-log.js` escribe el pcap por su cuenta enganchándose a los triggers de transmisión del radio medium, con el mismo formato que `PcapExporter.java`. En MQTT el tráfico cruza al host, y ahí se capturó con `tcpdump` en `tun0`: `evidencia/a2-mqtt.pcap` (901 paquetes TCP al/desde :1883, reloj del host). Las tramas de Cooja no llevan FCS, así que para abrir `a1-coap.pcap` en Wireshark hay que fijar *IEEE 802.15.4 → FCS format: None* (`-o wpan.fcs_format:0` en `tshark`).

### Tabla comparativa

Tiempo de respuesta por mensaje visto desde la aplicación del sensor, en ms de tiempo simulado, agrupado por saltos de radio hasta la raíz. CoAP: 129 `POST` CON → `2.04`, 0 timeouts, 0 retransmisiones (corrida de 282,4 s). MQTT: 273 `PUBLISH` QoS 1 → `PUBACK`, 0 sin `PUBACK` (corrida de 299 s). Ambas con semilla 123456, UDGM con éxito 1,0, MAC CSMA, RPL Lite *non-storing*.

| saltos | protocolo | n | media | mediana | p95 | mín | máx |
|---|---|---|---|---|---|---|---|
| 1 | CoAP (POST CON → 2.04) | 79 | 48,4 | 46,8 | 81,8 | 13,5 | 90,8 |
| 1 | MQTT (PUBLISH QoS 1 → PUBACK) | 114 | 114,3 | 60,3 | 97,7 | 19,3 | 6238,3 |
| 2 | CoAP | 50 | 97,0 | 98,4 | 138,4 | 44,4 | 141,4 |
| 2 | MQTT | 108 | 168,4 | 164,3 | 246,2 | 67,6 | 284,6 |
| 3 | CoAP | — | — | — | — | — | — |
| 3 | MQTT | 51 | 213,2 | 211,8 | 290,8 | 132,8 | 381,8 |
| **todos** | **CoAP** | **129** | **67,2** | **61,8** | **121,4** | **13,5** | **141,4** |
| **todos** | **MQTT** | **273** | **154,2** | **130,6** | **243,7** | **19,3** | **6238,3** |

Notas de la tabla. Las filas por salto de CoAP se agregan desde `a1-tiempos-coap.csv` (n2+n3+n4 a un salto, n5+n6 a dos) con el mismo `p95` de `tiempos.py` (rango más cercano); las de MQTT son las de `a2-resumen-mqtt.md`, cuyo `analizar.py` interpola el p95. La red CoAP no tiene clientes a tres saltos. La fila MQTT de un salto está arrastrada por una sola muestra de 6238,3 ms (la primera publicación del mote 2); sin ella la media del mote 2 es indistinguible de la del mote 3 (61,5 ms). El detalle por cliente está en `evidencia/a1-resumen-coap.md` y `evidencia/a2-resumen-mqtt.md`.

### Análisis

**Primero la salvedad: la comparación mezcla protocolo y camino.** En la Actividad 1 el servidor CoAP corre en la propia raíz del DODAG, así que la respuesta `2.04` sale del mismo mote que termina la ruta RPL y la ida y vuelta es puramente de radio. En la Actividad 2 el broker está en el host: el `PUBACK` nace en Mosquitto después de cruzar el border router, el enlace serial SLIP (`tunslip6`), `tun0` y la pila TCP de Linux, y vuelve por el mismo camino. Entonces la diferencia entre filas no es "CoAP contra MQTT" a secas, es "CoAP sobre UDP terminado en la raíz" contra "MQTT sobre TCP terminado en un broker detrás de un border router". Separar las dos contribuciones exigiría poner el servidor CoAP también detrás de `tunslip6`; **[FALTA]** no se hizo. Con esa salvedad, lo que las cifras sí dicen es lo siguiente.

**A igual número de saltos las medianas son comparables y el orden es el esperado.** A un salto, 46,8 ms CoAP contra 60,3 ms MQTT; a dos, 98,4 contra 164,3 ms. Los dos protocolos hacen una ida y una vuelta por dato (RFC 7252 §5.2.1: respuesta piggybacked en el ACK del CON; OASIS MQTT 3.1.1 §3.4: `PUBACK` por cada `PUBLISH` con QoS 1), así que la mediana la fija el costo por trama, no el protocolo de aplicación. La diferencia que sobra la explican tres cosas: MQTT carga 20 bytes de cabecera TCP por segmento donde CoAP carga 8 de UDP y 4 de cabecera CoAP, lo que vuelve las tramas más largas; en los primeros intercambios de cada conexión Linux responde con dos paquetes por publicación (el ACK puro y el segmento con el `PUBACK`, separados mientras está en *quick ACK*), que son dos tramas más bajando por la red de radio; y el `PUBACK` paga además el tramo SLIP + host que el `2.04` de CoAP no paga.

**Lo que domina en las dos es el backoff CSMA por trama, no el aire ni la aplicación.** Una `POST` de CoAP ocupa 81–90 bytes de trama, que a 250 kb/s son unos 2,7–2,9 ms de aire, más el ACK de enlace de 5 bytes y los turnarounds; la respuesta, 56–64 bytes, unos 2 ms. Si solo contara el aire, un RTT a un salto rondaría los 6 ms, y el mínimo medido (13,5 ms) está cerca de eso. Lo que abre el rango hasta 45 ms por sentido es el backoff que el CSMA de Contiki-NG aplica **antes de cada transmisión**, aunque el canal esté libre: `csma-output.c` retrasa cada trama nueva un tiempo uniforme en `[0, (2^BE − 1) · backoff_period)` con `CSMA_MIN_BE = 3`, y en la plataforma Cooja `backoff_period()` devuelve 6 ms, es decir un retardo uniforme en [0, 42) ms. Sumado a los ~3 ms de aire da el intervalo 5,6–45,6 ms que se midió por sentido en CoAP, y la distribución casi plana de los RTT a un salto es la suma de dos uniformes. Por eso cada salto de radio suma del orden de 50 ms de ida y vuelta en los dos protocolos: en CoAP la mediana pasa de 46,8 a 98,4 ms de uno a dos saltos; en MQTT de 60,3 a 164,3 y a 211,8 ms de uno a dos a tres. Cada salto agrega una trama hacia arriba y otra hacia abajo, con su backoff, su ACK de enlace y el reenvío en el mote intermedio; la bajada además lleva la cabecera de ruteo por fuente que pone la raíz en RPL Lite *non-storing*. El MAC es CSMA, no ContikiMAC ni TSCH: los radios están siempre encendidos, así que no hay latencia por ciclo de trabajo.

**La cola es donde los protocolos se separan de verdad: el outlier de 6,2 s.** El máximo de CoAP en toda la corrida es 141,4 ms; el de MQTT, 6238,3 ms, la primera publicación del mote 2. Esa muestra es una retransmisión TCP. El enlace SLIP es un cuello de botella serial sin control de flujo: correlacionando los `tcp: bad checksum` del log de los motes (tres en toda la corrida: dos en el mote 2, uno en el 4) con la captura en `tun0`, cada uno coincide con un par de paquetes que el broker emitió hacia el mismo mote con menos de 1 ms de separación (ACK puro seguido del segmento con `CONNACK` o `PUBACK`); el segundo llega corrupto al mote y uIP lo descarta. El dato no se pierde, TCP lo repara, pero lo repara con su RTO, que para esa conexión quedó inflado por el *handshake* lento. Es un costo estructural de confiar la entrega a TCP: la recuperación es correcta pero la latencia la decide el temporizador de retransmisión, no la red. CoAP también retransmite un `CON` si no llega el ACK en `ACK_TIMEOUT · ACK_RANDOM_FACTOR` (RFC 7252 §4.2: 2 s × 1,5; Contiki-NG usa `COAP_RESPONSE_TIMEOUT` = 3 s), hasta `MAX_RETRANSMIT` = 4 veces, y una sola retransmisión habría puesto su RTT también en el orden de los segundos; pero en esta corrida no hubo ninguna (0 `Retransmitting` en el log, los 129 `mid` respondidos al primer intento), porque el medio UDGM no pierde tramas y las colisiones entre clientes las absorbe la retransmisión de enlace de CSMA, que es decenas de ms. La diferencia de fondo es que los reintentos de CoAP los controla la aplicación con tiempos acotados y cada datagrama es independiente, mientras que en MQTT una pérdida frena la cola TCP entera. En una red real con pérdidas, la cola de los dos la darían las retransmisiones, pero las de MQTT las fija el RTO de TCP y las de CoAP el temporizador de la aplicación.

**Lo que TCP cobra antes del primer dato: el handshake bloqueado hasta el DAO.** MQTT exige una conexión TCP (OASIS MQTT 3.1.1 §4.2) y después `CONNECT`/`CONNACK` antes de publicar. En la red RPL eso tiene un costo que no se ve en el RTT pero sí en el tiempo hasta la primera publicación: el cliente abre el TCP apenas tiene dirección global y ruta por defecto, pero el `SYN-ACK` del broker solo puede bajar cuando la raíz ya tiene la ruta descendente, es decir después del `DAO`/`DAO-ACK` del cliente. En la captura se ve al broker retransmitir el `SYN-ACK` (1, 2 y 4 s de *backoff*) hasta que la ruta existe; mientras tanto el mote reenvía el `SYN` (18 `SYN` y 27 `SYN-ACK` en la captura para solo 5 conexiones). Cada cliente recibió su `DAO-ACK` entre 0,7 y 4,6 s antes de lograr el `CONNECTED`, y la primera publicación llegó entre 12 s (mote 3) y 48 s (mote 6, a tres saltos) después del arranque. Con CoAP sobre UDP ese *handshake* no existe: el primer datagrama ya es el dato (los clientes CoAP mandaron su primer `POST` apenas tuvieron ruta, entre 20,7 y 42,3 s, tiempo que es íntegramente de convergencia RPL), y si no hubiera ruta de vuelta solo se perdería la respuesta de ese mensaje, no se postergaría toda la sesión. Tampoco hubo *keep alive* que pagar: con 15 s de *keep alive* y publicaciones cada 5 s no apareció ningún `PINGREQ`/`PINGRESP`; si el sensor publicara menos seguido que su *keep alive*, MQTT pagaría un par de paquetes por intervalo solo para mantener la conexión (§3.1.2.10), costo que CoAP no tiene porque no mantiene conexión.

**El tamaño del mensaje casi no pesa, y ninguno fragmenta.** El payload CoAP son 20–21 bytes de texto; con la cabecera CoAP de 4 bytes (RFC 7252 §3; Lección 10, "Mensaje CoAP"), las opciones Uri-Path y Content-Format, y UDP e IPv6 comprimidos por 6LoWPAN IPHC, las `POST` ocupan 81–90 bytes de trama y las respuestas 56–64; el `PUBLISH` MQTT completo pesa 59–61 bytes (cabecera fija, tema, *packet id* y payload JSON de 38 bytes) y se forzó un segmento TCP de 64 bytes justamente para que fuera en una sola trama. Todo cabe en los 127 bytes de 802.15.4, así que en ninguno de los dos hay fragmentación 6LoWPAN, que habría agregado una trama más por sentido y por salto.

**En resumen.** En esta red el tiempo de respuesta de CoAP es, en mediana, 47 ms a un salto y ~98 ms a dos; el de MQTT con QoS 1, 60 ms a un salto, 164 a dos y 212 a tres, entre 1,3 y 1,7 veces el de CoAP a igual profundidad, con la salvedad de que el `PUBACK` cruza además el border router y el host. Los dos escalan linealmente con los saltos porque lo que manda es el backoff CSMA por trama (hasta 42 ms por trama en Cooja), no la cabecera de aplicación. La diferencia cualitativa está en la cola: CoAP sobre UDP no tiene *handshake*, no mantiene conexión y sus reintentos son acotados por la aplicación, de modo que su máximo quedó en 141 ms; MQTT sobre TCP paga el *handshake* bloqueado hasta el DAO, el *keep alive* si publica poco, y cuando el enlace SLIP corrompe un segmento la latencia la decide el RTO de TCP (6,2 s). Lo que MQTT da a cambio es la entrega *al menos una vez* con *packet id* (§4.3.2), el desacople publicador/suscriptor por el broker y la sesión persistente, que CoAP no ofrece por sí solo.

**Supuestos y límites.** Red simulada (UDGM con éxito 1,0 y sin pérdida por distancia, 6 motes, semilla fija 123456), broker en la misma máquina: los números miden el costo del protocolo y de la pila, no un enlace real. En MQTT el tiempo simulado corre a 1,0× contra el reloj del host; en los tramos en que el host se atrasa, el RTT medido desde el mote incluye ese atraso. **[FALTA]** los pcap no se validaron con Wireshark/tshark (no están instalados en la máquina); `a1-coap.pcap` se validó por estructura con Python (magic, linktype 195, 428 tramas de datos + 392 ACK, payload CoAP legible en 358 tramas de datos) y `a2-mqtt.pcap` con el volcado de `tcpdump -tt -nn -v`.

---

## Actividad 4 — Sensores Contiki-NG en Azure IoT Hub

Código y evidencia en `tarea4/a4-azure/` y `tarea4/evidencia/a4-*`. La cadena completa sensor → enrutador → middleware quedó verificada localmente dentro del contenedor `contiker/contiki-ng`; el tramo contra Azure IoT Hub está implementado y listo, pero su corrida depende de las cadenas de conexión del portal, que se marcan como **[FALTA: cadena de conexión de Azure]**.

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
```

### 4.b ¿Cuál es el resultado presentado por el sensor o mote?

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

**Evidencia** (`evidencia/a4-sensor.log`, `evidencia/a4-curl.txt`). Al arrancar, el mote reporta `Routing: RPL Lite`, `Net: tun6`, link-layer `0102.0304.0506.0708` y `tun0` con `fd00::1/64`. Desde el contenedor, `ping6 -c 3 fd00::302:304:506:708` responde los tres paquetes (0 % de pérdida, RTT 0,07 a 0,77 ms) y tres `GET http://[fd00::302:304:506:708]/` devuelven `HTTP 200 OK`, `Server: Contiki`, `Content-Type: application/json` con cuerpos `{"temp":24,"hum":80}`, `{"temp":24,"hum":81}` y `{"temp":16,"hum":86}`; el mismo `GET` desde el Mac vía el puente TCP devolvió `{"temp":33,"hum":80}`. Del lado del mote, cada petición queda registrada como `[INFO: Sensor ] GET / -> {"temp":..,"hum":..}`. El enrutador `rpl-border-router` del árbol también compila para native en el contenedor (`evidencia/a4-border-router-build.log`, exit 0); la topología con Cooja + Serial Socket + `tunslip6` queda documentada en `a4-azure/router/README.md` pero no se corrió (requiere GUI).

### 4.a ¿Cuál es el resultado presentado por la interfaz middleware?

**Planteamiento.** `middleware/middleware-azure.js` (Node 22, ESM) reemplaza al `middleware-azure.js` del libro: se quita `request` (deprecado) y Express (no participaba del flujo), se usa `fetch` nativo con timeout, y el SDK `azure-iot-device` + `azure-iot-device-mqtt` actual con su API de promesas (`Client.fromConnectionString(cs, Mqtt)`, `await client.open()`, `await client.sendEvent(message)`). Cada 3 s consulta `SENSOR_URL`, valida que `temp` y `hum` sean números, arma `{"deviceId","temperature","humidity"}` (el `deviceId` se toma de la propia cadena de conexión), agrega la propiedad de aplicación `temperatureAlert = temperature > 30` y lo envía al IoT Hub por MQTT. La cadena de conexión y la URL del sensor vienen sólo de variables de entorno (`.env.example` versionado, `.env` no). El modo `--dry-run` hace todo menos conectar a Azure e imprime el mensaje que mandaría, lo que permite probar sensor → middleware sin credenciales.

```js
function buildMessage({ temperature, humidity }) {
  const data = JSON.stringify({ deviceId, temperature, humidity });
  const message = new Message(data);
  message.properties.add('temperatureAlert', temperature > TEMPERATURE_ALERT_C ? 'true' : 'false');
  message.contentType = 'application/json';
  message.contentEncoding = 'utf-8';
  return message;
}

async function tick(client) {
  const reading = await readSensor();            // fetch(SENSOR_URL) con timeout, valida temp/hum
  const message = buildMessage(reading);
  if (dryRun) {
    console.log(`[dry-run] enviaría a IoT Hub: ${message.getData()} ...`);
    return;
  }
  await client.sendEvent(message);               // Client.fromConnectionString(cs, Mqtt)
  console.log(`[iothub] enviado: ${message.getData()} temperatureAlert=${alert}`);
}
```

**Evidencia local** (`evidencia/a4-middleware-dryrun.log`). Corrido contra el sensor real durante 11 s: `Modo --dry-run ... deviceId=contiki-ng-01 sensor=http://127.0.0.1:8080/ cada 3000 ms`, seguido de cuatro pares de líneas `[sensor] ... -> {"temp":26,"hum":89}` / `[dry-run] enviaría a IoT Hub: {"deviceId":"contiki-ng-01","temperature":26,"humidity":89} properties={temperatureAlert:false}`; en la tercera lectura (`temp=35`) la propiedad pasa a `temperatureAlert:true`, lo que confirma el umbral.

**Evidencia contra Azure.** **[FALTA: cadena de conexión de Azure]**. Cuando esté, correr desde `tarea4/a4-azure/middleware`:

```bash
AZURE_DEVICE_CONNECTION_STRING='HostName=<hub>.azure-devices.net;DeviceId=contiki-ng-01;SharedAccessKey=<clave>' \
SENSOR_URL=http://127.0.0.1:8080/ node middleware-azure.js | tee ../../evidencia/a4-middleware-azure.log
```

y pegar aquí la salida esperada: `Conectado a IoT Hub por MQTT como contiki-ng-01 ...` y luego una línea `[iothub] enviado: {...} temperatureAlert=...` por cada lectura, más el gráfico *Device to cloud messages* del portal.

### 4.c ¿Cuál es el resultado presentado por el suscriptor de Azure IoT Hub?

**Planteamiento.** `subscriber/azure-sensor-subscriber.js` reemplaza el paquete archivado `azure-event-hubs` por `@azure/event-hubs` 6. El IoT Hub expone la telemetría en su endpoint integrado `messages/events`, compatible con Event Hubs (AMQP 5671), así que un `EventHubConsumerClient` con el consumer group `$Default` y la cadena "Event Hub-compatible endpoint" se suscribe a todas las particiones desde `latestEventPosition` (equivalente al `startAfterTime: Date.now()` del libro) e imprime, por cada evento, la partición, el `iothub-connection-device-id` de las propiedades de sistema, el cuerpo JSON y las propiedades de aplicación (`temperatureAlert`). La cadena viene sólo de `EVENTHUB_COMPATIBLE_CONNECTION_STRING`; si falta o si se pega por error la cadena del dispositivo (`HostName=...`), el programa termina con un mensaje que indica qué cadena corresponde.

```js
const client = new EventHubConsumerClient(consumerGroup, connectionString);
const subscription = client.subscribe(
  {
    processEvents: async (events, context) => {
      for (const event of events) {
        console.log(`Message received (partition ${context.partitionId}, deviceId=${event.systemProperties?.['iothub-connection-device-id'] ?? '?'}):`);
        console.log(JSON.stringify(event.body));
        if (event.properties && Object.keys(event.properties).length > 0) {
          console.log(`properties: ${JSON.stringify(event.properties)}`);
        }
      }
    },
    processError: async (err, context) => { console.error(`Error en partición ${context.partitionId ?? '?'}: ${err.message}`); },
  },
  { startPosition: latestEventPosition },
);
```

**Evidencia local** (`evidencia/a4-subscriber-sinvar.log`). Sin la variable, el programa arranca y sale con código 1 y el mensaje `Falta EVENTHUB_COMPATIBLE_CONNECTION_STRING (IoT Hub > Built-in endpoints > Event Hub compatible endpoint)`; con una cadena de tipo dispositivo responde `La cadena no parece la del endpoint compatible con Event Hubs ...`.

**Evidencia contra Azure.** **[FALTA: cadena de conexión de Azure]**. Cuando esté, correr desde `tarea4/a4-azure/subscriber`, con el middleware enviando en otra terminal:

```bash
EVENTHUB_COMPATIBLE_CONNECTION_STRING='Endpoint=sb://<ns>.servicebus.windows.net/;SharedAccessKeyName=service;SharedAccessKey=<clave>;EntityPath=<hub>' \
node azure-sensor-subscriber.js | tee ../../evidencia/a4-subscriber-azure.log
```

y pegar aquí la salida esperada: `Suscrito al IoT Hub (consumer group $Default); esperando mensajes...` y luego, cada 3 s, `Message received (partition N, deviceId=contiki-ng-01):` con `{"deviceId":"contiki-ng-01","temperature":T,"humidity":H}` y `properties: {"temperatureAlert":"true|false"}`, que debe coincidir uno a uno con lo que el middleware reporta como enviado.

**Publicación.** El enunciado pide publicar la adaptación en GitHub o equivalente. El código vive en el repo `dreamxist/tel329`, carpeta `tarea4/a4-azure/`, que hoy es privado: **[FALTA]** decidir si se hace público, se publica en un repo aparte o se adjunta como zip (ver `entrega/Pendientes_Tarea4.md`).

---

## Archivos de evidencia

Rutas relativas a `tarea4/` en el repo.

| Archivo | Actividad | Qué muestra |
|---|---|---|
| `evidencia/a1-coap.log` | A1/A3 | Mote output completo de la corrida CoAP, `<us simulados> n<id> <msg>` (2467 líneas; TX/REG/RX de cada petición, DIO/DAO de RPL) |
| `evidencia/a1-coap.pcap` | A1/A3 | 820 tramas IEEE 802.15.4 del radio (linktype 195, sin FCS), 4,28 s a 282,36 s simulados |
| `evidencia/a1-tiempos-coap.csv` | A3 | una fila por petición CoAP (129): cliente, seq, t_envio_ms, t_respuesta_ms, rtt_ms, saltos, código, mid, bytes, temp, t_registro_raiz_ms, rtt_mote_ms |
| `evidencia/a1-resumen-coap.md` | A3 | n, media, mediana, p95, mín, máx por cliente y global, generado por `a1-coap/tiempos.py` |
| `evidencia/a2-mqtt.log` | A2/A3 | Mote output completo de la corrida MQTT (A2,START/CONNECTED/PUB/PUBACK por cliente, DODAG, los 3 `tcp: bad checksum`) |
| `evidencia/a2-mosquitto.log` | A2 | el broker en `-v`: CONNECT/CONNACK por cliente, cada PUBLISH recibido y PUBACK enviado |
| `evidencia/a2-tunslip6.log` | A2 | tunslip6 y la consola del border router (Routing: RPL Lite, prefijo, dirección global) |
| `evidencia/a2-mqtt.pcap` | A2/A3 | captura `tcpdump -i tun0 port 1883` (901 paquetes TCP, reloj del host) |
| `evidencia/a2-mqtt-pcap.txt` | A2/A3 | volcado `tcpdump -tt -nn -v` del pcap anterior, con los hop limit 63/62/61 |
| `evidencia/a2-tiempos-mqtt.csv` | A3 | una fila por PUBLISH (273): cliente, seq, mid, t_publish_ms, t_puback_ms, rtt_ms, saltos, rank_dag, padre |
| `evidencia/a2-resumen-mqtt.md` | A3 | n, media, mediana, p95, mín, máx por cliente, por saltos y global; tiempos de conexión; hop limit en tun0, generado por `a2-mqtt/analizar.py` |
| `evidencia/a4-sensor.log` | A4.b | arranque del mote native (tun0 fd00::1/64, RPL Lite, link-layer 0102.0304.0506.0708) y cada GET atendido con su JSON |
| `evidencia/a4-curl.txt` | A4.b | `ifconfig tun0`, `ping6` al mote (0 % pérdida), tres GET con 200 OK + application/json, y el curl desde el Mac vía el puente TCP |
| `evidencia/a4-border-router-build.log` | A4.b | `make TARGET=native` del `rpl-border-router` del árbol, exit 0 |
| `evidencia/a4-middleware-dryrun.log` | A4.a | middleware real contra el sensor real, 11 s: cuatro lecturas, mensaje {deviceId,temperature,humidity} y temperatureAlert que pasa a true con temp=35 |
| `evidencia/a4-subscriber-sinvar.log` | A4.c | el suscriptor arranca y falla con mensaje claro sin EVENTHUB_COMPATIBLE_CONNECTION_STRING |

Pendiente de generar (ver `Pendientes_Tarea4.md`): `evidencia/a4-middleware-azure.log`, `evidencia/a4-subscriber-azure.log` y `evidencia/a4-portal-d2c.png` (envío real a Azure); capturas GUI de Cooja `evidencia/a1-cooja-*.png`, `evidencia/a2-cooja-*.png`; capturas de Wireshark `evidencia/a1-wireshark.png`, `evidencia/a2-wireshark.png`.

## Código entregado

| Archivo(s) | Qué es |
|---|---|
| `a1-coap/root-coap.c` | raíz del DODAG + servidor CoAP (nodo 1) |
| `a1-coap/cliente-coap.c` | cliente CoAP que sensa y notifica por POST confirmable (nodos 2..6) |
| `a1-coap/resources/res-temp.c` | el recurso `sensores/temp`: POST/PUT registra con hops, GET devuelve la última |
| `a1-coap/project-conf.h · Makefile · a1-coap.csc` | configuración, build (CSMA + IPv6 + RPL Lite + coap) y simulación para la GUI |
| `a1-coap/correr.sh · js/volcar-log.js · tiempos.py` | corrida headless, pcap desde el ScriptRunner, log → CSV + resumen |
| `a2-mqtt/mqtt-cliente.c` | cliente MQTT 3.1.1 QoS 1 sobre RPL Lite, adaptado de examples/mqtt-client (motes 2..6) |
| `a2-mqtt/project-conf.h · Makefile · mosquitto.conf · a2-mqtt.csc` | TCP, versión, broker, tema, segmento 64 B; build; broker; simulación con Serial Socket :60001 |
| `a2-mqtt/correr.sh · js/volcar-log.js · analizar.py · Dockerfile` | orquestación mosquitto + Cooja + tunslip6 + tcpdump, log → CSV + resumen |
| `a4-azure/sensor/sensor-azure.c · project-conf.h · Makefile` | mote sensor con httpd-ws que responde {"temp","hum"} |
| `a4-azure/router/README.md · tcp-forward.py` | rpl-border-router (Cooja + tunslip6 o native) y puente TCP hacia el Mac |
| `a4-azure/middleware/middleware-azure.js · package.json · .env.example` | middleware Node 22: fetch al sensor → azure-iot-device (MQTT) |
| `a4-azure/subscriber/azure-sensor-subscriber.js · package.json · .env.example` | suscriptor @azure/event-hubs al endpoint integrado del IoT Hub |

## Registro de ejecución

Las dos simulaciones Cooja corrieron sin GUI dentro de `contiker/contiki-ng`, con un `ScriptRunner` inyectado por `correr.sh` que vuelca el Mote output con tiempo simulado y corta la corrida: A1 a los 282,4 s simulados, cuando los cinco clientes llegaron a 25 respuestas; A2 a los 300 s simulados (299 s de log), a velocidad 1,0 porque Mosquitto y `tunslip6` viven en el reloj del host y el contenedor corrió con `--privileged` por `/dev/net/tun`. El sensor de la A4 se compiló para `TARGET=native` en el mismo contenedor y se consultó desde adentro (`ping6`, `GET`) y desde el Mac a través de `tcp-forward.py` publicado en `127.0.0.1:8080`; el middleware corrió en el Mac en `--dry-run` contra ese puente. Los comandos exactos de cada corrida están en `tarea4/README.md` §"Reproducir cada corrida".

---

## Referencias

- Shelby, Z., Hartke, K., Bormann, C. (2014). *RFC 7252: The Constrained Application Protocol (CoAP)*. IETF. §3 (formato de mensaje), §4.2 (mensajes confirmables y retransmisión), §4.8 (parámetros de transmisión), §5 (modelo petición/respuesta), §5.2.1 (respuestas piggybacked), §6.1 (puerto 5683).
- Hartke, K. (2015). *RFC 7641: Observing Resources in CoAP*. IETF.
- OASIS (2014). *MQTT Version 3.1.1*. OASIS Standard. §3.1.2.10 (keep alive), §3.3–3.4 (PUBLISH/PUBACK), §4.2 (transporte sobre TCP), §4.3.2 (QoS 1, al menos una vez).
- TEL329, Lección 8 — pila de protocolos de la WSN (aplicación sobre UDP/uIP, IPv6 + RPL, 6LoWPAN, CSMA/802.15.4).
- TEL329, Lección 10 — CoAP: modelo de petición/respuesta y formato del mensaje.
- Contiki-NG. Documentación: [CoAP](https://docs.contiki-ng.org/en/develop/doc/programming/CoAP.html), [MQTT](https://docs.contiki-ng.org/en/develop/doc/tutorials/MQTT.html), [RPL border router](https://docs.contiki-ng.org/en/develop/doc/tutorials/RPL-border-router.html); fuentes `os/net/mac/csma/csma-output.c`, `os/net/app-layer/coap/`, `os/net/app-layer/mqtt/`, `os/net/app-layer/httpd-ws/`, `examples/coap/`, `examples/mqtt-client/`, `examples/rpl-border-router/`; `tools/cooja/doc/ChangeLog.md` ("Avoid starting the AWT thread in headless mode").
- Eclipse Mosquitto 2.0.11, [mosquitto.org](https://mosquitto.org/).
- Kurniawan, A. (2018). *Practical Contiki-NG: Programming for Wireless Sensor Networks*. Apress. Cap. "Contiki-NG en la nube".
- Microsoft Learn: [Create an IoT hub](https://learn.microsoft.com/azure/iot-hub/create-hub), [Register and connect an IoT device](https://learn.microsoft.com/azure/iot-hub/create-connect-device?tabs=portal), [Understand the built-in endpoint](https://learn.microsoft.com/azure/iot-hub/iot-hub-devguide-messages-read-builtin).
- Enunciado `TEL329-Tarea4-2026-v1.0.ipynb` (B. Contreras, UTFSM, 2026).
