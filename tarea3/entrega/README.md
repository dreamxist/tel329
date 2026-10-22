# La entrega de la Tarea 3

Lo que armó **Ignacio**, tal como lo pasó el 22-oct-2026. **Los tres archivos
están sin tocar**; cualquier corrección va acá en el README o en
[../respuestas.md](../respuestas.md), no editándolos por encima.

| Archivo | Qué es |
|---|---|
| `TEL329-Tarea3-2026-v1.0.ipynb` | el notebook del enunciado con las cuatro actividades resueltas — **es la entrega** |
| `Solucion_Tarea3.md` | el informe largo: tablas de medición, decisiones de implementación y el registro de ejecución |
| `Pendientes_Tarea3.md` | lo que él mismo deja marcado como evidencia faltante |

Corrió **cinco escenarios Cooja headless**, todos con `TEST OK`: RPL Lite con el
control de 10 s y con los tres periodos sub-tick, más RPL Classic con el control
sobre la misma topología. Cadena de cuatro motes (raíz en x=0, clientes en 40,
80 y 120 m, alcance 50 m), con el `ping` disparado desde el shell serie de la
raíz por el ScriptRunner.

## Verificado al subirlo

- **JSON válido**, 14 celdas.
- **El material original está completo**: las 11 celdas con contenido del
  enunciado se conservan, verificadas una por una contra el `.ipynb` original de
  iCloud. Solo falta la celda final `## Fin de la Tarea!`.
- ⚠️ **Las 5 celdas de código del enunciado quedaron como markdown.** El
  contenido está —dentro de bloques con cerco— pero el tipo de celda cambió:
  el notebook ahora es markdown de punta a punta. Si la pauta pide conservar la
  estructura del notebook original, hay que devolverlas a `code`.

## Lo que falta pedirle

Su propio informe referencia archivos que **no llegaron**, y son el respaldo de
las cifras de la Actividad 3:

- `evidencia/rpl-udp-case-*.txt` — las salidas con RTT ICMP, Hop Limit, muestras
  UDP y RTT UDP.
- los `.log` de compilación y ejecución de las cinco corridas.
- `solucion_rpl_udp/` — sus fuentes y sus `.csc`.

Todo eso vive en su máquina (`E:\redes de sensores`). Sin esos archivos las
tablas del informe no tienen evidencia detrás, que es justo lo que la rúbrica
mira.

Ojo que su implementación **no es la de este repo**: él usa `PERIOD_CASE` y sus
propios `.csc`, y acá las carpetas `a1`/`a3`/`a4` usan `CASO` y `MAKE_ROUTING`.
Las dos llegan al mismo resultado; no hay que unificarlas, pero tampoco mezclar
cifras de una con escenarios de la otra sin decirlo.
