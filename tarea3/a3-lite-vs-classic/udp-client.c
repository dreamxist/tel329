/*
 * TEL329 - Tarea 3, Actividad 3: cliente UDP que mide el RTT de aplicacion.
 *
 * El enunciado pide comparar, sobre la misma topologia multi-salto, el tiempo
 * de respuesta ICMP (`ping` desde el shell de la raiz) contra el de la
 * aplicacion (peticion UDP y su eco). Este cliente mide el segundo:
 *
 *   - guarda clock_time() al enviar la peticion seq N;
 *   - cuando llega el eco, el RTT es la diferencia, en ms;
 *   - ademas lee UIP_IP_BUF->ttl del eco, que da los saltos de bajada.
 *     Ojo con la cuenta: el hop limit lo baja el que reenvia, no el destino,
 *     asi que los saltos son 65 - ttl y no 64 - ttl.
 *
 * Importante para la comparacion: el RTT de aplicacion incluye el camino
 * descendente, y ahi es donde Lite (non-storing) y Classic (storing por
 * defecto) se separan -- en non-storing la raiz arma la ruta con cabeceras
 * RH4 y los paquetes de bajada crecen con cada salto.
 *
 * Tambien imprime su propia direccion global al tenerla: es la que se copia
 * al `ping` de la raiz.
 */
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "random.h"
#include "sys/log.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../nodos.h"

#define LOG_MODULE "A3cli"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
#define SEND_INTERVAL (10 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static uint32_t rx_count;

/* Instante del ultimo envio, para el RTT. */
static clock_time_t tx_time;
static int esperando_eco;

PROCESS(udp_client_process, "UDP client A3");
AUTOSTART_PROCESSES(&udp_client_process);

static int
leer_sensor(void)
{
  return 20 + (node_id % 5) + (int)(random_rand() % 3);
}

static void
udp_rx_callback(struct simple_udp_connection *c,
                const uip_ipaddr_t *sender_addr,
                uint16_t sender_port,
                const uip_ipaddr_t *receiver_addr,
                uint16_t receiver_port,
                const uint8_t *data,
                uint16_t datalen)
{
  uint8_t ttl = UIP_IP_BUF->ttl;

  rx_count++;

  if(esperando_eco) {
    clock_time_t rtt = clock_time() - tx_time;
    LOG_INFO("eco '%.*s' rtt=%lu ms ttl=%u (saltos=%u)\n",
             datalen, (char *)data,
             (unsigned long)((1000UL * rtt) / CLOCK_SECOND),
             ttl, (unsigned)(65 - ttl));
    esperando_eco = 0;
  } else {
    LOG_INFO("eco fuera de turno '%.*s' ttl=%u\n", datalen, (char *)data, ttl);
  }
}

PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static char str[48];
  static uint32_t tx_count;
  static uint32_t missed_tx_count;
  static int direccion_anunciada;
  uip_ipaddr_t dest_ipaddr;
  uip_ds6_addr_t *global;

  PROCESS_BEGIN();

  LOG_INFO("nodo %u (%s) cliente multi-salto\n", node_id, node_name());

  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(!direccion_anunciada) {
      global = uip_ds6_get_global(ADDR_PREFERRED);
      if(global != NULL) {
        LOG_INFO("mi direccion global (para el ping desde la raiz): ");
        LOG_INFO_6ADDR(&global->ipaddr);
        LOG_INFO_("\n");
        direccion_anunciada = 1;
      }
    }

    if(NETSTACK_ROUTING.node_is_reachable() &&
       NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

      snprintf(str, sizeof(str), "id=%u seq=%" PRIu32 " t=%d",
               node_id, tx_count, leer_sensor());

      tx_time = clock_time();
      esperando_eco = 1;
      simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);

      LOG_INFO("tx %" PRIu32 " '%s' (rx %" PRIu32 ", perdidos %" PRIu32 ")\n",
               tx_count, str, rx_count, missed_tx_count);
      tx_count++;
    } else {
      LOG_INFO("todavia sin ruta hacia la raiz\n");
      if(tx_count > 0) {
        missed_tx_count++;
      }
    }

    etimer_set(&periodic_timer, SEND_INTERVAL);
  }

  PROCESS_END();
}
