/*
 * TEL329 - Tarea 3, Actividad 1: cliente UDP sobre una red RPL autonoma.
 *
 * El payload lleva datos sensados arbitrarios con el formato que pide el
 * enunciado: "id=%u seq=%lu t=%d".
 *
 * Lo que la actividad investiga es el PERIODO del etimer. El caso se elige al
 * compilar, con CASO= en el make (ver Makefile y README):
 *
 *   CASO=base    10 * CLOCK_SECOND     (referencia del ejemplo rpl-udp)
 *   CASO=1e-5    1e-5 * CLOCK_SECOND
 *   CASO=1e-10   1e-10 * CLOCK_SECOND
 *   CASO=1e-20   1e-20 * CLOCK_SECOND
 *
 * El nodo imprime al arrancar cuantos ticks le quedo el intervalo. Ese numero
 * es la respuesta de la actividad: clock_time_t es entero, asi que los tres
 * casos degenerados truncan a 0 y el etimer queda sin espera.
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

#define LOG_MODULE "A1cli"
#define LOG_LEVEL LOG_LEVEL_INFO

#define UDP_CLIENT_PORT 8765
#define UDP_SERVER_PORT 5678

/*
 * CASO_ID lo define el Makefile. Se pasa como numero y no como string para no
 * depender de como Cooja escapa el comando de compilacion: cuando compila un
 * mote desde la GUI no usa tu shell.
 */
#ifndef CASO_ID
#define CASO_ID 0
#endif

#if CASO_ID == 0
#define SEND_INTERVAL_FACTOR 10.0
#define CASO_NOMBRE "base: 10 * CLOCK_SECOND"
#define LOG_CADA_N 1
#elif CASO_ID == 5
#define SEND_INTERVAL_FACTOR 1e-5
#define CASO_NOMBRE "1e-5 * CLOCK_SECOND"
#define LOG_CADA_N 500
#elif CASO_ID == 10
#define SEND_INTERVAL_FACTOR 1e-10
#define CASO_NOMBRE "1e-10 * CLOCK_SECOND"
#define LOG_CADA_N 500
#elif CASO_ID == 20
#define SEND_INTERVAL_FACTOR 1e-20
#define CASO_NOMBRE "1e-20 * CLOCK_SECOND"
#define LOG_CADA_N 500
#else
#error "CASO_ID desconocido: usar make CASO=base|1e-5|1e-10|1e-20"
#endif

static struct simple_udp_connection udp_conn;
static uint32_t rx_count;

PROCESS(udp_client_process, "UDP client A1");
AUTOSTART_PROCESSES(&udp_client_process);

/*
 * El intervalo tal como lo escribe el enunciado. La multiplicacion se hace en
 * punto flotante y el resultado se asigna a clock_time_t, que es entero: ahi
 * esta el truncamiento que la actividad tiene que explicar.
 */
static clock_time_t
intervalo_ticks(void)
{
  return (clock_time_t)(SEND_INTERVAL_FACTOR * CLOCK_SECOND);
}

/* Valor "sensado" arbitrario: base por nodo mas una variacion pequena. */
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

  LOG_INFO("nodo %u (%s) cliente, caso %s\n", node_id, node_name(), CASO_NOMBRE);
  LOG_INFO("CLOCK_SECOND=%u -> intervalo = %lu ticks\n",
           (unsigned)CLOCK_SECOND, (unsigned long)intervalo_ticks());

  simple_udp_register(&udp_conn, UDP_CLIENT_PORT, NULL,
                      UDP_SERVER_PORT, udp_rx_callback);

  /* Desincronizar el primer envio para que no colisionen todos a la vez. */
  etimer_set(&periodic_timer, random_rand() % (10 * CLOCK_SECOND));

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&periodic_timer));

    if(NETSTACK_ROUTING.node_is_reachable() &&
       NETSTACK_ROUTING.get_root_ipaddr(&dest_ipaddr)) {

      snprintf(str, sizeof(str), "id=%u seq=%" PRIu32 " t=%d",
               node_id, tx_count, leer_sensor());
      simple_udp_sendto(&udp_conn, str, strlen(str), &dest_ipaddr);

      if(tx_count < 20 || tx_count % LOG_CADA_N == 0) {
        LOG_INFO("tx %" PRIu32 " '%s' (rx %" PRIu32 ", perdidos %" PRIu32 ")\n",
                 tx_count, str, rx_count, missed_tx_count);
      }
      tx_count++;
    } else {
      /*
       * El contador propio es necesario: con el periodo degenerado el nodo
       * puede no enviar nunca, y entonces tx_count se queda en 0 y cualquier
       * throttle que dependa de el imprime en cada vuelta. Con el etimer en 0
       * ticks eso son miles de lineas por segundo simulado.
       */
      static uint32_t sin_ruta;

      if(sin_ruta % LOG_CADA_N == 0) {
        LOG_INFO("todavia sin ruta hacia la raiz (intento %" PRIu32 ")\n",
                 sin_ruta);
      }
      sin_ruta++;
      if(tx_count > 0) {
        missed_tx_count++;
      }
    }

    etimer_set(&periodic_timer, intervalo_ticks());
  }

  PROCESS_END();
}
