# TEL329 · Tarea 3 — uIP y RPL

Tres carpetas, una por actividad que necesita código. Cada una es un proyecto
Contiki-NG independiente: sus dos `.c` (servidor y cliente), su `Makefile`, su
`project-conf.h` y el `.csc` de la simulación.

```
tarea3/
├── respuestas.md              ← el borrador de las 4 respuestas (empezar acá)
├── nodos.h                    convención compartida: el nodo 1 es la raíz del DODAG
├── a1-etimer-degenerado/      A1 · el periodo del etimer (base + 1e-5 + 1e-10 + 1e-20)
├── a3-lite-vs-classic/        A3 · RPL Lite vs Classic en cadena de 4 saltos
├── a4-sensor-websense/        A4 · sensado hacia el sink, registro parseable
└── evidencia/                 logs y capturas de las corridas
```

**La actividad 2 no tiene carpeta**: es de análisis, y está respondida completa
en [respuestas.md](respuestas.md).

## Preparación

Una sola vez, desde la raíz del repo (un nivel arriba de acá):

```bash
ln -s ~/contiki-ng contiki-ng
```

Si no tenés Contiki-NG clonado:

```bash
git clone --depth 1 --recurse-submodules --shallow-submodules \
    https://github.com/contiki-ng/contiki-ng.git ~/contiki-ng
# si falla en gecko_sdk por git-lfs, no importa: para Cooja alcanza con
git -C ~/contiki-ng submodule update --init --depth 1 --recursive tools/cooja
```

Para compilar y correr sin instalar nada más está la imagen oficial:

```bash
docker pull contiker/contiki-ng
```

## Compilar

Desde la carpeta de una actividad:

```bash
make TARGET=cooja
```

En la imagen de Docker, pasando la ruta y desactivando ccache (si no, tira
`ccache: error: Permission denied`):

```bash
docker run --rm -e CCACHE_DISABLE=1 \
  -v ~/contiki-ng:/home/user/contiki-ng \
  -v "$PWD":/work -w /work/tarea3/a1-etimer-degenerado \
  contiker/contiki-ng make TARGET=cooja CONTIKI=/home/user/contiki-ng
```

### ⚠️ `make distclean` al cambiar de variante

`make` **no detecta un cambio de `CFLAGS` ni de `MAKE_ROUTING`**: si cambiás de
caso o de routing sin limpiar, el binario anterior se queda pegado y la corrida
miente. Siempre:

```bash
make distclean TARGET=cooja && make TARGET=cooja <la variante nueva>
```

## Correr cada actividad

### A1 — el periodo del `etimer`

Cuatro casos, elegidos al compilar:

```bash
make distclean TARGET=cooja && make TARGET=cooja CASO=base     # 10 * CLOCK_SECOND
make distclean TARGET=cooja && make TARGET=cooja CASO=1e-5
make distclean TARGET=cooja && make TARGET=cooja CASO=1e-10
make distclean TARGET=cooja && make TARGET=cooja CASO=1e-20
```

El cliente imprime al arrancar en cuántos ticks quedó el intervalo — ese número
es la respuesta de la actividad. El `.csc` trae `CASO=base` en el comando de
compilación del tipo de mote: para los otros casos, o se edita esa línea en la
GUI (*Mote type* → editar comando) o se compila a mano antes de abrir la
simulación.

`speedlimit` está en 20 para el caso base. Para los degenerados conviene
bajarlo a 1, o la interfaz no da abasto.

### A3 — Lite vs Classic

```bash
make distclean TARGET=cooja && make TARGET=cooja                                   # Lite
make distclean TARGET=cooja && make TARGET=cooja MAKE_ROUTING=MAKE_ROUTING_RPL_CLASSIC
```

Los cinco nodos están **en línea a 40 m con alcance de 50 m**: cada uno alcanza
solo a sus vecinos, así que la cadena llega a 4 saltos. Es lo que hace que la
comparación mida algo — con todos a un salto, *storing* y *non-storing* se
portan igual.

El `ping` sale del **shell de la raíz**: click derecho en el mote 1 → *Mote
tools for Contiki* → *Serial Socket*, o la ventana *Serial*. El shell arranca
solo, no hay que llamarlo desde la aplicación. Comandos útiles: `ping <addr>`
(imprime ttl y delay) y `routes` (la tabla, que es la diferencia visible entre
Lite y Classic).

Las direcciones a pinguear las imprime cada cliente al tenerlas.

### A4 — sensado hacia el sink

```bash
make distclean TARGET=cooja && make TARGET=cooja
```

En el *Mote output*, filtrar por `SENSOR`. Cada muestra sale como
`SENSOR,<id>,<seq>,<temp>,<ttl>`, lista para pegar en una planilla.

## Qué está hecho y qué falta

| Actividad | Código | Respuesta escrita | Corrida + evidencia |
|---|---|---|---|
| A1 · periodo del etimer | ✅ compila | ✅ conclusión y el porqué | ❌ faltan los 4 logs y el Timeline |
| A2 · `simple_udp` y `tunslip6` | — (no lleva) | ✅ completa | — (no lleva) |
| A3 · Lite vs Classic | ✅ compila en los dos modos | ✅ método + lo deducible (TTL) | ❌ faltan las dos tablas de medición |
| A4 · sensor → sink | ✅ compila | ✅ completa | ❌ falta la serie `SENSOR,…` |

Todo lo pendiente está marcado **[FALTA]** en [respuestas.md](respuestas.md).

### Reparto sugerido

Lo que queda es **correr y medir**, no programar. Se puede tomar por actividad
sin pisarse, porque cada carpeta es independiente:

- **A1** — correr los 4 casos, capturar Timeline y Mote output. Es mecánico: el
  resultado ya se sabe, hay que documentarlo.
- **A3** — la más larga: dos compilaciones, esperar convergencia, `ping` a 4
  nodos por escenario, `routes` en raíz e intermedio, y llenar las dos tablas.
- **A4** — correr unos minutos, filtrar `SENSOR`, graficar la serie.

El informe de A2 ya está escrito; solo queda verificar el detalle del border
router nativo que quedó marcado en `respuestas.md`.
