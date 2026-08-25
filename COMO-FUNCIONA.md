# Cómo funciona esto — explicado simple

Documento de apoyo para entender la tarea sin leer el código, y para tener a mano
los datos de las corridas al escribir el informe o defenderla en clase.

Para instalar y correr, ver [README.md](README.md). Acá va el **qué** y el **por qué**.

---

## 1. Lo que pide la tarea

Cuatro ejercicios sobre **Contiki-NG** (el sistema operativo de los nodos) simulados
en **Cooja** (el simulador que viene con Contiki). Todos usan **NullNet**, que es
una capa de red mínima: la aplicación arma el paquete y lo entrega directo a la
capa de enlace, **sin ruteo de por medio**. Eso es a propósito — la tarea quiere
que se vea el enlace desnudo, sin que un protocolo de red haga magia por debajo.

| # | Lo que pide el enunciado |
|---|---|
| 1 | Broadcast que arranque con un **click de botón** |
| 2 | Unicast con **LED**, el receptor escribe su nombre, e intervalo que crece 2→20 s |
| 3 | **Multicast lógico**: como NullNet no tiene capa de red, emularlo con un ID de grupo |
| 4 | **TSCH**: slotframe de 10 ranuras armado a mano, sin el horario automático |

---

## 2. La idea que simplifica todo

Las cuatro actividades comparten **una sola regla**:

> ### El nodo 1 es siempre la estación base. Los demás le reportan.

Eso viene directo del modelo de la Lección 1 (los sensores mandan hacia la BS,
*upstream*). Y tiene una ventaja práctica grande: **armás el escenario en Cooja una
sola vez** — mismos motes, mismas posiciones — y te sirve para las cuatro. Lo único
que cambia entre actividades es el firmware que cargás.

Esa regla vive en `nodos.h`, que además le pone nombre a cada nodo (Pancho,
Francisco, Ignacio, Berioska) para que el panel *Mote Output* se lea solo, sin
tener que descifrar direcciones tipo `0200.0000.0000.0000`.

---

## 3. Actividad por actividad

### 1 · Broadcast por botón

**La idea:** el nodo no hace nada. Se queda dormido esperando el evento del botón.
Cuando lo apretás, manda un mensaje a todos sus vecinos.

**Por qué así:** sin temporizadores de por medio queda clarísimo que NullNet entrega
el paquete a la capa de enlace y ya — no hay periodicidad ni ruteo. El proceso
entero es una línea que espera (`PROCESS_WAIT_EVENT_UNTIL`) y otra que transmite.

**El detalle que hay que saber:** en NullNet, mandar con destino `NULL` significa
"a todos" (broadcast); con una dirección, es a uno solo (unicast). **Esa única
diferencia** separa la actividad 1 de la 2.

**Data de la corrida:**

```
*** click de boton en el nodo 2
5236000 n2  TX broadcast: "hola desde Francisco"
5280152 n1  RX broadcast: "hola desde Francisco" desde 0200.0000.0000.0000
5280152 n3  RX broadcast: "hola desde Francisco" desde 0200.0000.0000.0000
```

> **1 click → 1 transmisión → 2 recepciones.** Los otros dos nodos lo reciben en el
> mismo instante (`5280152`), que es lo que uno espera de un broadcast.

---

### 2 · Unicast con intervalo creciente

**La idea:** los sensores le mandan mensajes al nodo 1, pero cada vez más espaciados:
2 s, 4 s, 6 s… hasta 20 s, y ahí vuelve a 2 s.

**Cómo se resolvió:** toda esa lógica es una función de tres líneas, separada del
resto para que se entienda de una:

```c
static clock_time_t next_interval(clock_time_t current) {
  clock_time_t next = current + INTERVAL_STEP;
  return (next > INTERVAL_MAX) ? INTERVAL_MIN : next;
}
```

