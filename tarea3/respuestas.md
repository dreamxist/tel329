# TEL329 · Tarea 3 — uIP y RPL · borrador de respuestas

> **Estado.** Las actividades 2 y 4 están respondidas (son de análisis, no
> necesitan corrida). La 1 tiene la conclusión escrita y le falta pegar la
> evidencia. La 3 tiene el método y las tablas armadas, y le faltan los
> números: hay que correrla.
>
> Lo que falta está marcado con **[FALTA]**. Nada de lo que está escrito se
> inventó: lo verificable salió de leer la fuente de Contiki-NG, y está citado.

---

## Actividad 1 — el periodo del `etimer`

### La respuesta corta

Los tres casos del enunciado — `1e-5`, `1e-10` y `1e-20` × `CLOCK_SECOND` —
**colapsan al mismo valor: 0 ticks**. No son tres escenarios distintos, son el
mismo escenario degenerado tres veces, y eso es lo que el ejercicio hace ver.

El motivo es de tipos, no de red. En la plataforma Cooja
`CLOCK_CONF_SECOND 1000L` (`arch/platform/cooja/contiki-conf.h:105`), o sea un
tick dura **1 ms**. `etimer_set()` recibe un `clock_time_t`, que es **entero**:

| Caso | Cálculo | Ticks tras truncar |
|---|---|---|
| base | `10 × 1000` | 10000 (10 s) |
| `1e-5` | `0,00001 × 1000 = 0,01` | **0** |
| `1e-10` | `1e-10 × 1000 = 1e-7` | **0** |
| `1e-20` | `1e-20 × 1000 = 1e-17` | **0** |

El cliente imprime el valor al arrancar, así que no hay que creerlo:

```
[INFO: A1cli] CLOCK_SECOND=1000 -> intervalo = 0 ticks
```

### Qué pasa en la red con 0 ticks

Un `etimer` armado en 0 está vencido en la siguiente vuelta del planificador:
`PROCESS_WAIT_EVENT_UNTIL(etimer_expired(...))` deja pasar de inmediato y el
proceso del cliente queda girando tan rápido como el planificador lo despacha.
Consecuencias, en orden de aparición:

1. **La radio no duerme.** El nodo pasa de transmitir una vez cada 10 s a
   intentarlo de forma continua. Todo el argumento de ahorro de energía de una
   WSN — el de la Lección 1 y el que la Tarea 1 demostró con el slotframe TSCH
   — desaparece: el *duty cycle* se va al techo.
2. **La cola de CSMA se llena y empieza a descartar.** El cliente genera
   datagramas más rápido de lo que la MAC puede sacarlos.
3. **El tráfico de control de RPL queda tapado.** Los DIO y DAO compiten por el
   mismo medio contra el flood de la aplicación, así que la convergencia del
   DODAG se retrasa o no ocurre.
4. **`node_is_reachable()` puede no volverse nunca verdadero**, y el log se
   llena de `todavia sin ruta hacia la raiz` mientras `missed_tx` sube. El nodo
   se inunda a sí mismo antes de tener con quién hablar.

### El fondo: `etimer` no puede expresar sub-milisegundos

La conclusión que vale la pena escribir en el informe no es "se inunda", es que
**la granularidad del reloj de eventos es el piso de lo que se puede pedir**.
Con `CLOCK_SECOND = 1000`, `etimer` no expresa nada bajo 1 ms, y pedírselo no
da un periodo chico: da cero. Para temporización más fina existe `rtimer`, con
su propia base `RTIMER_SECOND`, que es la herramienta correcta cuando se
necesita precisión por debajo del tick del sistema — y es lo que usa TSCH por
dentro para sus ranuras.

### [FALTA] Evidencia

- [ ] Correr los cuatro casos y guardar los logs en `evidencia/` (hay script:
      `./correr-casos.sh`, ver README).
- [ ] Captura del *Timeline* del caso base: ráfagas separadas, radio apagada
      entre medio.
- [ ] Captura del *Timeline* de un caso degenerado: actividad continua y
      colisiones.
- [ ] Mote output del servidor con el `acumulado:` de cada caso — es la cifra
      que compara los cuatro de una sola mirada.

---

## Actividad 2 — `simple_udp`, y cuándo hace falta `tunslip6`

### Cómo funciona `simple_udp`

`simple_udp_register(&conn, puerto_local, addr_remota, puerto_remoto, cb)`
registra una conexión UDP y guarda el *callback* en la estructura `conn`. Con
`addr_remota == NULL` la conexión acepta datagramas de cualquier origen,
filtrando solo por los dos puertos. Es lo que permite que un único servidor
atienda a todos los clientes sin conocerlos de antemano.

La recepción no la maneja la aplicación: cuando llega un datagrama al puerto
registrado, la pila entrega el paquete y `simple-udp` busca la conexión que
calza e invoca el *callback* con el origen, el destino, los puertos y un
puntero al payload. Ese puntero apunta a `uip_buf`, el buffer de la pila — por
eso **solo es válido dentro del callback** y por eso ahí mismo se puede leer la
cabecera IP del paquete que llegó, que es el truco que usan las actividades 3 y
4 para sacar el TTL (`UIP_IP_BUF->ttl`, con `UIP_IP_BUF` definido en
`os/net/ipv6/uip.h:71` como `((struct uip_ip_hdr *)uip_buf)`).

