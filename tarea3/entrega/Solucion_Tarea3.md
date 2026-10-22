# Solución — Tarea 3: uIP y RPL

**Asignatura:** TEL329 Redes de Sensores · 2026  
**Proyecto Contiki-NG:** `solucion_rpl_udp/`  
**Repositorio Contiki-NG:** `E:\contiki-ng` (en WSL: `/mnt/e/contiki-ng`)

## 1. Temporizadores y carga UDP en Cooja

El cliente de `rpl-udp` se modifica para enviar `id=<mote> seq=<n> t=<valor>` y mantener una raíz UDP que imprime cada muestra y devuelve el mismo datagrama. Los fuentes listos para compilar están en `solucion_rpl_udp/udp-client.c` y `solucion_rpl_udp/udp-server.c`. `PERIOD_CASE=0` es el control de 10 s; `1`, `2` y `3` representan los periodos solicitados. El valor sensado es una secuencia determinista arbitraria, adecuada para probar el formato y el recorrido extremo a extremo.

| Periodo pedido | Expresión en ticks | Resultado en Cooja (`CLOCK_SECOND=1000`) | Consecuencia esperable |
|---|---:|---:|---|
| `10^-5 * CLOCK_SECOND` | `CLOCK_SECOND / 100000` | 0 ticks (0,01 tick antes de truncar) | El `etimer` queda vencido inmediatamente; envío en cuanto el proceso vuelve a ejecutarse. No equivale a 100 000 envíos/s. |
| `10^-10 * CLOCK_SECOND` | `CLOCK_SECOND / 10000000000` | 0 ticks | Igual resolución efectiva que el caso anterior; más intentos de envío de los que la simulación puede procesar por tiempo lógico. |
| `10^-20 * CLOCK_SECOND` | 0 ticks | 0 | También se cuantiza a cero; no es físicamente distinguible de los dos casos anteriores usando `etimer`. |

`etimer` usa `clock_time_t`, un contador entero de ticks. Para los motes Cooja de esta versión, `CLOCK_CONF_SECOND=1000`; incluso `10^-5 * CLOCK_SECOND = 0,01` tick es inferior a la resolución y se trunca a cero. Los otros factores también producen cero. Una expresión como `10^-5` tampoco es notación de potencia en C (`^` es XOR); se debe escribir como una constante decimal/división y convertir a ticks. En los tres casos sub-tick se usa una rama de preprocesador para no compilar el `% SEND_INTERVAL` original: una comprobación `if` de ejecución no basta, porque GCC detecta que el divisor es constante cero y rechaza el código con `-Werror=div-by-zero`.

**Resultados medidos (Cooja, Lite, cuatro motes):** las tres compilaciones sub-tick producen exactamente el mismo `SEND_INTERVAL=0`. Tras alcanzar el root, el cliente 2 generó 30 solicitudes antes de que el test de Cooja registrara su estadística a `10,344792 s` de tiempo simulado: `Tx/Rx/MissedTx = 30/1/0`. El sink registró la primera muestra a `10,3182 s`; la prueba terminó correctamente con `TEST OK`. Esto confirma convergencia suficiente para enviar/recibir tráfico y la expiración inmediata del temporizador. La diferencia entre los exponentes no es observable a resolución de `etimer`. El único `RTT_ticks=2` capturado en cada corrida extrema **no es una medición fiable de ida y vuelta**: los envíos se solapan y la marca de tiempo única del cliente se sobrescribe. `MissedTx=0` solo indica que la ruta no se perdió; no significa que los ecos llegaran.

| Caso Lite | Tiempo simulado al registro | `Tx/Rx/MissedTx` del mote 2 | Resultado |
|---|---:|---:|---|
| Control `10 * CLOCK_SECOND` | 113,985 s | 10/9/0 | Tráfico periódico de 10 s; el contador se lee al décimo envío, con un eco aún pendiente. |
| `10^-5 * CLOCK_SECOND` | 10,345 s | 30/1/0 | Temporizador de cero ticks; ráfaga de solicitudes y pocos ecos observados antes de cerrar la prueba. |
| `10^-10 * CLOCK_SECOND` | 10,345 s | 30/1/0 | Igual que el anterior por cuantización a cero. |
| `10^-20 * CLOCK_SECOND` | 10,345 s | 30/1/0 | Igual que los otros factores sub-tick. |

La diferencia `Tx`/`Rx` muestra pérdidas de datagramas o ecos bajo la ráfaga, consistente con contención y colisiones en la topología. No es un contador de colisiones: el modo headless usado no expone el Timeline gráfico ni permite atribuir cada pérdida a una colisión. Los `.csc` generados conservan el plugin Timeline para inspección gráfica posterior.

