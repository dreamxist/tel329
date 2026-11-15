# Actividad 1 — Red WSN con RPL Lite y CoAP

## Qué se construyó

Una red de seis Cooja motes sobre la pila de Contiki-NG que vimos en la Lección 8 (aplicación CoAP sobre UDP/uIP, IPv6 con RPL, adaptación 6LoWPAN y MAC CSMA sobre IEEE 802.15.4). El nodo 1 arranca como raíz del DODAG con `NETSTACK_ROUTING.root_start()` y es además el servidor CoAP: activa un único recurso, `sensores/temp`, cuyo handler de POST/PUT registra lo que llega y responde `2.04 Changed`. Los nodos 2 a 6 son los clientes: se unen al DODAG, esperan a que `NETSTACK_ROUTING.node_is_reachable()` sea verdadero, toman la dirección de la raíz con `get_root_ipaddr()` y cada 10 segundos envían un POST **confirmable** a `coap://[raíz]:5683/sensores/temp` con el texto `id=<nodo> seq=<k> temp=<xx.x>`, donde la temperatura sale de `random_rand()` (`150 + random_rand() % 300` décimas de grado, es decir uniforme en 15,0–44,9 °C). La topología (UDGM, alcance 50 m) deja a n2, n3 y n4 a 40 m de la raíz y a n5 y n6 a 80 m, de modo que estos dos solo llegan a través de n2 y n3: la red tiene clientes a uno y a dos saltos y RPL es el que arma el camino.

El código adaptado está en `tarea4/a1-coap/` (`root-coap.c`, `cliente-coap.c`, `resources/res-temp.c`, `Makefile`, `project-conf.h`) y parte de los ejemplos `examples/coap/coap-example-server` y `coap-example-client` de Contiki-NG, con tres cambios de fondo. Primero, el servidor del ejemplo no era raíz RPL ni el cliente conocía al servidor más que por una dirección fija (`SERVER_EP "coap://[fe80::202:2:2:2]"`); acá el endpoint es la raíz que RPL ya conoce, así que no hay direcciones hardcodeadas y el mismo binario sirve para los cinco clientes. Segundo, en vez de `COAP_BLOCKING_REQUEST`, que deja al protothread clavado hasta la respuesta, se usa la API no bloqueante `coap_send_request()` de `coap-callback-api.h`: el callback recibe el estado `COAP_REQUEST_STATUS_RESPONSE` con el mensaje (o `TIMEOUT` si se agotaron las retransmisiones), y eso permite imprimir el envío y la respuesta como líneas separadas, que Cooja marca con el tiempo simulado. Tercero, el recurso lee el hop limit del datagrama IPv6 que todavía está en `uip_buf` cuando corre el handler (`UIP_IP_BUF->ttl`) y de ahí deduce los saltos (`64 − ttl + 1`, porque uIP sale con `UIP_TTL = 64` y cada reenvío descuenta uno); lo devuelve en el payload de la respuesta para que el cliente también lo registre.

Se eligió que el cliente notifique con POST y no que la raíz observe (RFC 7641) porque el enunciado pide que los clientes *notifiquen*, y en el modelo petición/respuesta de CoAP (RFC 7252 §5; Lección 10, "Modelo de Petición/Respuesta") la forma directa de empujar un dato al servidor es una petición POST o PUT. Se eligió confirmable y no non-confirmable porque con CON el servidor responde con la respuesta *piggybacked* en el ACK (RFC 7252 §5.2.1) y esa ida y vuelta es exactamente la que mide la Actividad 3; con NON no habría respuesta que medir. El motor CoAP (proceso `coap_engine`, UDP 5683, RFC 7252 §6.1) lo levanta `contiki-main.c` al incluir el módulo `coap` en el Makefile, igual que en los ejemplos.

## Qué se ve

Corrida headless con semilla 123456 (`correr.sh`, log completo en `tarea4/evidencia/a1-coap.log`). La raíz crea el DAG a los 0,38 s (`created DAG with instance ID 0, DAG ID fd00::201:1:1:1, rank 128`). Los clientes a un salto obtienen ruta y mandan su primer POST a los 20,7 s (n4), 22,7 s (n2) y 24,6 s (n3); los de dos saltos a los 36,9 s (n5) y 42,3 s (n6), porque primero tienen que oír un DIO de un nodo que ya tenga padre. Desde ahí cada ronda deja tres líneas, por ejemplo:

```
20695000 n4 [INFO: A1cli ] TX id=4 seq=1 mid=17114 len=20 'id=4 seq=1 temp=24.2'
20712624 n1 [INFO: A1raiz] REG id=4 seq=1 temp=24.2 hops=1 ttl=64 mid=17114 total=1
20719824 n4 [INFO: A1cli ] RX id=4 seq=1 code=2.04 mid=17114 rtt_ms=25 'ok hops=1'
```

