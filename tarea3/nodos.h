/*
 * TEL329 Tarea 3 - convenciones compartidas por las actividades.
 *
 * Regla unica de la tarea: el nodo 1 es SIEMPRE la raiz del DODAG (y el
 * servidor UDP). Los demas nodos son clientes que le reportan.
 *
 * Diferencia importante con la Tarea 1: aca node_id SI es confiable.
 *
 * La trampa de la Tarea 1 -- node_id == 0 en todos los motes -- aparece solo
 * cuando se compila SIN IPv6. En ese caso la plataforma Cooja escribe el ID
 * del mote en los dos PRIMEROS bytes del link-layer address, y
 * node_id_init() lo deriva de los dos ULTIMOS. Con MAKE_NET_IPV6 (el caso de
 * toda esta tarea) el ID queda donde node_id_init() lo busca, asi que no hace
 * falta el workaround de node_num() y se usa node_id directo.
 *
 * Se comprueba en el log: la Tarea 1 imprime "Node ID: 0" y esta imprime el
 * numero real del mote.
 */
#ifndef NODOS_H_
#define NODOS_H_

#include "contiki.h"
#include "sys/node-id.h"

#define RAIZ_DODAG_ID 1

/* Nombre legible del nodo, para que el Mote Output se lea solo. */
static inline const char *
node_name(void)
{
  switch(node_id) {
  case 1:  return "Pancho";
  case 2:  return "Francisco";
  case 3:  return "Ignacio";
  case 4:  return "Berioska";
  case 5:  return "Mote5";
  default: return "Anonimo";
  }
}

static inline int
es_raiz(void)
{
  return node_id == RAIZ_DODAG_ID;
}

#endif /* NODOS_H_ */
