# TEL329 · Redes de Sensores Inalámbricas — 2026

Repo del ramo. Una carpeta por entrega, cada una autocontenida.

| Carpeta | Qué es | Entrega |
|---|---|---|
| [tarea1/](tarea1/) | Cooja, difusión, temporizadores y capa de enlace (NullNet + TSCH manual) | jue 27-ago-2026 ✅ |
| [tarea3/](tarea3/) | uIP y RPL (`rpl-udp`, RPL Lite vs. Classic) | jue 22-oct-2026 |

La Tarea 2 no está acá: su enunciado existe (`TEL329-Tarea2-2026-v1.0.ipynb`, del
22-sep) pero no se trabajó en este repo. Revisar en Aula si quedó entregada — las
4 tareas prácticas son **requisito para rendir el certamen** del 26-nov.

## Preparación: el symlink a Contiki-NG

**Un solo paso, y hay que hacerlo.** Desde la raíz del repo:

```bash
ln -s ~/contiki-ng contiki-ng     # o donde lo tengas clonado
```

Está en `.gitignore`, así que no se versiona.

**Por qué no basta con `$HOME`:** cuando Cooja compila un mote desde la GUI no
lanza `make` con tu shell — arma un entorno mínimo con `PATH`, `CC` y `LIBNAME`,
**y nada más**. `$HOME` no existe ahí. Un `Makefile` que dependa de `$HOME`
compila bien en la terminal y falla al abrir la simulación, con un
`/Makefile.include: No such file or directory` que no dice nada.

Por eso los `Makefile` resuelven la ruta relativa al repo primero y dejan `$HOME`
como respaldo para la línea de comandos. Detalle en [tarea1/README.md](tarea1/README.md).

## Las tres trampas del entorno

Las descubrió la Tarea 1 corriendo las simulaciones, no leyéndolas. Ninguna tira
error. Están documentadas en [tarea1/COMO-FUNCIONA.md](tarea1/COMO-FUNCIONA.md):

1. **`node_id` vale 0 en Cooja sin IPv6** — la plataforma escribe el ID del mote
   en los dos *primeros* bytes del link-layer address, y `node_id_init()` lo lee
   de los dos *últimos*. Aplica solo a builds sin IPv6 (NullNet, o sea la Tarea
   1): **con RPL/IPv6 `node_id` es correcto** y no hace falta el workaround.
2. **Cooja compila sin `$HOME`** — ver arriba.
3. **Los `printf` van en ASCII puro** — un guión largo o una tilde salen
   corruptos en el *Mote Output*.
