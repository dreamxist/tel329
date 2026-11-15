#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

/* httpd-ws corre sobre TCP; Contiki-NG lo trae apagado por defecto. */
#define UIP_CONF_TCP 1

/* httpd-ws exige un buffer de entrada >= ventana TCP; con el MSS por defecto
 * (~1200 B) no compila. 128 B alcanza para un GET y es lo que cabe en un mote. */
#define UIP_CONF_TCP_MSS 128
#define UIP_CONF_RECEIVE_WINDOW 128

/* Buffers de httpd-ws del mismo tamaño que la ventana: alcanza para la
 * request-line y evita el -Werror=format-truncation de GCC moderno al copiar
 * inputbuf -> filename (httpd-ws.c:255). */
#define WEBSERVER_CONF_CFS_PATHLEN 128
#define WEBSERVER_CONF_INBUF_SIZE 128

/* Logs del stack IPv6/RPL al mínimo para que la salida del mote sea legible. */
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_RPL  LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_WARN

#endif /* PROJECT_CONF_H_ */