`simple_udp_sendto(&conn, datos, largo, addr_destino)` envía al destino que se
le pasa, usando el puerto remoto que quedó en el registro. En el servidor de
estas actividades se lo llama con la dirección del remitente que trae el propio
callback: eso es el eco.

### Por qué este escenario no necesita `tunslip6`

Porque nadie fuera de la simulación participa. El servidor hace dos papeles a
la vez: es raíz del DODAG (`NETSTACK_ROUTING.root_start()`, que anuncia el
prefijo y acepta los DAO) y es el destino de la aplicación. El cliente no
necesita que le configuren una dirección: espera a que
`NETSTACK_ROUTING.node_is_reachable()` sea verdadero y le pide a la pila la
dirección de la raíz con `get_root_ipaddr()`. Todo el camino —prefijo,
direcciones, rutas, datagramas— vive dentro de Cooja.

### Las tres topologías

| | Dónde vive la raíz | Qué se necesita | Para qué sirve |
|---|---|---|---|
| **(a) RPL autónoma en Cooja** (`rpl-udp`) | un mote de la simulación | nada más | estudiar la malla por dentro: convergencia, rank, saltos, TTL, RTT, colisiones |
| **(b) Border router + `tunslip6`** | un mote, conectado por SLIP a un socket serial (puerto 60001) | `make tunslip6`, `sudo`, una interfaz `tun0` en el host | que el host entre a la malla: `ping6` desde el sistema operativo, un navegador, un cliente CoAP |
| **(c) Border router nativo** | un proceso del host (`TARGET=native`), con un *slip-radio* como puente a la radio | compilar el BR para `native` | mismo alcance que (b) con menos piezas intermedias: el BR corre en el host y no dentro de un mote simulado |

La regla práctica: **`tunslip6` se justifica cuando el tráfico tiene que cruzar
la frontera de la simulación.** Si la pregunta es sobre el interior de la malla
—que es el caso de las cuatro actividades de esta tarea— el ejemplo `rpl-udp`
basta y además es más robusto: no depende de `sudo`, del dispositivo `tun`, ni
de que el socket serial del mote quede escuchando. Cada una de esas tres cosas
es un modo de falla que no aporta nada a la pregunta que se quiere responder.

> **[FALTA] Verificar un detalle de (c)** en la documentación de Contiki-NG
> antes de entregar: cómo queda exactamente la interfaz `tun` con el BR nativo
> (si la crea el propio BR o sigue haciendo falta el paso de `tunslip6`). El
> resto de la tabla está comprobado en la fuente y en el enunciado.

---

## Actividad 3 — RPL Lite vs. RPL Classic

### El escenario

`a3-lite-vs-classic/rpl-udp-multisalto.csc`: cinco nodos **en línea, separados
40 m, con alcance de 50 m**. Cada nodo alcanza únicamente a sus vecinos
inmediatos, así que la topología fuerza una cadena de hasta **4 saltos** en vez
de una estrella. Sin esa separación la comparación no mide nada: con todos los
nodos a un salto, *storing* y *non-storing* se comportan igual.

Los dos escenarios se compilan del mismo código:

```bash
make distclean TARGET=cooja && make TARGET=cooja                                  # Lite
make distclean TARGET=cooja && make TARGET=cooja MAKE_ROUTING=MAKE_ROUTING_RPL_CLASSIC
```

### Lo que ya se puede afirmar

**El TTL es aritmética, no medición.** El hop limit por defecto es
`UIP_TTL 64` (`os/net/ipv6/uipopt.h:125`), puesto en
`uip_ds6_if.cur_hop_limit` al inicializar la interfaz
(`os/net/ipv6/uip-ds6.c:139`), y cada reenvío lo baja en 1. Entonces el TTL que
el servidor ve en el paquete de un cliente a *n* saltos es **64 − n**:

| Nodo | Saltos a la raíz | TTL esperado en la raíz |
|---|---|---|
| 2 | 1 | 63 |
| 3 | 2 | 62 |
| 4 | 3 | 61 |
| 5 | 4 | 60 |

El código imprime las dos cosas —`ttl=` y `saltos=`— en cada recepción, así que
la tabla se comprueba de una pasada. Si un TTL no calza con la posición del
nodo, el DODAG no se armó como la topología sugiere, y *eso* es el hallazgo.

**Dónde se separan Lite y Classic.** RPL Lite implementa solo **non-storing**
(MOP 1): los nodos intermedios no guardan rutas, la raíz arma el camino de
bajada y lo mete en la cabecera de enrutamiento del paquete, que por lo tanto
**crece con cada salto**. RPL Classic por defecto es **storing** (MOP 2): cada
nodo guarda las rutas de su subárbol, los paquetes de bajada son más chicos y
el costo se paga en memoria por nodo. La diferencia se ve en el camino
descendente, o sea en el **eco** de la aplicación y en la respuesta del `ping`
— no en la subida.

