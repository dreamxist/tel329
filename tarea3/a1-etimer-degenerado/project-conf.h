/*
 * TEL329 - Tarea 3, Actividad 1: configuracion de logs.
 *
 * RPL en INFO es lo que deja ver la convergencia del DODAG (o su ausencia,
 * que es justamente lo que pasa con el periodo degenerado). IPv6 y TCPIP en
 * WARN para que el flood no tape los mensajes de la aplicacion.
 */
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_WARN

#endif /* PROJECT_CONF_H_ */
