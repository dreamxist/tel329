/*
 * TEL329 - Tarea 3, Actividad 4: el mote que sensa y empuja al sink.
 *
 * Diferencia con el cliente de la A1: la lectura del sensor esta aislada en
 * una sola funcion, leer_sensor(), que es el unico punto a cambiar si se
 * reemplaza el valor arbitrario por un sensor real de la plataforma. El resto
 * del codigo no sabe de donde viene el numero.
 *
 * El valor es una caminata aleatoria acotada alrededor de una base por nodo:
 * se mueve poco entre muestras, como se moveria una temperatura, para que la
 * serie que queda en el sink tenga sentido al graficarla.
 */
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/netstack.h"
#include "net/ipv6/simple-udp.h"
#include "random.h"
#include "sys/log.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../nodos.h"

#define LOG_MODULE "A4mote"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678
#define SEND_INTERVAL (10 * CLOCK_SECOND)

/* Limites de la caminata, en grados. */
#define TEMP_MIN 15
#define TEMP_MAX 32

static struct simple_udp_connection udp_conn;
static uint32_t rx_count;
static int temp_actual;

PROCESS(udp_client_process, "UDP sensor A4");
AUTOSTART_PROCESSES(&udp_client_process);

/*
 * UNICO punto de contacto con el "sensor". Para usar un sensor real de la
 * plataforma, reemplazar el cuerpo por su lectura (SENSORS_ACTIVATE + value)
 * y dejar la firma igual.
 */
static int
leer_sensor(void)
{
  int paso = (int)(random_rand() % 3) - 1; /* -1, 0 o +1 */

  temp_actual += paso;
  if(temp_actual < TEMP_MIN) {
    temp_actual = TEMP_MIN;
  } else if(temp_actual > TEMP_MAX) {
    temp_actual = TEMP_MAX;
  }
  return temp_actual;
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
  rx_count++;
}

PROCESS_THREAD(udp_client_process, ev, data)
{
  static struct etimer periodic_timer;
  static char str[48];
  static uint32_t tx_count;
  static uint32_t missed_tx_count;
  uip_ipaddr_t dest_ipaddr;

  PROCESS_BEGIN();

  /* Base por nodo, para que las series no queden encimadas. */
  temp_actual = 20 + (node_id % 5);

  LOG_INFO("nodo %u (%s) sensor, base %d grados\n",
           node_id, node_name(), temp_actual);

  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  etimer_set(&periodic_timer, random_rand() % SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() &&
       NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

      snprintf(str, sizeof(str), "id=%u seq=%" PRIu32 " t=%d",
               node_id, tx_count, leer_sensor());
      simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);

      LOG_INFO("tx %" PRIu32 " '%s'\n", tx_count, str);
      tx_count++;
    } else {
      LOG_INFO("todavia sin ruta hacia el sink\n");
      if(tx_count > 0) {
        missed_tx_count++;
      }
    }

    etimer_set(&periodic_timer, SEND_INTERVAL);
  }

  PROCESS_END();
}