### Cómo medir

1. Arrancar la simulación y esperar la convergencia (los clientes dejan de
   imprimir `todavia sin ruta`).
2. Cada cliente imprime su dirección global al tenerla. Copiarla.
3. Abrir el shell de la raíz: click derecho en el mote 1 → *Mote tools for
   Contiki* → *Serial Socket*, o la ventana *Serial*. Correr `ping <addr>` por
   cada cliente. La respuesta trae **ttl y delay** ya calculados
   (`os/services/shell/shell-commands.c:163`).
4. El RTT de aplicación lo mide el cliente solo: guarda el instante del envío y
   al llegar el eco imprime `rtt=NNN ms`.
5. `routes` en la raíz y en un nodo intermedio: es la forma directa de mostrar
   *storing* vs *non-storing* — en Classic el intermedio tiene entradas, en
   Lite no.

### [FALTA] Tablas de resultados

**RPL Lite (non-storing)**

| Nodo | Saltos | TTL observado | Delay ICMP (ms) | RTT UDP (ms) | Tx/Rx/MissedTx |
|---|---|---|---|---|---|
| 2 | 1 | | | | |
| 3 | 2 | | | | |
| 4 | 3 | | | | |
| 5 | 4 | | | | |

**RPL Classic (storing)**

| Nodo | Saltos | TTL observado | Delay ICMP (ms) | RTT UDP (ms) | Tx/Rx/MissedTx |
|---|---|---|---|---|---|
| 2 | 1 | | | | |
| 3 | 2 | | | | |
| 4 | 3 | | | | |
| 5 | 4 | | | | |

- [ ] `routes` en la raíz y en el nodo 3, en los dos escenarios (captura).
- [ ] Captura del *Timeline* de cada escenario, para las colisiones.
- [ ] Una línea de cierre: ¿el RTT crece lineal con los saltos o se degrada
      más rápido? ¿Y es peor en Lite por el tamaño de la cabecera de bajada?

---

## Actividad 4 — de `sky-websense.c` a Contiki-NG

### Qué hacía `sky-websense.c` y por qué no se mantiene

En Contiki-OS cada mote Sky levantaba un servidor HTTP mínimo
(`httpd-simple`/`webserver-nogui`) y publicaba sus propias lecturas de sensor;
uno abría el navegador contra la dirección del mote y leía la página. Modelo
**pull**, con el servidor en el nodo constreñido.

Contiki-NG no conserva ese patrón: el módulo de webserver quedó asociado al
border router, no a cada mote, y el ejemplo de estudio equivalente pasó a ser
`rpl-udp` — modelo **push**, con los motes empujando el dato al sink.

### La integración propuesta (implementada en esta carpeta)

Tres piezas, mínimas a propósito:

1. **`udp-client.c` lee el sensor.** La lectura está aislada en una sola
   función, `leer_sensor()`, que hoy devuelve una caminata aleatoria acotada
   alrededor de una base por nodo. Es el único punto a cambiar para enchufar un
   sensor real: la firma se mantiene y el resto del código no sabe de dónde
   viene el número.
2. **El dato viaja en el datagrama** con el formato del enunciado:
   `id=%u seq=%lu t=%d`.
3. **`udp-server.c` lo registra en una línea parseable:**
   `SENSOR,<id>,<seq>,<temp>,<ttl>`. Se filtra `SENSOR` en el *Mote output* y la
   salida se pega directo en una planilla. Por eso el `project-conf.h` de esta
   actividad deja RPL, IPv6, TCPIP y MAC en `WARN`: cualquier otro módulo en
   `INFO` ensucia la serie.

### Por qué el visor web va en el sink y no en cada mote

El enunciado no exige navegador, pero pide justificar dónde iría. Cuatro
razones, de la más fuerte a la más débil:

1. **Rompe el ahorro de energía.** Un servidor HTTP es *pull*: para responder
   hay que estar escuchando. Un mote que no puede dormir pierde lo único que
   hace viable una WSN a batería. El modelo push deja al mote despertar, enviar
   y volver a dormir.
2. **Obliga a que cada mote sea alcanzable desde afuera.** Hay que exponer la
   malla completa con border router y rutas hacia cada nodo, en vez de exponer
   un solo punto.
3. **TCP y HTTP cuestan caro sobre 6LoWPAN.** La trama de 802.15.4 son 127
   bytes: una respuesta HTTP se fragmenta, y cada fragmento perdido cuesta la
   respuesta entera.
4. **El sink ya tiene todo junto.** Tiene la serie de todos los nodos, puede
   guardar historia y atender a muchos clientes sin tocar la malla. El
   navegador consulta al sink; la malla no se entera.

### [FALTA] Evidencia

- [ ] Correr la simulación unos minutos y pegar el bloque de líneas `SENSOR,…`
      filtrado del *Mote output*.
- [ ] Un gráfico de la serie por nodo (basta la planilla).
- [ ] Captura del filtro aplicado en el *Mote output*, que es lo que el
      enunciado pide mostrar.