### Registro de ejecución y evidencia

Se inició Docker Desktop. El motor respondió como Docker 29.0.1; la imagen `contiker/contiki-ng:latest`, el repositorio (`E:\contiki-ng`), el submódulo Cooja y la caché de Gradle quedaron en el almacenamiento Docker del disco E (`E:\DockerData\docker_data.vhdx`). Se compiló y ejecutó Cooja headless desde esa imagen; las cinco simulaciones terminaron con `TEST OK`.

Los `.csc` de `solucion_rpl_udp/cooja/` tienen cuatro motes en línea (root en x=0; clientes x=40, 80 y 120; rango de transmisión 50 y de interferencia 100). En Lite se ejecutaron los tres periodos extremos y el control de 10 s; en Classic se ejecutó el control sobre la misma topología. Para el control, el ScriptRunner envió los comandos `ping` por el shell serie del root y guardó RTT ICMP, Hop Limit, muestras UDP y RTT UDP en `evidencia/rpl-udp-case-*.txt`. Las salidas completas de Cooja/compilación están en los `.log` de esa carpeta. El modo headless no genera una captura de pantalla del Timeline; las pérdidas se describen como consistentes con contención, no como un recuento visual de colisiones.

Para repetir una corrida individual en Windows con Docker iniciado:

```powershell
docker run --rm `
  --mount "type=bind,source=E:\contiki-ng,destination=/home/user/contiki-ng" `
  --mount "type=bind,source=E:\redes de sensores,destination=/workspace/task" `
  --mount "type=volume,source=contiki-gradle-cache,destination=/home/user/.gradle" `
  contiker/contiki-ng:latest bash -lc 'cd /home/user/contiki-ng/examples/rpl-udp-tarea3 && make TARGET=cooja clean && cd /home/user/contiki-ng/tools/cooja && ./gradlew run --args="--no-gui /workspace/task/solucion_rpl_udp/cooja/rpl-udp-case-1-lite.csc"'
```

## 2. `simple_udp` y los tres escenarios de conectividad

`simple_udp_register(&conn, local_port, peer_addr, peer_port, callback)` enlaza el socket UDP simplificado al puerto local, configura el filtro opcional de dirección/puerto remoto y registra la función llamada al llegar un datagrama coincidente. Con `peer_addr == NULL`, se aceptan direcciones de origen arbitrarias (sujeto al puerto remoto configurado). El callback recibe la conexión, dirección y puerto de origen/destino, puntero al payload y longitud; debe tratar `data` como bytes con longitud explícita, no asumir que termina en `\0`. El servidor usa 5678 y escucha clientes desde 8765.

`simple_udp_sendto(&conn, data, len, &dest)` envía esos `len` bytes desde el puerto local asociado a `conn` a la dirección IPv6 `dest` y el puerto remoto con que se registró. No abre una ruta IP externa ni realiza el eco automáticamente: el callback del servidor invoca `sendto` explícitamente para devolver el payload al remitente. RPL proporciona la ruta; `node_is_reachable()`/`get_root_ipaddr()` evitan mandar antes de que el nodo tenga ruta y dirección de raíz.

| Arquitectura | Dónde está el root/BR | Qué conecta | Uso |
|---|---|---|---|
| RPL autónoma (`rpl-udp` en Cooja) | Un mote simulado hace `root_start()` y también es servidor UDP | Solo motes de la simulación | Basta para probar DODAG, tráfico y aplicación. No necesita `tunslip6` porque no hay que sacar paquetes a una red del anfitrión. |
| BR embebido + `tunslip6` | El mote físico/simulado ejecuta el border router; `tunslip6` corre en Linux y crea `tun` | Malla IPv6 ↔ anfitrión/red externa | Hace falta si el host debe intercambiar IPv6 con la red de motes mediante SLIP. `tun` es la interfaz virtual del host; serial socket/SLIP conecta con el mote. |
| BR nativo | El proceso BR corre en el anfitrión; un mote hace de `slip-radio` | Radio del mote ↔ pila IP en el host | Alternativa para mantener la capa de radio en el mote y ejecutar las capas superiores en el host; no es la topología de un servidor UDP autónomo en Cooja. |

Por tanto `tunslip6` es necesario para exponer/alcanzar la malla desde el host a través de la interfaz tun; no es necesario para el ejemplo `rpl-udp` ni para que clientes y root simulados se comuniquen dentro de Cooja.

## 3. RPL Lite frente a RPL Classic, multi-salto y RTT

