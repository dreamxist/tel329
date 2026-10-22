# Pendientes para dejar lista la entrega — Tarea 3

## Ya completado

- El notebook `TEL329-Tarea3-2026-v1.0.ipynb` contiene las respuestas a las cuatro actividades, conserva el material original y es JSON válido.
- Se compilaron y ejecutaron cinco escenarios Cooja: RPL Lite con control de 10 s y con los tres intervalos sub-tick; RPL Classic con el control de 10 s. Los cinco terminaron con `TEST OK`.
- Las mediciones de UDP, ping IPv6, Hop Limit y RTT están en `evidencia/rpl-udp-case-*.txt`; los logs de compilación/ejecución están en los `.log` correspondientes.
- El ejemplo usa una muestra sensada arbitraria y determinista, tal como permite la actividad; no depende de un sensor físico.

## Evidencia que todavía podría agregarse

### 1. Capturas gráficas de Cooja

Las pruebas se ejecutaron en modo headless. Por eso no hay capturas de pantalla de la topología, Timeline, Mote Output ni Radio Messages. Si la entrega exige evidencia visual, abrir los escenarios `.csc` de `solucion_rpl_udp/cooja/` en Cooja con interfaz gráfica y guardar las capturas en `evidencia/`. Para respaldar el análisis de colisiones, la captura debería mostrar las transmisiones superpuestas en Timeline o Radio Messages. Los contadores `Tx/Rx/MissedTx` por sí solos no identifican la causa de cada pérdida.

### 2. Túnel IPv6 con `tunslip6` — solo si se exige ejecutar los pasos introductorios

No se ejecutó un border router conectado a `tun0`: las simulaciones medidas usan el ejemplo RPL-UDP autónomo, donde el servidor es la raíz y no hace falta `tunslip6`. Si también deben demostrarse los pasos 1 y 2 del notebook, falta correr el escenario separado de border router, verificar `ifconfig tun0` y hacer `ping6` al router; guardar sus salidas o capturas en `evidencia/`.

## Alcance de la comparación

Los RTT y Hop Limit de Lite y Classic corresponden a una corrida de control por protocolo en la misma topología de cuatro motes. Sirven como resultados observados de estos ensayos, pero no como comparación estadística general. Repetir con varias semillas y recopilar los contadores de todos los clientes sería una ampliación opcional si se pide mayor rigor.

## Ubicación

Notebook, informe, escenarios y evidencia están en `E:\redes de sensores`. La única limitación actual de evidencia es la falta de capturas gráficas y, si la pauta lo requiere por separado, la demostración del túnel `tunslip6`.
