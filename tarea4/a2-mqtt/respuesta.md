# Actividad 2 — RPL Lite + MQTT

Se implementó en Cooja una WSN de seis motes: el mote 1 es el `rpl-border-router`
de Contiki-NG sin modificar, raíz del DODAG, y los motes 2 a 6 corren
`mqtt-cliente.c`, una adaptación de `examples/mqtt-client` que publica cada 5 s en
un único tema, `usm/tel329/temp`, una temperatura aleatoria obtenida con
`random_rand()` (entre 15,0 y 34,9 °C, en décimas para no usar punto flotante). El
payload es JSON: `{"id":<mote>,"seq":<n>,"temp":<xx.x>,"t":<ms de reloj>}`. El
broker es Mosquitto 2.0.11, que corre en el host (el contenedor Docker) y escucha
en `fd00::1:1883`; entre la red 6LoWPAN y el host está `tunslip6`, conectado al
puerto serial del border router a través del *Serial Socket Server* de Cooja
(puerto 60001), que crea la interfaz `tun0` con el prefijo `fd00::/64` y se lo
anuncia al border router.

Del ejemplo se conservó la máquina de estados (registro → esperar ruta por
defecto → `mqtt_connect` → publicar) y se quitó lo que no aportaba: IBM Watson,
LEDs, botón, extensiones por plataforma, suscripción y el *ping* al padre. Lo que
se cambió a propósito: el cliente usa **MQTT 3.1.1** (`MQTT_CONF_VERSION`; Contiki
usa 3.1 por defecto) porque es el estándar OASIS que cita el enunciado; publica
con **QoS 1** para recibir un `PUBACK` por cada mensaje, que es lo que permite
medir ida y vuelta en la Actividad 3; el `client id` es `tel329-a2-mote<N>`; y el
tamaño máximo de segmento TCP se subió de 32 a **64 bytes**, de modo que un
`PUBLISH` completo (59 a 61 bytes: cabecera fija, tema, *packet id* y payload)
viaje en un solo segmento y en una sola trama 802.15.4 sin fragmentación 6LoWPAN.
El *keep alive* queda en 15 s (tres periodos de publicación, como en el ejemplo).
Cada cliente imprime en el *Mote output* una línea `A2,PUB,…` al llamar a
`mqtt_publish()` y una `A2,PUBACK,…` al recibir el evento `MQTT_EVENT_PUBACK`,
con el `packet id` para emparejarlas, más el *rank* y el padre RPL del momento.

La topología se eligió para que la red tenga profundidad y no todos los clientes
estén a un salto: con alcance UDGM de 50 m, los motes 2 y 3 están a 30 m de la
raíz (1 salto), 4 y 5 a 60 m (2 saltos, vía 2 y 3) y 6 a 90 m (3 saltos, vía 4).
RPL Lite, en modo *non-storing*, formó exactamente ese DODAG: la raíz aprende las
rutas descendentes con los DAO y encamina hacia abajo con cabecera de ruteo por
fuente (SRH), cosa que se ve en el log (`read SRH, path len 1`).

Todo corre con un solo `docker run --privileged` de la imagen oficial
`contiker/contiki-ng` y el script `correr.sh`: arranca Mosquitto, lanza Cooja sin
GUI con el `.csc`, espera a que el socket serial del mote 1 esté abierto, lanza
`tunslip6`, captura con `tcpdump` en `tun0` y al terminar vuelca los logs y
calcula los tiempos. La simulación corre a velocidad 1,0 (tiempo real) porque el
broker y `tunslip6` viven en el reloj del host.

Lo que se observa (evidencia en `tarea4/evidencia/a2-*`): en `a2-mosquitto.log`
los cinco clientes se conectan (`New client connected from fd00::20N:N:N:N as
tel329-a2-mote<N> (p2, c1, k15)`: protocolo 3.1.1, *clean session*, *keep alive*
15) y a partir de ahí cada 5 s aparece el par `Received PUBLISH from
tel329-a2-mote<N> (d0, q1, r0, m<mid>, 'usm/tel329/temp', ... (38 bytes))` /
`Sending PUBACK`. En `a2-mqtt.log` (el *Mote output* con tiempo simulado en µs)
están las líneas `A2,PUB` y `A2,PUBACK` de cada cliente, junto con la formación
del DODAG (DIO, DAO, DAO-ACK). En `a2-tunslip6.log` está la consola del border
router (`Routing: RPL Lite`, el prefijo recibido, su dirección global). Y en
`a2-mqtt.pcap` está el tráfico TCP/MQTT tal como llega al host: los *hop limit*
con que llegan los paquetes (63, 62 y 61) confirman los 1, 2 y 3 saltos de la
topología.

# Actividad 3 (mitad MQTT) — tiempos de respuesta

