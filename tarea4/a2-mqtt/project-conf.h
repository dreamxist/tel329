/*
 * TEL329 - Tarea 4, Actividad 2: cliente MQTT sobre RPL Lite.
 */
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* MQTT va sobre TCP; uIP lo trae apagado por defecto. */
#define UIP_CONF_TCP 1

/* El enunciado cita el estandar OASIS 3.1.1; Contiki-NG usa 3.1 por defecto. */
#define MQTT_CONF_VERSION MQTT_PROTOCOL_VERSION_3_1_1

/* Broker: el host del otro lado de tunslip6 (fd00::1 en tun0). */
#define A2_BROKER_IP   "fd00::1"
#define A2_BROKER_PORT 1883
#define A2_TOPIC       "usm/tel329/temp"

/* Cada cuanto publica cada cliente, en segundos. */
#ifndef A2_INTERVALO_S
#define A2_INTERVALO_S 5
#endif

/* Keep alive MQTT = 3 periodos de publicacion, como en el ejemplo. */
#define A2_KEEP_ALIVE_S (3 * A2_INTERVALO_S)

/*
 * Un PUBLISH completo (hdr 2 + topic 17 + mid 2 + payload ~41) entra en 64 B,
 * y 64 B de TCP caben en una sola trama 802.15.4 sin fragmentar en 6LoWPAN.
 * El ejemplo usa 32, que partiria cada PUBLISH en dos segmentos TCP.
 */
#define A2_MAX_TCP_SEGMENT 64

/* Logs: RPL en INFO para ver el DODAG formarse; el resto en WARN. */
#define LOG_CONF_LEVEL_RPL   LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_IPV6  LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC   LOG_LEVEL_WARN

#endif /* PROJECT_CONF_H_ */
