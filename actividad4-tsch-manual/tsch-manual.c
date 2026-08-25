/*
 * TEL329 - Tarea 1, Actividad 4
 * Slotframe TSCH construido a mano, sin 6TiSCH Minimal Schedule.
 *
 * Metodo - una regla y de ahi sale todo:
 *
 *     el nodo N transmite unicamente en la ranura N-1
 *
 * Con el nodo 1 como coordinador, eso reparte el slotframe asi:
 *
 *     ranura 0 : baliza (EB). El coordinador TX, todos los demas RX.
 *     ranura 1 : solo el nodo 2 transmite. El coordinador RX.
 *     ranura 2 : solo el nodo 3 transmite. El coordinador RX.
 *     ranura 3 : solo el nodo 4 transmite. El coordinador RX.
 *     ranuras 4-9 : sin enlaces -> la radio se apaga (ahorro de energia).
 *
 * Determinismo y ausencia de colisiones quedan demostrados POR CONSTRUCCION:
 * ningun par de nodos comparte una ranura de transmision, asi que dos nodos
 * nunca pueden estar transmitiendo en el mismo instante. El Timeline de Cooja
 * lo muestra como un patron que se repite identico en cada slotframe.
 *
 * Sin capa de red que elija un coordinador, hay que forzarlo a mano:
 * el nodo 1 llama tsch_set_coordinator() y es la fuente de tiempo del resto.
 */
#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "net/mac/tsch/tsch.h"
#include "net/mac/tsch/tsch-schedule.h"
#include "dev/leds.h"
#include "sys/ctimer.h"
#include "../nodos.h"

#include <stdio.h>
#include <string.h>

#include "sys/log.h"
#define LOG_MODULE "A4"
#define LOG_LEVEL LOG_LEVEL_INFO

#define SLOTFRAME_HANDLE 0
#define SLOTFRAME_SIZE   10  /* 10 ranuras, como pide la actividad */
#define CHANNEL_OFFSET   0
#define EB_TIMESLOT      0   /* la ranura reservada a las balizas */
#define MAX_NODES        4   /* coordinador + 3 sensores */

#define SEND_INTERVAL (4 * CLOCK_SECOND)
#define LED_ON_TIME   (CLOCK_SECOND / 2)

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
/* La regla del reparto: el nodo N transmite en la ranura N-1. */
static uint16_t
my_timeslot(void)
{
  return node_num() - 1;
}
/*---------------------------------------------------------------------------*/
/*
 * Construye el slotframe manualmente. Cada nodo agrega SOLO sus propios
 * enlaces, pero todos aplican la misma regla, asi que las tablas calzan.
 */
static int
build_manual_schedule(void)
{
  struct tsch_slotframe *sf;
  uint16_t ts;

  sf = tsch_schedule_add_slotframe(SLOTFRAME_HANDLE, SLOTFRAME_SIZE);
  if(sf == NULL) {
    LOG_ERR("no se pudo crear el slotframe\n");
    return 0;
  }

  if(is_base_station()) {
    /* Ranura 0: sin este enlace de balizas, nadie se sincroniza nunca. */
    tsch_schedule_add_link(sf, LINK_OPTION_TX, LINK_TYPE_ADVERTISING,
                           &tsch_broadcast_address,
                           EB_TIMESLOT, CHANNEL_OFFSET, 1);

    /* Una ranura de escucha por cada sensor, en la ranura propia de cada uno. */
    for(ts = 1; ts < MAX_NODES; ts++) {
      tsch_schedule_add_link(sf, LINK_OPTION_RX, LINK_TYPE_NORMAL,
                             &tsch_broadcast_address,
                             ts, CHANNEL_OFFSET, 1);
    }
  } else {
    /* Ranura 0: escucha las balizas para sincronizar su reloj. */
    tsch_schedule_add_link(sf, LINK_OPTION_RX, LINK_TYPE_ADVERTISING,
                           &tsch_broadcast_address,
                           EB_TIMESLOT, CHANNEL_OFFSET, 1);

    /* Ranura propia: la unica del slotframe donde este nodo transmite. */
    tsch_schedule_add_link(sf, LINK_OPTION_TX, LINK_TYPE_NORMAL,
                           &base_addr,
                           my_timeslot(), CHANNEL_OFFSET, 1);
  }

  return 1;
}
/*---------------------------------------------------------------------------*/
/* Imprime la matriz tiempo x frecuencia de este nodo - evidencia del informe. */
static void
print_schedule_matrix(void)
{
  uint16_t ts;

  LOG_INFO("slotframe de %u ranuras, channel offset %u\n",
           SLOTFRAME_SIZE, CHANNEL_OFFSET);

  for(ts = 0; ts < SLOTFRAME_SIZE; ts++) {
    const char *rol = "dormir (radio apagada)";

    if(ts == EB_TIMESLOT) {
      rol = is_base_station() ? "TX baliza EB" : "RX baliza EB";
    } else if(is_base_station() && ts < MAX_NODES) {
      rol = "RX de un sensor";
    } else if(!is_base_station() && ts == my_timeslot()) {
      rol = "TX al coordinador";
    }

    LOG_INFO("  ranura %2u | canal %u | %s\n", ts, CHANNEL_OFFSET, rol);
  }
}
/*---------------------------------------------------------------------------*/
static void
receive_packet(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dest)
{
  blink_led(LEDS_RED);
  LOG_INFO("RX: \"%s\" desde ", (char *)data);
  LOG_INFO_LLADDR(src);
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
/*
 * El paquete se entrega a TSCH, que lo retiene hasta que llegue la ranura
 * propia de este nodo. Por eso el envio nunca colisiona: no sale cuando la
 * aplicacion quiere, sale cuando el calendario lo permite.
 */
static void
send_to_coordinator(unsigned seq)
{
  snprintf(payload, sizeof(payload), "dato %u de %s", seq, node_name());

  nullnet_buf = (uint8_t *)payload;
  nullnet_len = strlen(payload) + 1;

  blink_led(LEDS_GREEN);
  LOG_INFO("TX en ranura %u: \"%s\"\n", my_timeslot(), payload);

  NETSTACK_NETWORK.output(&base_addr);
}
/*---------------------------------------------------------------------------*/
PROCESS(tsch_manual_process, "A4 slotframe TSCH manual");
AUTOSTART_PROCESSES(&tsch_manual_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(tsch_manual_process, ev, data)
{
  static struct etimer send_timer;
  static unsigned seq;

  PROCESS_BEGIN();

  nullnet_set_input_callback(receive_packet);

  /* 1. El schedule se arma antes de asociarse, para que ya este listo. */
  if(!build_manual_schedule()) {
    PROCESS_EXIT();
  }
  print_schedule_matrix();

  /* 2. Recien ahora se declara el coordinador: empieza a emitir balizas. */
  if(is_base_station()) {
    LOG_INFO("nodo %u (%s): soy el coordinador de tiempo\n",
             node_num(), node_name());
    tsch_set_coordinator(1);

    while(1) {
      PROCESS_YIELD(); /* solo recibe */
    }
  }

  LOG_INFO("nodo %u (%s): esperando balizas para sincronizar\n",
           node_num(), node_name());

  etimer_set(&send_timer, SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));

    if(tsch_is_associated) {
      send_to_coordinator(seq);
      seq++;
    } else {
      LOG_INFO("todavia sin asociar a la red TSCH\n");
    }

    etimer_reset(&send_timer);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
