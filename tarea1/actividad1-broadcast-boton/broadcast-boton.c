/*
 * TEL329 - Tarea 1, Actividad 1
 * Broadcast con NullNet disparado por el boton del nodo.
 *
 * Metodo: el proceso no usa temporizadores. Se bloquea esperando el evento
 * button_hal_press_event y recien ahi transmite. Es la forma mas simple de
 * mostrar que NullNet entrega el paquete directo a la capa MAC: no hay
 * periodicidad ni ruteo de por medio, solo un evento de la aplicacion.
 */
#include "contiki.h"
#include "net/netstack.h"
#include "net/nullnet/nullnet.h"
#include "dev/button-hal.h"
#include "dev/leds.h"
#include "sys/ctimer.h"
#include "../nodos.h"

#include <stdio.h>
#include <string.h>

#include "sys/log.h"
#define LOG_MODULE "A1"
#define LOG_LEVEL LOG_LEVEL_INFO

#define LED_ON_TIME (CLOCK_SECOND / 2)

static char payload[40];
static struct ctimer led_timer;

/*---------------------------------------------------------------------------*/
/* ctimer: apaga el LED medio segundo despues, sin bloquear el proceso. */
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
/* NullNet devuelve el paquete a la aplicacion por este callback. */
static void
receive_packet(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dest)
{
  blink_led(LEDS_RED);
  LOG_INFO("RX broadcast: \"%s\" desde ", (char *)data);
  LOG_INFO_LLADDR(src);
  LOG_INFO_("\n");
}
/*---------------------------------------------------------------------------*/
/* dest = NULL en NullNet significa "a todos los vecinos" (broadcast). */
static void
send_broadcast(void)
{
  snprintf(payload, sizeof(payload), "hola desde %s", node_name());

  nullnet_buf = (uint8_t *)payload;
  nullnet_len = strlen(payload) + 1;

  blink_led(LEDS_GREEN);
  LOG_INFO("TX broadcast: \"%s\"\n", payload);
  NETSTACK_NETWORK.output(NULL);
}
/*---------------------------------------------------------------------------*/
PROCESS(broadcast_boton_process, "A1 broadcast por boton");
AUTOSTART_PROCESSES(&broadcast_boton_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(broadcast_boton_process, ev, data)
{
  PROCESS_BEGIN();

  nullnet_set_input_callback(receive_packet);
  LOG_INFO("nodo %u (%s) listo, presiona el boton para transmitir\n",
           node_num(), node_name());

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(ev == button_hal_press_event);
    send_broadcast();
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
