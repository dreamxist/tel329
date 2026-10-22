# TEL329 · Tarea 1 — Cooja, difusión, temporizadores y capa de enlace

Cuatro actividades sobre Contiki-NG + Cooja. Entrega: **jueves 27-ago-2026**.

> 📖 **¿Querés entender qué hace cada actividad sin leer código?**
> Andá a **[COMO-FUNCIONA.md](COMO-FUNCIONA.md)**: la explicación en simple, los
> datos reales de cada corrida y los problemas que aparecieron. Este README es la
> parte operativa (instalar, compilar, correr).

---

## Convención única del repo

Las cuatro actividades comparten una sola regla, definida en `nodos.h`:

> **El nodo 1 es siempre la estación base.** Los demás son sensores que le reportan.

Eso reproduce el modelo de la Lección 1 (nodos → BS, *upstream*) y evita tener
que reconfigurar el escenario entre actividades: los mismos motes en las mismas
posiciones sirven para las cuatro. `nodos.h` además le pone un nombre legible a
cada nodo para que el panel *Mote Output* se entienda sin descifrar direcciones.

---

## Estructura

```
tel329-tarea1/
├── nodos.h                       convención compartida (ID base + nombres)
├── COMO-FUNCIONA.md              la explicación en simple + los datos de las corridas
├── verificar-sintaxis.sh         chequeo rápido de las cuatro sin levantar Cooja
├── actividad1-broadcast-boton/   broadcast NullNet disparado por el botón
├── actividad2-unicast-etimer/    unicast + LED + intervalo creciente 2→20 s
├── actividad3-multicast-logico/  multicast emulado con ID de grupo en el payload
└── actividad4-tsch-manual/       slotframe TSCH de 10 ranuras construido a mano
```

Cada carpeta es un proyecto Contiki-NG independiente: el `.c`, su `Makefile`, el
`.csc` de la simulación y —solo la 4— un `project-conf.h`.

---

## Preparación: el symlink

**Un solo paso, y hay que hacerlo.** Desde la raíz del repo:

```bash
ln -s ~/contiki-ng contiki-ng     # o ~/Downloads/contiki-ng, donde lo tengas
```

Está en `.gitignore`, así que no se versiona.

**Por qué no basta con `$HOME`:** cuando Cooja compila un mote desde la GUI, no
lanza `make` con tu shell — arma un entorno mínimo con `PATH`, `CC` y `LIBNAME`,
**y nada más**. `$HOME` no existe ahí. Un `Makefile` que dependa de `$HOME`
compila bien en la terminal y falla al abrir la simulación, con un
`/Makefile.include: No such file or directory` que no dice nada.

Por eso los `Makefile` resuelven la ruta en este orden:

```make
CONTIKI ?= $(firstword $(wildcard $(CURDIR)/../contiki-ng $(HOME)/contiki-ng $(HOME)/Downloads/contiki-ng))
```

El symlink gana siempre y es relativo al repo, así que funciona en los dos casos.

## Compilar a mano

```bash
cd actividad1-broadcast-boton
make TARGET=cooja broadcast-boton
```

Con una ruta distinta: `make TARGET=cooja CONTIKI=/ruta/a/contiki-ng`.

---

## Metodología por actividad

### 1 · Broadcast por botón

El proceso **no usa temporizadores**: se bloquea en
`PROCESS_WAIT_EVENT_UNTIL(ev == button_hal_press_event)` y transmite recién al
soltarse el evento del botón. Es la forma más directa de mostrar que NullNet
entrega el paquete a la capa MAC sin periodicidad ni ruteo de por medio.

En NullNet, `NETSTACK_NETWORK.output(NULL)` **con destino NULL significa
broadcast**; con una dirección, unicast. Esa única línea separa la actividad 1
de la 2.

| Función | Qué hace |
|---|---|
| `send_broadcast()` | arma el payload y lo emite a todos los vecinos |
| `receive_packet()` | callback que NullNet invoca al recibir |
| `blink_led()` / `led_off_callback()` | enciende el LED y lo apaga con un `ctimer` |