**Qué se midió.** Para cada publicación, el tiempo entre `mqtt_publish()` en el
mote y la llegada del `PUBACK` al mismo mote, en el reloj simulado de Cooja (del
*Mote output*, resolución de µs). Es el tiempo de respuesta visto por la
aplicación en el sensor: recorre la red RPL hacia arriba, el border router, el
enlace serial SLIP, `tun0`, la pila TCP de Linux y Mosquitto, y vuelve por el
mismo camino. Los saltos se obtienen de la cadena de padres RPL que reporta cada
cliente y se cotejan con el *hop limit* del pcap. Las dos fuentes complementarias
son el log de Mosquitto (`PUBLISH` recibido / `PUBACK` enviado, resolución de 1 s)
y la captura en `tun0` (µs, pero reloj del host, no el simulado).

**Resultados** (corrida de 300 s simulados, semilla 123456; CSV completo en `tarea4/evidencia/a2-tiempos-mqtt.csv`).

Fuente: `a2-mqtt.log` (299 s simulados, 273 PUBLISH QoS 1, 273 PUBACK).

| cliente | saltos | n | media (ms) | mediana (ms) | p95 (ms) | min (ms) | max (ms) | sin PUBACK |
|---|---|---|---|---|---|---|---|---|
| 2 | 1 | 56 | 169.1 | 59.8 | 95.1 | 19.3 | 6238.3 | 0 |
| 3 | 1 | 58 | 61.5 | 61.8 | 98.8 | 23.3 | 133.3 | 0 |
| 4 | 2 | 53 | 152.1 | 136.6 | 237.4 | 67.6 | 277.6 | 0 |
| 5 | 2 | 55 | 184.1 | 179.0 | 249.9 | 85.6 | 284.6 | 0 |
| 6 | 3 | 51 | 213.2 | 211.8 | 290.8 | 132.8 | 381.8 | 0 |
| **global** | | 273 | 154.2 | 130.6 | 243.7 | 19.3 | 6238.3 | 0 |

| saltos | n | media (ms) | mediana (ms) | p95 (ms) | min (ms) | max (ms) |
|---|---|---|---|---|---|---|
| 1 | 114 | 114.3 | 60.3 | 97.7 | 19.3 | 6238.3 |
| 2 | 108 | 168.4 | 164.3 | 246.2 | 67.6 | 284.6 |
| 3 | 51 | 213.2 | 211.8 | 290.8 | 132.8 | 381.8 |

| cliente | START (s) | primer CONNECTED (s) | primer PUB (s) | desconexiones |
|---|---|---|---|---|
| 2 | 0.2 | 22.0 | 22.5 | 0 |
| 3 | 0.9 | 12.3 | 12.4 | 0 |
| 4 | 0.3 | 34.7 | 35.1 | 0 |
| 5 | 0.7 | 24.8 | 25.2 | 0 |
| 6 | 0.2 | 47.7 | 48.0 | 0 |

Captura en tun0 (`a2-mqtt-pcap.txt`): 901 paquetes TCP al/desde :1883.

| origen (mote) | paquetes hacia el broker | hop limit visto en tun0 | saltos = 64 - hlim |
|---|---|---|---|
| fd00::202:2:2:2 | 121 | 63 | 1 |
| fd00::203:3:3:3 | 122 | 63 | 1 |
| fd00::204:4:4:4 | 114 | 62 | 2 |
| fd00::205:5:5:5 | 116 | 62 | 2 |
| fd00::206:6:6:6 | 109 | 61 | 3 |

**Análisis.**

*Lo que cuesta un PUBLISH con QoS 1.* Con 273 publicaciones y 273 `PUBACK` (0
pérdidas de aplicación), la mediana crece con los saltos de forma casi lineal: 60
ms a un salto, 164 ms a dos y 212 ms a tres, es decir del orden de 50 a 100 ms
por salto de radio ida y vuelta. Cada salto de radio agrega una trama 802.15.4 hacia arriba y otra
hacia abajo, con su CSMA, su ACK de enlace y el reenvío en el mote intermedio, y
el `PUBACK` tiene que volver a bajar con la cabecera de ruteo por fuente que pone
la raíz. En el camino de vuelta hay además dos paquetes TCP del broker por cada
publicación en los primeros intercambios de la conexión (el ACK puro y el segmento
con el `PUBACK`, que Linux manda separados mientras está en *quick ACK*); después
Linux los junta en un solo segmento y la respuesta queda en un paquete.

*Lo que cuesta TCP antes del primer dato.* MQTT exige una conexión TCP (OASIS
MQTT 3.1.1 §4.2) y después un `CONNECT`/`CONNACK` antes de publicar. En la red
RPL eso tiene un costo que no se ve en el RTT pero sí en el tiempo hasta la
primera publicación: el cliente abre el TCP apenas tiene dirección global y ruta
por defecto, pero el `SYN-ACK` del broker solo puede bajar cuando la raíz ya
tiene la ruta descendente, es decir después del `DAO`/`DAO-ACK` del cliente. En
la captura se ve al broker retransmitir el `SYN-ACK` (1, 2 y 4 s de *backoff*)
hasta que la ruta existe; mientras tanto el mote reenvía el `SYN` (18 `SYN` y 27
`SYN-ACK` en la captura para solo 5 conexiones). En esta corrida cada cliente
recibió su `DAO-ACK` entre 0,7 y 4,6 s antes de lograr el `CONNECTED`, y la
primera publicación llegó entre 12 s (mote 3) y 48 s (mote 6, a tres saltos)
después del arranque. Con CoAP sobre
UDP ese *handshake* no existe: el primer datagrama ya es el dato, y si no hay
ruta de vuelta solo se pierde la respuesta de ese mensaje, no se posterga toda la
sesión.

