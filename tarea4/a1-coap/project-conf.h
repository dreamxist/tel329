/*
 * TEL329 - Tarea 4, Actividad 1: RPL Lite + CoAP en Cooja.
 *
 * Logs: la aplicacion y RPL en INFO (RPL deja ver cuando converge el DODAG y
 * cuando cada cliente encuentra ruta); CoAP, IPv6, TCPIP y MAC en WARN para
 * que el Mote Output quede legible y la evidencia sea parseable.
 */
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LOG_LEVEL_APP LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_COAP LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_WARN

/*
 * Cinco clientes pueden llegar a la raiz casi al mismo tiempo; cada respuesta
 * ocupa una transaccion mientras se serializa y se envia. El default es 4.
 */
#define COAP_MAX_OPEN_TRANSACTIONS 8

/* Un cliente solo tiene UNA peticion en vuelo (ver cliente-coap.c). */
#define COAP_OBSERVE_CLIENT 0

#endif /* PROJECT_CONF_H_ */