### 2 · Unicast, LED e intervalo creciente

El intervalo sube 2 → 4 → … → 20 s y vuelve a 2 s. Toda esa lógica vive en una
función de tres líneas, aislada del proceso:

```c
static clock_time_t next_interval(clock_time_t current) {
  clock_time_t next = current + INTERVAL_STEP;
  return (next > INTERVAL_MAX) ? INTERVAL_MIN : next;
}
```

El requisito de que el receptor *«escriba su nombre en el paquete recibido»* se
cumple en `receive_packet()`: se concatena `" | leído por <nombre>"` al texto
recibido antes de imprimirlo. LED **verde** al transmitir, **rojo** al recibir.

Esta actividad usa a propósito **los dos temporizadores de evento del curso**:
`etimer` marca el ritmo de envío (despierta al proceso con un evento) y `ctimer`
apaga el LED (llama a una función). Sirve directo como evidencia de la teoría.

### 3 · Multicast lógico

El multicast es un servicio de la **capa de red**, y NullNet elimina esa capa.
Se emula entonces en la aplicación: se transmite en broadcast —que sí existe a
nivel de enlace— llevando un **ID de grupo dentro del payload**.

```c
typedef struct {
  uint8_t grupo;
  char texto[32];
} multicast_packet_t;
```

La regla vive en el receptor y es una sola comparación:
`if(rx->grupo != my_group()) { descartar; }`.

El emisor **alterna** el grupo destino en cada envío, así que en el mismo
*Mote Output* se ve a unos nodos imprimir `PROCESA` y a otros `DESCARTA` sobre el
mismo paquete. **El descarte se loguea a propósito**: es la evidencia de que el
filtro corre en el receptor y no en la red.

### 4 · Slotframe TSCH manual

Una sola regla genera todo el calendario:

> **El nodo N transmite únicamente en la ranura N−1.**

| Ranura | Quién | Qué |
|---|---|---|
| 0 | coordinador TX · resto RX | baliza (EB) — `LINK_TYPE_ADVERTISING` |
| 1 | nodo 2 → coordinador | datos |
| 2 | nodo 3 → coordinador | datos |
| 3 | nodo 4 → coordinador | datos |
| 4–9 | nadie | sin enlaces → **radio apagada** |

**Determinismo y ausencia de colisiones quedan demostrados por construcción**:
ningún par de nodos comparte ranura de transmisión, así que dos nodos no pueden
transmitir en el mismo instante. No es una propiedad que haya que medir — es una
consecuencia del reparto.

Tres decisiones que la actividad exige y que están explícitas en el código:

1. **Apagar el 6TiSCH Minimal Schedule** (`project-conf.h`). Si queda activo,
   Contiki-NG crea un slotframe con una única ranura compartida `TX|RX|SHARED` y
   los nodos vuelven a competir por el medio: el schedule manual queda tapado.
2. **Forzar el coordinador de tiempo**. Sin capa de red no hay quién lo elija:
   el nodo 1 llama `tsch_set_coordinator(1)` y es la fuente de reloj del resto.
3. **Enlace de balizas en la ranura 0**. Es el que más se olvida — sin un link
   `LINK_TYPE_ADVERTISING` en el coordinador, los demás nodos **nunca se
   sincronizan** y la red no llega a formarse.

El orden importa: `build_manual_schedule()` corre **antes** de
`tsch_set_coordinator()`, para que el coordinador ya tenga su calendario cuando
empiece a emitir balizas.

`print_schedule_matrix()` imprime la matriz tiempo × frecuencia de cada nodo al
arrancar. Ese log es la evidencia directa del informe: no hay que interpretar
nada, el nodo declara su propio calendario.

---

## Correr en Cooja

Cada carpeta trae su `.csc` ya armado: medio de radio **UDGM – Distance Loss**,
los motes posicionados dentro del rango y los paneles *Visualizer*, *LogListener*
y *Timeline* abiertos. Las rutas usan `[CONFIG_DIR]`, así que **funcionan sin
editar nada** aunque muevas el repo de lugar.

