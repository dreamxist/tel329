# Evidencia de la Tarea 3

Acá van los logs de las corridas y las capturas. Mismo criterio que la Tarea 1:
**esto no es la entrega**, es la referencia de "así se ve cuando funciona".

## Ya está (Actividad 1)

Generados con `../a1-etimer-degenerado/correr-casos.sh`, Cooja headless, semilla
fija `123456`, cuatro nodos a un salto.

| Archivo | Qué demuestra |
|---|---|
| `a1-base.log` | intervalo en `10000 ticks`; 3 de 3 clientes mandan cada 10 s; 45 recibidos, 0 perdidos |
| `a1-1e-5.log` · `a1-1e-10.log` · `a1-1e-20.log` | intervalo en `0 ticks`. **Los tres difieren en 3 líneas sobre 4004**: las que imprimen el nombre del caso. Es la prueba de que son una sola corrida con tres etiquetas — comparar con `diff` |
| `a1-1e-5-largo.log` | el mismo caso con el corte de líneas en 25.000, para pasar la convergencia: el nodo 2 entra al DODAG a los 19,47 s tras 32.000 vueltas, y aparecen las 5.410 `Neighbor queue full` |

Los tres logs de 4004 líneas se cortan por cantidad, no por tiempo: el límite
está en `MAX_LINEAS` dentro de `../a1-etimer-degenerado/js/volcar-log.js`.
Subirlo es lo que produjo la corrida larga.

## Lo que falta juntar

| Archivo esperado | Qué tiene que demostrar |
|---|---|
| `a3-lite.log` · `a3-classic.log` | el TTL de cada nodo (64 − saltos), el RTT de aplicación y el `Tx/Rx/MissedTx` |
| `a3-ping-lite.txt` · `a3-ping-classic.txt` | salida del `ping` desde el shell de la raíz, con ttl y delay |
| `a3-routes-lite.txt` · `a3-routes-classic.txt` | `routes` en la raíz y en el nodo 3 — en Classic el intermedio tiene entradas, en Lite no |
| `a4-sensores.csv` | las líneas `SENSOR,id,seq,temp,ttl` filtradas del Mote output |

Capturas de pantalla aparte: Timeline de A1 (base y degenerado), Timeline de A3
en los dos modos, y el filtro aplicado en el Mote output de A4.

Los `.log` de esta carpeta sí se versionan (hay una excepción en el
`.gitignore` de la raíz).
