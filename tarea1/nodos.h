/*
 * TEL329 Tarea 1 - convenciones compartidas por las cuatro actividades.
 *
 * Regla unica de la tarea: el nodo 1 es SIEMPRE la estacion base.
 * Los demas nodos son sensores que le reportan (upstream).
 */
#ifndef NODOS_H_
#define NODOS_H_

#include "contiki.h"
#include "net/linkaddr.h"

#define ESTACION_BASE_ID 1

/*
 * Numero de nodo, leido del link-layer address.
 *
 * NO se usa el node_id de Contiki-NG a proposito. node_id_init() lo deriva de
 * los dos ULTIMOS bytes del link-layer address, pero la plataforma Cooja
 * escribe el ID del mote en los dos PRIMEROS cuando se compila sin IPv6 -- que
 * es justamente el caso con NullNet. El resultado es node_id == 0 en todos los
 * motes, y toda comparacion contra el falla en silencio.
 *
 * Es la misma razon por la que los ejemplos oficiales de NullNet comparan
 * direcciones con linkaddr_cmp() en lugar de mirar node_id.
 */
static inline uint16_t
node_num(void)
{
  return linkaddr_node_addr.u8[0] + (linkaddr_node_addr.u8[1] << 8);
}

/* Nombre legible del nodo, para que el Mote Output se lea solo. */
static inline const char *
node_name(void)
{
  switch(node_num()) {
  case 1:  return "Pancho";
  case 2:  return "Francisco";
  case 3:  return "Ignacio";
  case 4:  return "Berioska";
  default: return "Anonimo";
  }
}

static inline int
is_base_station(void)
{
  return node_num() == ESTACION_BASE_ID;
}

#endif /* NODOS_H_ */
