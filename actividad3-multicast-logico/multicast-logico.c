/*
 * TEL329 - Tarea 1, Actividad 3
 * Multicast logico sobre broadcast NullNet.
 *
 * Por que "logico": el multicast es un servicio de la CAPA DE RED, y NullNet
 * justamente elimina esa capa. Entonces se emula en la aplicacion: se
 * transmite en broadcast (que si existe a nivel de enlace) y se lleva un
 * ID de grupo DENTRO del payload. El receptor aplica la regla:
 *
 *     si grupo del paquete == mi grupo  -> procesa
 *     si no                             -> descarta
 *
 * El descarte se imprime a proposito: es la evidencia de que el filtro
 * corre en el receptor y no en la red.
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
#define LOG_MODULE "A3"
#define LOG_LEVEL LOG_LEVEL_INFO

#define GRUPO_A       1
#define GRUPO_B       2
#define SEND_INTERVAL (4 * CLOCK_SECOND)
#define LED_ON_TIME   (CLOCK_SECOND / 2)

/* El paquete es la unica diferencia con la actividad 1: lleva el ID de grupo. */
typedef struct {
  uint8_t grupo;
  char texto[32];
} multicast_packet_t;

static multicast_packet_t packet;
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
static const char *
group_name(uint8_t grupo)
{
  return (grupo == GRUPO_A) ? "A" : "B";
}
/*---------------------------------------------------------------------------*/
/* Membresia estatica: nodos 2 y 3 al grupo A, el resto al grupo B. */
static uint8_t
my_group(void)
{
  return (node_num() <= 3) ? GRUPO_A : GRUPO_B;
}
/*---------------------------------------------------------------------------*/
/* Aca vive la regla logica que reemplaza al multicast de capa de red. */
static void
receive_packet(const void *data, uint16_t len,
               const linkaddr_t *src, const linkaddr_t *dest)
{
  const multicast_packet_t *rx = (const multicast_packet_t *)data;

  if(len != sizeof(multicast_packet_t)) {
    return;
  }

  if(rx->grupo != my_group()) {
    LOG_INFO("DESCARTA: paquete del grupo %s y yo soy del grupo %s\n",
             group_name(rx->grupo), group_name(my_group()));
    return;
  }

  blink_led(LEDS_RED);
  LOG_INFO("PROCESA: grupo %s - \"%s\"\n", group_name(rx->grupo), rx->texto);
}
/*---------------------------------------------------------------------------*/
/* Se transmite en broadcast: el filtro no esta aca, esta en el receptor. */
static void
send_multicast(uint8_t grupo, unsigned seq)
{
  packet.grupo = grupo;
  snprintf(packet.texto, sizeof(packet.texto), "aviso %u para el grupo %s",
           seq, group_name(grupo));

  nullnet_buf = (uint8_t *)&packet;
  nullnet_len = sizeof(packet);

  blink_led(LEDS_GREEN);
  LOG_INFO("TX broadcast dirigido al grupo %s: \"%s\"\n",
           group_name(grupo), packet.texto);

  NETSTACK_NETWORK.output(NULL);
}
/*---------------------------------------------------------------------------*/
PROCESS(multicast_logico_process, "A3 multicast logico por ID de grupo");
AUTOSTART_PROCESSES(&multicast_logico_process);
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(multicast_logico_process, ev, data)
{
  static struct etimer send_timer;
  static unsigned seq;
  static uint8_t grupo_destino = GRUPO_A;

  PROCESS_BEGIN();

  nullnet_set_input_callback(receive_packet);

  if(!is_base_station()) {
    LOG_INFO("nodo %u (%s) escucha - pertenece al grupo %s\n",
             node_num(), node_name(), group_name(my_group()));
    while(1) {
      PROCESS_YIELD();
    }
  }

  LOG_INFO("nodo %u (%s) es el emisor - alterna entre grupo A y B\n",
           node_num(), node_name());

  etimer_set(&send_timer, SEND_INTERVAL);

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&send_timer));

    send_multicast(grupo_destino, seq);
    seq++;

    /* Alterna el destino para que se vea a un grupo procesar y al otro descartar. */
    grupo_destino = (grupo_destino == GRUPO_A) ? GRUPO_B : GRUPO_A;
    etimer_reset(&send_timer);
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