**El requisito raro del enunciado** ("los receptores deben escribir su nombre en el
paquete recibido") se cumple al recibir: se le concatena `" | leido por <nombre>"`
al texto y se imprime.

**Bonus para el informe:** esta actividad usa **los dos temporizadores de la teoría**
a propósito. El `etimer` marca el ritmo de envío (al expirar **manda un evento** al
proceso) y el `ctimer` apaga el LED (al expirar **llama a una función**). Es la
lámina de timers demostrada en código.

**Data de la corrida (120 s simulados):**

| Métrica | Valor |
|---|---|
| Transmisiones | **24** |
| Recepciones | **24** |
| Pérdidas | **0** |
| Intervalos vistos | 2, 4, 6, 8, 10, 12, 14, 16, 18, 20 s (y reinicia) |

```
n2  nodo 2 (Francisco) transmite hacia la estacion base
n1  nodo 1 (Pancho) es la estacion base - solo recibe
n1  RX unicast: "msg 0 de Francisco | leido por Pancho" desde 0200.0000.0000.0000
```

---

### 3 · Multicast lógico

**El problema:** el multicast es un servicio de la **capa de red**, y NullNet
justamente elimina esa capa. O sea: multicast de verdad **no se puede**.

**La solución:** emularlo. Se manda en broadcast (que sí existe a nivel de enlace)
pero llevando un **número de grupo dentro del mensaje**. El que lo recibe aplica una
regla de una línea:

> ¿el grupo del paquete es mi grupo? → lo proceso.
> ¿no? → lo boto.

**Detalle que suma nota:** el descarte **se imprime a propósito**. Es la evidencia de
que el filtro corre en el receptor y no en la red — si no se logueara, no habría
forma de mostrar que el mecanismo funciona.

El emisor va **alternando** el grupo destino, así que en el mismo panel se ve el
mismo paquete siendo procesado por unos y descartado por otro:

```
n1  TX broadcast dirigido al grupo A: "aviso 0 para el grupo A"
n2  PROCESA: grupo A - "aviso 0 para el grupo A"
n3  PROCESA: grupo A - "aviso 0 para el grupo A"
n4  DESCARTA: paquete del grupo A y yo soy del grupo B
```

**Data de la corrida — y por qué los números importan:**

| Métrica | Valor |
|---|---|
| Envíos | **14** (7 al grupo A, 7 al grupo B) |
| `PROCESA` | **21** |
| `DESCARTA` | **21** |

Los receptores son los nodos 2 y 3 (grupo A) y el nodo 4 (grupo B). Entonces:

```
PROCESA  = 7 × 2 (grupo A) + 7 × 1 (grupo B) = 21  ✔
DESCARTA = 7 × 1            + 7 × 2          = 21  ✔
```

> Que los números den **exactos** prueba que el filtro corre en cada receptor y no
> procesa ni descarta de más. Es un argumento más fuerte que "se ve que funciona".

---

### 4 · TSCH — slotframe manual

Es la única actividad con chicha real.

**El problema:** si todos los nodos hablan cuando quieren, chocan entre sí y se
pierden mensajes. En una fábrica o una ciudad inteligente eso no sirve.

**Qué hace TSCH:** parte el tiempo en turnos (*timeslots*) agrupados en un ciclo
(*slotframe*). Cada nodo sabe en qué turno le toca hablar, en cuál escuchar, y en
cuáles **apagar la radio** (que es de donde sale el ahorro de batería).

**La regla que inventé para que sea simple de explicar y de defender:**

> ## El nodo N habla solo en el turno N−1

De ahí sale el calendario completo, sin tablas que memorizar:

| Turno | Quién | Qué hace |
|---|---|---|
| 0 | coordinador manda, el resto escucha | la **baliza** (el "tic" del reloj de la red) |
| 1 | nodo 2 | manda sus datos |
| 2 | nodo 3 | manda sus datos |
| 3 | nodo 4 | manda sus datos |
| 4–9 | nadie | **radio apagada** |

**Lo mejor para el informe:** no hay que medir nada para probar que no hay choques.

> Si cada nodo tiene **su propio turno**, es imposible que dos hablen al mismo tiempo.
> La ausencia de colisiones es **una consecuencia del reparto**, no un resultado
> experimental.

**Las tres cosas que hay que hacer sí o sí** (y que la actividad pide):

1. **Apagar el horario automático** (6TiSCH Minimal). Si queda prendido, Contiki crea
   un turno único compartido y los nodos vuelven a competir: tu calendario manual
   queda tapado y no demostrás nada.
2. **Nombrar el coordinador a mano.** Sin capa de red no hay quién lo elija, así que
   el nodo 1 se declara fuente de tiempo del resto.
3. **Poner la baliza en el turno 0.** Es la que más se olvida: sin ese enlace, los
   otros nodos **nunca se sincronizan** y la red no llega a existir.

También importa el orden: primero se arma el calendario, **después** se declara el
coordinador. Así ya tiene su agenda lista cuando empieza a emitir balizas.

**Data de la corrida:**

Cada nodo imprime su propia agenda al arrancar (esto es evidencia directa, no hay
que interpretar nada):

```
n1  slotframe de 10 ranuras, channel offset 0
n1    ranura  0 | canal 0 | TX baliza EB
n1    ranura  1 | canal 0 | RX de un sensor
n1    ranura  2 | canal 0 | RX de un sensor
n1    ranura  3 | canal 0 | RX de un sensor
n1    ranura  4 | canal 0 | dormir (radio apagada)
      ... hasta la 9 ...
n1  nodo 1 (Pancho): soy el coordinador de tiempo
```

Y cada sensor usó **exactamente** el turno que le tocaba:

| Nodo | Turno usado |
|---|---|
| n2 | ranura 1 |
| n3 | ranura 2 |
| n4 | ranura 3 |

| Métrica | Valor |
|---|---|
| Transmisiones | **42** |
| Recepciones en el coordinador | **42** |
| Intentos sin estar asociado | **0** |

---

## 4. Cómo se hizo (el proceso)

No alcanza con que el código "se vea bien". El proceso fue:

1. **Leer el fuente real de Contiki-NG** antes de escribir nada, para no inventar
   firmas de funciones ni nombres de constantes.
2. **Chequeo de sintaxis** (`verificar-sintaxis.sh`) contra los headers reales —
   detecta errores de tipos y de API en segundos, sin levantar Cooja.
3. **Compilación real** con `make TARGET=cooja`, que produce el firmware.
4. **Simulación headless** en Cooja (`--no-gui`, semilla fija `123456`) sobre la
   imagen oficial `contiker/contiki-ng`, para obtener los logs de arriba.
5. **Validación de contenido**, no solo de que el programa no se caiga.

El paso 5 resultó ser el importante, y por una razón concreta: **la primera tanda de
simulaciones dio "test OK" en las cuatro, y la actividad 4 no transmitía absolutamente
nada.** El script solo verificaba que Cooja no crasheara. Desde entonces cuenta
transmisiones, recepciones y descartes — si algo no ocurre, se nota.

---

## 5. Los tres problemas que aparecieron

### El grave: `node_id` vale 0 en Cooja

Contiki tiene una variable estándar para saber "qué número de nodo soy". **En Cooja
con NullNet, vale 0 en todos los nodos.**

La causa: esa variable se calcula leyendo los **dos últimos** bytes de la dirección
de enlace, pero la plataforma Cooja escribe el ID del mote en los **dos primeros**
cuando se compila sin IPv6 — que es justo el caso de NullNet.

Lo peligroso no es el error, es que **falla en silencio**: nadie se reconoce como
estación base, y en la actividad 4 el cálculo del turno daba −1. Resultado: cero
mensajes, cero advertencias, y un test que decía "OK".

**Solución:** `node_num()` en `nodos.h`, que lee los bytes correctos. Es la misma
razón por la que los ejemplos oficiales de NullNet comparan direcciones con
`linkaddr_cmp()` en lugar de usar `node_id`.

### `$HOME` no existe cuando compila Cooja

Cooja no lanza `make` con tu shell: arma un entorno con `PATH`, `CC` y `LIBNAME`, y
nada más. Un Makefile que dependa de `$HOME` **compila bien desde la terminal y
falla al abrir la simulación**, con un error que no explica nada.

**Solución:** la ruta se resuelve relativa al repo (de ahí el symlink `contiki-ng`
que pide el README), con `$HOME` solo como respaldo.

### Los mensajes van en ASCII puro

Un guión largo o una tilde dentro de un `printf` sale corrupto en el *Mote Output*
(`—` aparecía como `ﾀﾔ`). Todos los textos del repo están sin acentos.

---

## 6. Glosario mínimo

| Término | En simple |
|---|---|
| **Cooja** | El simulador de Contiki. Su gracia: *emula* hardware real, así que el mismo código corre en el simulador y en un nodo físico sin tocar nada |
| **NullNet** | Capa de red mínima. La app entrega el paquete directo al enlace, sin ruteo |
| **Mote** | Un nodo sensor |
| **Broadcast / Unicast** | A todos / a uno solo |
| **TSCH** | Protocolo de enlace que reparte el tiempo en turnos y va saltando de canal |
| **Slotframe** | El ciclo de turnos que se repite (acá, de 10) |
| **Baliza (EB)** | El mensaje del coordinador que mantiene a todos sincronizados |
| **etimer / ctimer** | Al expirar: manda un evento al proceso / llama a una función |
