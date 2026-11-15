# Tiempos de respuesta CoAP (Actividad 3, mitad CoAP)

Fuente: `a1-coap.log` (Cooja headless, tiempo simulado). 129 peticiones POST confirmables, 129 con respuesta 2.04, 0 timeouts, 0 rondas saltadas por peticion en vuelo, 0 en vuelo al cortar.

RTT = tiempo simulado de la linea RX menos el de la linea TX del mismo (cliente, seq).

| cliente | saltos | n | media ms | mediana ms | p95 ms | min ms | max ms | timeouts | saltadas |
|---|---|---|---|---|---|---|---|---|---|
| n2 | 1 | 26 | 51.1 | 47.3 | 81.8 | 13.5 | 87.8 | 0 | 0 |
| n3 | 1 | 26 | 43.0 | 47.3 | 60.8 | 17.8 | 64.8 | 0 | 0 |
| n4 | 1 | 27 | 51.0 | 46.8 | 89.8 | 17.8 | 90.8 | 0 | 0 |
| n5 | 2 | 25 | 94.2 | 95.4 | 137.4 | 44.4 | 138.4 | 0 | 0 |
| n6 | 2 | 25 | 99.8 | 102.4 | 139.4 | 55.4 | 141.4 | 0 | 0 |
| **global** | 1-2 | 129 | 67.2 | 61.8 | 121.4 | 13.5 | 141.4 | 0 | 0 |

Primer envio por cliente (ruta a la raiz disponible): n2 a los 22.7 s, n3 a los 24.6 s, n4 a los 20.7 s, n5 a los 36.9 s, n6 a los 42.3 s

pcap: 820 tramas 802.15.4 (35211 bytes de trama), linktype 195, de 4.28 s a 282.36 s.
