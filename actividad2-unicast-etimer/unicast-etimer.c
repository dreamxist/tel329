/*
 * TEL329 - Tarea 1, Actividad 2
 * Unicast con NullNet, LED en cada evento e intervalo creciente con etimer.
 *
 * Metodo: el nodo 1 es la estacion base y solo recibe; el resto le transmite
 * en unicast (upstream). El receptor ESCRIBE su nombre sobre el texto recibido
 * antes de imprimirlo, que es la evidencia de que el paquete llego completo.
 *
 * Intervalo: 2s -> 4s -> ... -> 20s -> vuelve a 2s. Se implementa con un
 * etimer, que al expirar despierta al proceso con un evento (a diferencia del
 * ctimer, que llama una funcion; se usan los dos en este archivo).
 */
#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "dev/leds.h"
#include "sys/ctimer.h"
#include "../nodos.h"

#include <stdio.h>
#include <string.h>

#include "sys/log.h"
#define LOG_MODULE "A2"
#define LOG_LEVEL LOG_LEVEL_INFO

#define INTERVAL_MIN  (2 * CLOCK_SECOND)
#define INTERVAL_MAX  (20 * CLOCK_SECOND)
#define INTERVAL_STEP (2 * CLOCK_SECOND)
#define LED_ON_TIME   (CLOCK_SECOND / 2)

/* La estacion base siempre es el nodo 1. */
static linkaddr_t base_addr = {{ ESTACION_BASE_ID, 0, 0, 0, 0, 0, 0, 0 }};

static char payload[40];
static struct ctimer led_timer;

/*---------------------------------------------------------------------------*/
static void
led_off_callback(void *ptr)
{
  leds_off(LEDS_ALL);
}
/*---------------------------------------------------------------------------*/
static void
blink_led(leds_mask_t color)
{
  leds_on(color);
  ctimer_set(&led_timer, LED_ON_TIME, led_off_callback, NULL);
}
/*---------------------------------------------------------------------------*/
/* Escalera 2..20 segundos; al pasarse del maximo vuelve al valor inicial. */
static clock_time_t
next_interval(clock_time_t current)
{
  clock_time_t next = current + INTERVAL_STEP;
  return (next > INTERVAL_MAX) ? INTERVAL_MIN : next;
}
/*---------------------------------------------------------------------------*/
/* El receptor escribe su nombre sobre el paquete recibido y lo imprime. */
static void
receive_packet(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dest)
{
  char recibido[80];

  blink_led(LEDS_RED);
  snprintf(recibido, sizeof(recibido), "%s | leido por %s",
           (char *)data, node_name());

  LOG_INFO("RX unicast: \"%s\" desde ", recibido);
  LOG_INFO_LLADDR(src);
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
/* dest != NULL en NullNet significa unicast a esa direccion. */
static void
send_unicast(unsigned seq, clock_time_t interval)
{
  snprintf(payload, sizeof(payload), "msg %u de %s", seq, node_name());

  nullnet_buf = (uint8_t *)payload;
  nullnet_len = strlen(payload) + 1;

  blink_led(LEDS_GREEN);
  LOG_INFO("TX unicast (intervalo %lus): \"%s\" hacia ",
           (unsigned long)(interval / CLOCK_SECOND), payload);
  LOG_INFO_LLADDR(&base_addr);
  LOG_INFO_("\n");

  NETSTACK_NETWORK.output(&base_addr);
}
/*---------------------------------------------------------------------------*/
PROCESS(unicast_etimer_process, "A2 unicast con intervalo creciente");
AUTOSTART_PROCESSES(&unicast_etimer_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(unicast_etimer_process, ev, data)
{
  static struct etimer send_timer;
  static clock_time_t interval;
  static unsigned seq;

  PROCESS_BEGIN();

  nullnet_set_input_callback(receive_packet);

  if(is_base_station()) {
    LOG_INFO("nodo %u (%s) es la estacion base - solo recibe\n",
             node_num(), node_name());
    while(1) {
      PROCESS_YIELD(); /* no transmite: los paquetes llegan por el callback */
    }
  }

  LOG_INFO("nodo %u (%s) transmite hacia la estacion base\n",
           node_num(), node_name());

  interval = INTERVAL_MIN;
  etimer_set(&send_timer, interval);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));

    send_unicast(seq, interval);
    seq++;

    interval = next_interval(interval);
    etimer_set(&send_timer, interval);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