y para n5 y n6 el mismo patrón con `hops=2 ttl=63`. En 282,4 s simulados se enviaron 129 POST (25 a 27 por cliente) y llegaron 129 respuestas `2.04 Changed`: ningún timeout, ninguna ronda saltada por petición en vuelo y ninguna retransmisión CoAP. El `.csc` para la GUI (`a1-coap.csc`) muestra lo mismo en el *Log Listener* filtrado por `A1`, el DODAG en el *Visualizer* y el tráfico en el *Timeline* y el *Radio Logger*.

# Actividad 3 (mitad CoAP) — Captura de tráfico y tiempos de respuesta

## Cómo se capturó

Dos fuentes, las dos con tiempo simulado de Cooja. La primera es el Mote output: Cooja antepone el instante en microsegundos a cada línea, así que el tiempo de respuesta de cada petición es la diferencia entre la línea `RX` y la línea `TX` del mismo par (cliente, seq); el script `tiempos.py` las empareja y produce `tarea4/evidencia/a1-tiempos-coap.csv` (una fila por petición: cliente, seq, t_envio_ms, t_respuesta_ms, rtt_ms, saltos, código, mid, bytes de payload, temperatura, y el instante en que la raíz registró el dato) y el resumen `a1-resumen-coap.md`. La segunda es la captura del radio, `tarea4/evidencia/a1-coap.pcap`: 820 tramas IEEE 802.15.4 (linktype 195). Cooja trae esa exportación en el plugin *Radio Logger* ("6LoWPAN Analyzer with PCAP"), pero ese plugin es un `VisPlugin` y la versión actual de Cooja no lo arranca en modo `--no-gui` (ChangeLog, "Avoid starting the AWT thread in headless mode"); el `.csc` de la GUI lo deja configurado igual, y para la corrida headless el script `js/volcar-log.js` escribe el pcap por su cuenta enganchándose a los triggers de transmisión del radio medium y volcando cada trama con el mismo formato que `PcapExporter.java`. Las tramas no llevan FCS (Cooja no la genera), así que en Wireshark hay que fijar *IEEE 802.15.4 → FCS format: None*.

## Tabla de tiempos de respuesta

129 POST confirmables, 129 respuestas 2.04, 0 timeouts, 0 retransmisiones. RTT en milisegundos de tiempo simulado, del `TX` del cliente al `RX` de su respuesta.

| cliente | saltos | n | media | mediana | p95 | mín | máx |
|---|---|---|---|---|---|---|---|
| n2 | 1 | 26 | 51,1 | 47,3 | 81,8 | 13,5 | 87,8 |
| n3 | 1 | 26 | 43,0 | 47,3 | 60,8 | 17,8 | 64,8 |
| n4 | 1 | 27 | 51,0 | 46,8 | 89,8 | 17,8 | 90,8 |
| n5 | 2 | 25 | 94,2 | 95,4 | 137,4 | 44,4 | 138,4 |
| n6 | 2 | 25 | 99,8 | 102,4 | 139,4 | 55,4 | 141,4 |
| **global** | 1–2 | 129 | 67,2 | 61,8 | 121,4 | 13,5 | 141,4 |

Con la línea `REG` de la raíz el RTT se parte en subida (TX → REG) y bajada (REG → RX). A un salto cada sentido tarda entre 5,6 y 45,6 ms (medianas de 14 a 33 ms); a dos saltos, entre 16 y 89 ms (medianas de 45 a 52 ms). El `rtt_ms` que calcula el propio mote con `clock_time()` coincide con el del log con una diferencia máxima de 0,6 ms.

## Qué explica el RTT

**La mayor parte no es transmisión sino espera de CSMA.** Una POST ocupa 81–90 bytes de trama (ver abajo), que a 250 kb/s son unos 2,7–2,9 ms de aire, más el ACK de enlace de 5 bytes y los turnarounds; la respuesta, 56–64 bytes, unos 2 ms. Si solo contara el aire, un RTT a un salto rondaría los 6 ms, y el mínimo medido (13,5 ms, con sentidos de 5,6 ms) está cerca de eso. Lo que abre el rango hasta 45 ms por sentido es el backoff que el CSMA de Contiki-NG aplica **antes de cada transmisión**, aunque el canal esté libre: `csma-output.c` retrasa cada trama nueva un tiempo uniforme en `[0, (2^BE − 1) · backoff_period)` con `CSMA_MIN_BE = 3`, y en la plataforma Cooja `backoff_period()` devuelve `20 · CLOCK_SECOND / 3125` = 6 ms ("para compensar el medio de radio de grano grueso"), es decir un retardo uniforme en [0, 42) ms. Sumado a los ~3 ms de aire da exactamente el intervalo 5,6–45,6 ms que se midió por sentido, y la distribución casi plana de los RTT a un salto (entre 13 y 91 ms, concentrada en 30–50 ms) es la suma de dos uniformes. El MAC es CSMA, no ContikiMAC ni TSCH: los radios están siempre encendidos, así que no hay latencia por ciclo de trabajo; con un MAC con *duty cycling* habría que sumar la espera hasta la ventana de escucha del receptor, que suele dominar el RTT.

