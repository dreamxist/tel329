/*
 * TEL329 - Tarea 3, Actividad 4: el sink que recopila el sensado.
 *
 * Es el equivalente en Contiki-NG del patron de sky-websense.c: en Contiki-OS
 * cada mote exponia sus sensores por HTTP y uno los consultaba con el
 * navegador; aca los motes empujan el dato por UDP y el sink lo registra.
 *
 * La linea de registro es deliberadamente parseable:
 *
 *   SENSOR,<id>,<seq>,<temp>,<ttl>
 *
 * Asi se filtra "SENSOR" en el Mote output de Cooja y la salida se pega
 * directo en una planilla, sin tener que limpiar el log a mano.
 */
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "net/ipv6/uip.h"
#include "sys/log.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>

#include "../nodos.h"

#define LOG_MODULE "A4sink"
#define LOG_LEVEL LOG_LEVEL_INFO

#define WITH_SERVER_REPLY 1
#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

#define RESUMEN_INTERVAL (60 * CLOCK_SECOND)

static struct simple_udp_connection udp_conn;
static uint32_t rx_total;

PROCESS(udp_server_process, "UDP sink A4");
AUTOSTART_PROCESSES(&udp_server_process);

/*
 * Extrae el entero que sigue a una clave "clave=" dentro del payload.
 * Devuelve -1 si la clave no esta. El payload no viene terminado en '\0',
 * asi que se copia a un buffer local antes de buscar.
 */
static long
valor_de(const char *buf, const char *clave)
{
  const char *p = strstr(buf, clave);
  if(p == NULL) {
    return -1;
  }
  return strtol(p + strlen(clave), NULL, 10);
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
  char buf[64];
  uint16_t len = datalen < sizeof(buf) - 1 ? datalen : sizeof(buf) - 1;

  memcpy(buf, data, len);
  buf[len] = '\0';

  rx_total++;

  /* El registro recopilable. Filtrar por "SENSOR" en el Mote output. */
  LOG_INFO("SENSOR,%ld,%ld,%ld,%u\n",
           valor_de(buf, "id="),
           valor_de(buf, "seq="),
           valor_de(buf, "t="),
           UIP_IP_BUF->ttl);

#if WITH_SERVER_REPLY
  simple_udp_sendto(&udp_conn, data, datalen, sender_addr);
#endif
}

PROCESS_THREAD(udp_server_process, ev, data)
{
  static struct etimer resumen_timer;

  PROCESS_BEGIN();

  LOG_INFO("nodo %u (%s) arranca como sink y raiz del DODAG\n",
           node_id, node_name());
  LOG_INFO("formato de registro: SENSOR,id,seq,temp,ttl\n");

  NETSTACK_ROUTING.root_start();

  simple_udp_register(&udp_conn, UDP_SERVER_PORT, NULL,
                      UDP_CLIENT_PORT, udp_rx_callback);

  etimer_set(&resumen_timer, RESUMEN_INTERVAL);
  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&resumen_timer));
    LOG_INFO("acumulado: %" PRIu32 " muestras registradas\n", rx_total);
    etimer_reset(&resumen_timer);
  }

  PROCESS_END();
}
