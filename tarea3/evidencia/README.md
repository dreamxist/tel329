# Evidencia de la Tarea 3

Acá van los logs de las corridas y las capturas. Mismo criterio que la Tarea 1:
**esto no es la entrega**, es la referencia de "así se ve cuando funciona".

Lo que falta juntar:

| Archivo esperado | Qué tiene que demostrar |
|---|---|
| `a1-base.log` | el intervalo en `10000 ticks` y envíos cada 10 s por cliente |
| `a1-1e-5.log` · `a1-1e-10.log` · `a1-1e-20.log` | el intervalo en `0 ticks` y el flood: cola llena, RPL sin converger, `missed_tx` subiendo |
| `a3-lite.log` · `a3-classic.log` | el TTL de cada nodo (64 − saltos), el RTT de aplicación y el `Tx/Rx/MissedTx` |
| `a3-ping-lite.txt` · `a3-ping-classic.txt` | salida del `ping` desde el shell de la raíz, con ttl y delay |
| `a3-routes-lite.txt` · `a3-routes-classic.txt` | `routes` en la raíz y en el nodo 3 — en Classic el intermedio tiene entradas, en Lite no |
| `a4-sensores.csv` | las líneas `SENSOR,id,seq,temp,ttl` filtradas del Mote output |

Capturas de pantalla aparte: Timeline de A1 (base y degenerado), Timeline de A3
en los dos modos, y el filtro aplicado en el Mote output de A4.

Los `.log` de esta carpeta sí se versionan (hay una excepción en el
`.gitignore` de la raíz).