```bash
cd ~/contiki-ng/tools/cooja && ./gradlew run
# File → Open Simulation → elegir el .csc de la actividad
```

> Si al abrir falla la compilación del mote, casi siempre es el symlink: revisá
> que `contiki-ng` exista en la raíz del repo (ver arriba).

Cooja compila el firmware al abrir la simulación. Después: **Start**.

> La tarea pide guardar la simulación como `.csc` desde la GUI. Los incluidos
> son un punto de partida: al terminar, **File → Save simulation as…** y quedan
> con tus posiciones y tu configuración.

### Armarla desde cero (si prefieres seguir el enunciado al pie de la letra)

1. **File → New Simulation.** Medio de radio: **UDGM – Distance Loss**.
2. **Motes → Add Motes → Create New Mote Type → Cooja Mote.**
3. En *Contiki process source*, elegir el `.c` de la actividad. Compilar.
4. Agregar los motes en el plano. **El primero que se agrega es el nodo 1**
   (la estación base). Ponerlos dentro del círculo verde entre sí.
5. **File → Save simulation as…** en la carpeta de la actividad.

### Evidencia a capturar

| Actividad | Qué mostrar |
|---|---|
| 1 | *Mote Output* con el `TX broadcast` tras hacer clic derecho sobre el mote → **Click button on mote** |
| 2 | *Mote Output* con la escalera de intervalos (2, 4, 6 … 20, 2) y el `leído por` del receptor |
| 3 | *Mote Output* con `PROCESA` y `DESCARTA` sobre el mismo paquete en nodos distintos |
| 4 | La matriz impresa al arrancar **+ el panel Timeline**, donde el patrón de ranuras se repite idéntico en cada slotframe |

Para la 4, el *Timeline* es la evidencia que más pesa: se ve cada nodo
transmitiendo siempre en la misma posición del ciclo y las ranuras 4–9 vacías.

---

## Verificación

`./verificar-sintaxis.sh` corre `clang -fsyntax-only -Wall` sobre las cuatro
actividades usando los headers reales de Contiki-NG. No reemplaza a `make`
—no enlaza ni genera firmware— pero detecta en segundos errores de tipos,
includes y firmas de API sin levantar Cooja.

### Estado al 24-ago-2026

Las cuatro compilan con `make TARGET=cooja` y se corrieron en **Cooja headless**
(`--no-gui`, semilla `123456`) sobre la imagen oficial `contiker/contiki-ng`.
Los logs están en `evidencia/`.

| Actividad | Resultado |
|---|---|
| A1 broadcast | los 3 nodos se identifican; el broadcast sale al presionar el botón |
| A2 unicast | **24 TX / 24 RX** sin pérdidas · escalera 2→20 s completa · el receptor escribe su nombre |
| A3 multicast | 14 envíos → **21 `PROCESA` / 21 `DESCARTA`**, el reparto exacto que predice la membresía |
| A4 TSCH | **42 TX / 42 RX** · n2→ranura 1, n3→ranura 2, n4→ranura 3 · ranuras 4-9 dormidas |

### Dos trampas que costaron una corrida entera

**1 · `node_id` vale 0 en Cooja sin IPv6.** `node_id_init()` lo deriva de los dos
**últimos** bytes del link-layer address, pero la plataforma Cooja escribe el ID
del mote en los dos **primeros** cuando se compila sin IPv6 — el caso de NullNet.
Resultado: `node_id == 0` en todos los motes y cualquier comparación contra él
falla **en silencio**. En la actividad 4 eso dejaba la red sin coordinador y con
`my_timeslot()` en −1: cero transmisiones, sin un solo mensaje de error.

Por eso este repo usa `node_num()` (en `nodos.h`), que lee los bytes correctos.
Es la misma razón por la que los ejemplos oficiales de NullNet comparan
direcciones con `linkaddr_cmp()` en vez de mirar `node_id`.

**2 · Los `printf` van en ASCII puro.** Un `—` o una tilde salen corruptos en el
*Mote Output* (`ﾀﾔ`). Todos los literales del repo están sin acentos.
