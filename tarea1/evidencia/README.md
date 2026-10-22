# Evidencia de referencia

Salida real de las simulaciones corridas en Cooja headless sobre Contiki-NG
(`--no-gui`, semilla fija `123456`), con el escenario de cada `.csc` del repo.

**Esto no es la entrega.** Es la referencia de "así se ve cuando funciona":
sirve para comparar si al correrlo en tu máquina algo sale distinto. La
evidencia de la tarea son tus capturas del *Mote Output* y del *Timeline*.

| Archivo | Qué demuestra |
|---|---|
| `broadcast-boton.log` | un click de botón en el nodo 2 → 1 broadcast → recibido por los otros 2 nodos |
| `unicast-etimer.log` | 24 TX / 24 RX sin pérdidas · escalera de intervalos 2→20 s completa · el receptor escribe su nombre en el paquete |
| `multicast-logico.log` | 14 envíos alternando grupo → 21 `PROCESA` y 21 `DESCARTA` · el mismo paquete procesado por unos y descartado por otro |
| `tsch-manual.log` | La matriz de los 4 nodos · cada sensor transmite en su ranura propia (n2→1, n3→2, n4→3) · 42 TX / 42 RX · ranuras 4-9 dormidas |

## El conteo de A3 no es casual

14 transmisiones alternando grupo = 7 al grupo A + 7 al grupo B.
Los receptores son los nodos 2 y 3 (grupo A) y el nodo 4 (grupo B):

```
PROCESA  = 7 × 2 (grupo A) + 7 × 1 (grupo B) = 21
DESCARTA = 7 × 1            + 7 × 2          = 21
```

Que los números salgan exactos confirma que el filtro por ID de grupo corre en
cada receptor y no descarta ni procesa de más.

## A4: el determinismo se lee en el log

```
n2  TX en ranura 1
n3  TX en ranura 2
n4  TX en ranura 3
```

Ninguna ranura está compartida, así que dos nodos no pueden transmitir en el
mismo instante. No es un resultado medido: es la consecuencia del reparto.