Se ejecutó Lite —predeterminado en Contiki-NG— y luego Classic (`MAKE_ROUTING=MAKE_ROUTING_RPL_CLASSIC`), limpiando y recompilando entre variantes. La cadena de nodos está separada 40 m con rango de transmisión 50 m, por lo que los clientes 2, 3 y 4 quedan a uno, dos y tres enlaces del root. El servidor incluye `serial-shell` y el ScriptRunner ejecutó `ping` desde ese root a cada IPv6 de cliente.

| Propiedad | RPL Lite (predeterminado) | RPL Classic |
|---|---|---|
| Modo descendente por defecto | Non-storing | Storing (sin multicast); puede cambiarse con `RPL_CONF_MOP` |
| Estado de rutas hacia abajo | La raíz conserva enlaces/DAO y arma source-routing header para llegar al destino | Los routers intermedios mantienen entradas de ruta descendente distribuidas mediante DAO |
| Memoria | Menos tabla descendente en nodos intermedios; más responsabilidad de la raíz | Más estado de rutas por nodo, que crece con destinos/subárboles |
| Paquete descendente | El source-routing header crece con la longitud del camino y puede fragmentarse o superar MTU | Normalmente el datagrama se reenvía salto a salto sin toda la lista de ruta en cada paquete; añade control/estado de rutas |
| Sensibilidad a tráfico | Source route puede aumentar tamaño/tiempo de aire en caminos largos | DAO y mantenimiento de tablas consumen tráfico/ram; forwarding requiere entradas instaladas |

### Ping desde el root y RTT de aplicación

El shell de Contiki-NG muestra el Hop Limit recibido bajo la etiqueta `ttl`. Con un root directamente vecino, el destino de un enlace respondió con 64; los destinos a dos y tres enlaces respondieron con 63 y 62. En esta topología, los routers intermedios descuentan el límite en los paquetes de respuesta.

| Cliente | Saltos | Lite: Hop Limit / ICMP RTT | Classic: Hop Limit / ICMP RTT | Lite: RTT UDP medio | Classic: RTT UDP medio |
|---|---:|---:|---:|---:|---:|
| Mote 2 | 1 | 64 / 70 ms | 64 / 76 ms | 51,67 ms (9 ecos) | 45,78 ms (9 ecos) |
| Mote 3 | 2 | 63 / 95 ms | 63 / 119 ms | 93,12 ms (8 ecos) | 95,44 ms (9 ecos) |
| Mote 4 | 3 | 62 / 152 ms | 62 / 125 ms | 149,86 ms (7 ecos) | 165,89 ms (9 ecos) |

`CLOCK_SECOND=1000` en Cooja, así que cada `RTT_ticks` equivale a 1 ms. El RTT UDP incluye ida, procesamiento/eco en el root y vuelta; las medias se calcularon de los ecos registrados en las corridas de control. En esas corridas, el cliente 2 registró `Tx/Rx/MissedTx=10/9/0` al décimo envío (el último eco aún podía estar en vuelo). Los RTT cambian con los eventos discretos y la ruta. Una sola ejecución por protocolo/topología no demuestra una ventaja general de Lite o Classic. En los casos de temporizador cero las marcas UDP se pisan, así que sus RTT no se comparan.

## 4. De `sky-websense.c` a Contiki-NG

El patrón portable a Contiki-NG es sensor → aplicación del cliente → datagrama UDP → sink/servidor. La lectura real se hace con la API del sensor habilitado para la plataforma (`SENSORS_SENSOR(...).value(...)` tras activarlo); para una topología Cooja genérica el ejemplo incluido produce un dato arbitrario reproducible. El formato transmite identificador del mote, secuencia y muestra. El servidor registra emisor, payload y contador en `App` y responde por eco. Se puede filtrar `App` en Mote output para recolectar las muestras; no se precisa un navegador.

Si se añade un visor web, debe residir en el sink/anfitrión: allí se agregan los registros y hay más RAM/almacenamiento y capacidad de HTTP. Un servidor HTTP por mote duplica código/estado en dispositivos constreñidos, aumenta tráfico/consumo y no aporta valor al envío de mediciones. Para integrar un host externo, entonces sí corresponde exponer la malla mediante un BR y, en el escenario SLIP, `tunslip6`.

## Referencias

- Contiki-NG, [RPL tutorial](https://docs.contiki-ng.org/en/develop/doc/tutorials/RPL.html).
- Contiki-NG, [RPL border router](https://docs.contiki-ng.org/en/develop/doc/tutorials/RPL-border-router.html).
- Código base `examples/rpl-udp` de Contiki-NG (checkout en `E:\contiki-ng`).
- Notebook original `TEL329-Tarea3-2026-v1.0.ipynb`.
