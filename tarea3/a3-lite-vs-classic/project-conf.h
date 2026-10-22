/*
 * TEL329 - Tarea 3, Actividad 3: configuracion para la comparacion
 * Lite vs Classic.
 *
 * RPL en INFO deja ver el rank de cada nodo y los DAO: es como se comprueba
 * que la cadena quedo multi-salto y no todos colgados de la raiz.
 *
 * Sobre el modo de operacion (MOP), que es el fondo de la comparacion:
 *
 *   - RPL Lite implementa SOLO non-storing (MOP 1). Las rutas descendentes
 *     las arma la raiz y viajan en la cabecera de enrutamiento del paquete,
 *     asi que el paquete de bajada crece con cada salto. Los nodos
 *     intermedios no guardan tabla.
 *   - RPL Classic por defecto es storing (MOP 2): cada nodo guarda las rutas
 *     de su subarbol. Paquetes de bajada mas chicos, memoria por nodo mayor.
 *
 * Para forzar el MOP en Classic y ver la diferencia, descomentar:
 *   #define RPL_CONF_MOP RPL_MOP_NON_STORING
 */
#ifndef PROJECT_CONF_H_
#define PROJECT_CONF_H_

#define LOG_CONF_LEVEL_RPL LOG_LEVEL_INFO
#define LOG_CONF_LEVEL_IPV6 LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_TCPIP LOG_LEVEL_WARN
#define LOG_CONF_LEVEL_MAC LOG_LEVEL_WARN

#endif /* PROJECT_CONF_H_ */