**Cada salto suma una ida y una vuelta más.** Los clientes a dos saltos (n5, n6) tardan en mediana 95–102 ms contra 47 ms de los de un salto: el doble, porque la POST pasa por el padre (n2 o n3) que la recibe, la reenvía con su propio backoff de CSMA y su propio ACK de enlace, y la respuesta hace el mismo camino de vuelta por la ruta descendente que RPL Lite instaló en la raíz (modo *non-storing*: la raíz conoce la ruta completa y la pone en un encabezado de enrutamiento por origen). Son cuatro transmisiones de datos y cuatro ACK de enlace en vez de dos y dos; el hop limit lo confirma, 64 a un salto y 63 a dos, y el `hops=2` que devuelve la raíz.

**No hubo retransmisiones confirmables, y eso define la escala.** CoAP retransmite un CON si no llega el ACK en `ACK_TIMEOUT · ACK_RANDOM_FACTOR` (RFC 7252 §4.2: 2 s × 1,5; Contiki-NG usa `COAP_RESPONSE_TIMEOUT` = 3 s), duplicando hasta `MAX_RETRANSMIT` = 4 veces. Una sola retransmisión habría puesto el RTT en el orden de los **segundos**, no de las decenas de milisegundos: el log no tiene ningún `Retransmitting`, los 129 `mid` se responden en su primer intento y el máximo global es 141 ms. En este medio no se pierden tramas (UDGM con `success_ratio` 1,0) y las colisiones entre clientes que transmiten a la vez las absorbe la retransmisión de enlace de CSMA, que es mucho más rápida (decenas de ms) que la de CoAP. En una red real con pérdidas la cola de la distribución la darían esas retransmisiones de aplicación, y el p95 ya no sería 121 ms sino varios segundos.

**El tamaño del mensaje casi no pesa, pero se ve en el pcap.** El payload de aplicación son 20–21 bytes de texto; la cabecera CoAP son 4 bytes fijos (RFC 7252 §3; Lección 10, "Mensaje CoAP") más las opciones Uri-Path (`sensores`, `temp`) y Content-Format, y UDP e IPv6 viajan comprimidos por 6LoWPAN IPHC. El resultado son tramas de 81–90 bytes para la POST y de 56–64 para la respuesta `2.04` (que solo lleva `ok hops=n`); los 8 bytes de diferencia entre 82 y 90 aparecen cuando la trama la reenvía un padre y la dirección IPv6 de origen ya no se puede derivar de la dirección MAC de quien transmite. Todo cabe en una trama de 127 bytes, así que no hay fragmentación 6LoWPAN, que habría agregado una trama más por sentido y por salto. El resto del pcap son los 392 ACK de enlace (5 bytes) y el control de RPL: DIO de 73 bytes, DIS de 43–51 y DAO de 97–102. En total 428 tramas de datos para 129 peticiones: 258 son las POST y sus respuestas a un salto, unas 100 más los reenvíos de n5 y n6, y el resto es RPL.

En resumen: en esta red el tiempo de respuesta de CoAP es, en mediana, **47 ms a un salto y ~100 ms a dos**, y está dominado por el backoff aleatorio del CSMA en cada transmisión (hasta 42 ms por trama en Cooja), no por el tamaño del mensaje ni por el protocolo de aplicación, que agrega 4 bytes de cabecera y una respuesta piggybacked sin mensajes extra. Los saltos escalan el RTT linealmente, y las retransmisiones confirmables, que no ocurrieron, son el único mecanismo capaz de llevarlo a la escala de segundos.

## Evidencia

- `tarea4/evidencia/a1-coap.log` — Mote output completo con tiempo simulado (2467 líneas).
- `tarea4/evidencia/a1-coap.pcap` — 820 tramas 802.15.4, linktype 195, de 4,28 s a 282,36 s.
- `tarea4/evidencia/a1-tiempos-coap.csv` — una fila por petición (129).
- `tarea4/evidencia/a1-resumen-coap.md` — el resumen que genera `tiempos.py`.
- `[FALTA]` captura de pantalla de la GUI (Visualizer/Timeline/Radio Logger): la corrida se hizo headless; el `a1-coap.csc` queda listo para abrirlo y sacarla.
- El pcap se validó con `tshark` 3.6.2 (`-o wpan.fcs_format:0`, salida en `evidencia/a1-tshark.txt`): de las 820 tramas, 358 decodifican hasta CoAP, 179 `CON POST /sensores/temp` y 179 `ACK 2.04 Changed`, con 129 MID distintos de cada uno (las 129 peticiones del log; las 50 de más son el reenvío del mote intermedio en las peticiones a dos saltos).
