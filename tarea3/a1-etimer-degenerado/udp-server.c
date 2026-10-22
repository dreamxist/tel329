/*
 * TEL329 - Tarea 3, Actividad 1: servidor UDP y raiz del DODAG.
 *
 * Es el nodo 1 (ver ../nodos.h). Hace dos cosas:
 *   1. NETSTACK_ROUTING.root_start() -> se declara raiz RPL: anuncia el
 *      prefijo, acepta DAOs y habilita las rutas descendentes.
 *   2. escucha en 5678 y hace eco del payload al cliente que lo envio.
 *
 * El contador periodico es lo que permite comparar los cuatro casos de la
 * actividad: con el periodo base llegan ~1 peticion cada 10 s por cliente,
 * con los periodos degenerados llegan miles por segundo.
 */
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "sys/log.h"

#include <inttypes.h>

#include "../nodos.h"

#define LOG_MODULE "A1srv"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY 1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

/* Cada cuanto imprimir el acumulado de peticiones recibidas. */
#define RESUMEN_INTERVAL (10 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static uint32_t rx_total;

PROCESS(udp_server_process, "UDP server A1");
AUTOSTART_PROCESSES(&udp_server_process);

static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  rx_total++;

  LOG_INFO("rx '%.*s' desde ", datalen, (char *)data);
  LOG_INFO_6ADDR(sender_addr);
  LOG_INFO_("\n");

#if WITH_SERVER_REPLY
  simple_udp_sendto(&udp_conn, data, datalen, sender_addr);
#endif
}

PROCESS_THREAD(udp_server_process, ev, data)
{
  static struct etimer resumen_timer;

  PROCESS_BEGIN();

  LOG_INFO("nodo %u (%s) arranca como raiz del DODAG\n", node_id, node_name());

  NETSTACK_ROUTING.root_start();

  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  etimer_set(&resumen_timer, RESUMEN_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&resumen_timer));
    LOG_INFO("acumulado: %" PRIu32 " peticiones recibidas\n", rx_total);
    etimer_reset(&resumen_timer);
  }

  PROCESS_END();
}