*El costo del border router y tunslip.* El enlace SLIP es un cuello de botella
serial sin control de flujo. Correlacionando los `tcp: bad checksum` del log de
los motes con la captura, cada uno coincide con un par de paquetes que el broker
emitió hacia el mismo mote con menos de 1 ms de separación (ACK puro seguido del
segmento con `CONNACK` o `PUBACK`): el segundo llega corrupto al mote y uIP lo
descarta. El dato no se pierde, TCP lo repara, pero lo repara con su RTO: el del
mote (uIP) o el del broker, que para esa conexión quedó inflado por el
*handshake* lento. De ahí sale el máximo de varios segundos de la tabla, en la
primera publicación de la conexión. Es un costo que paga MQTT por confiar
en TCP para la entrega: la recuperación es correcta pero la latencia la decide el
temporizador de retransmisión, no la red. En la corrida definitiva pasó tres
veces (dos en el mote 2, una en el 4) y dejó un único RTT de 6.238 ms, la primera
publicación del mote 2; sin esa muestra la media del mote 2 es indistinguible de
la del 3. Es el único máximo fuera de escala de la tabla: el resto queda bajo 400
ms. CoAP con mensajes `CON` también
retransmite, pero con un `ACK_TIMEOUT` de 2 s y *backoff* propio (RFC 7252 §4.8),
y sin que una pérdida frene los mensajes siguientes, que son datagramas
independientes.

*Keep alive.* Con *keep alive* de 15 s y publicaciones cada 5 s no hubo ningún
`PINGREQ`/`PINGRESP` en toda la corrida (0 en el log de Mosquitto): cada
`PUBLISH` reinicia el temporizador. Si el sensor publicara menos seguido que su
*keep alive*, pagaría un par de paquetes extra por intervalo solo para mantener
viva la conexión TCP (OASIS MQTT 3.1.1 §3.1.2.10), un costo que CoAP sobre UDP no
tiene porque no mantiene conexión.

*Comparado con lo que se espera de CoAP sobre UDP.* Un `POST` confirmable de CoAP
hace un solo viaje de ida y vuelta por mensaje (petición `CON` → respuesta `ACK`
con el código, RFC 7252 §4.2), con una cabecera de 4 bytes y sin cabecera TCP de
20 bytes, sin *handshake*, sin ACKs puros de transporte y sin estado de conexión
en el nodo. Por eso, a igual topología, la mediana de CoAP debería ser igual o
menor que la de MQTT QoS 1 (los dos hacen una ida y una vuelta por dato, pero
MQTT transporta más bytes por trama y, en los primeros intercambios, dos paquetes
en vez de uno en la bajada) y, sobre todo, su cola debería ser más corta: los
reintentos de CoAP los controla la aplicación con tiempos acotados, los de MQTT
los controla TCP. Lo que MQTT da a cambio es la entrega *al menos una vez* con
*packet id* (§4.3.2), el desacople publicador/suscriptor por el broker y la
sesión persistente, que CoAP no ofrece por sí solo. Contra la mitad CoAP de esta
misma tarea (`tarea4/evidencia/a1-resumen-coap.md`: mediana 47 ms a un salto y
95 a 102 ms a dos, con el servidor CoAP corriendo en la propia raíz del DODAG)
MQTT mide 60 ms a un salto y 164 ms a dos: entre 1,3 y 1,7 veces más. Parte de
esa diferencia es el protocolo (TCP + MQTT contra UDP + CoAP) y parte es el
camino, porque el `PUBACK` de MQTT además cruza el border router, el enlace SLIP
y la pila del host, cosa que el `ACK` de CoAP de la A1 no hace. Separar las dos
contribuciones exigiría poner el servidor CoAP también detrás de `tunslip6`
[FALTA: no se hizo].

**Supuestos y límites.** La red es simulada (UDGM con éxito 1,0 y sin pérdida
por distancia, 6 motes, semilla fija 123456) y el broker está en la misma
máquina: los números miden el costo del protocolo y de la pila, no un enlace
real. El tiempo simulado de Cooja corre a 1,0× contra el reloj del host; en los
tramos en que el host se atrasa, el RTT medido desde el mote incluye ese atraso.
Los saltos se infieren del padre RPL reportado por el cliente en cada
publicación. Las lecciones del ramo no se pudieron consultar al redactar
(los PDF están en iCloud sin descargar, [FALTA: citar la lección de capa de
aplicación]); los fundamentos se tomaron del estándar OASIS MQTT 3.1.1, de la
RFC 7252 y de la documentación de Contiki-NG.
