/*
 * TEL329 - Tarea 4, Actividad 2: cliente MQTT sobre RPL Lite.
 *
 * Adaptado de examples/mqtt-client/mqtt-client.c (Contiki-NG). Se quito todo
 * lo de IBM Watson, LEDs, boton, suscripcion y extensiones por plataforma; lo
 * que queda es la maquina de estados de conexion y un unico PUBLISH periodico
 * con QoS 1 al tema A2_TOPIC.
 *
 * Cada publicacion lleva una temperatura aleatoria (random_rand()) y deja dos
 * lineas parseables en el Mote Output, con el reloj del mote en ms:
 *
 *   A2,PUB,<id>,<seq>,<mid>,<t_ms>,<rank>,<padre>   al llamar mqtt_publish()
 *   A2,PUBACK,<id>,<mid>,<t_ms>                     al recibir MQTT_EVENT_PUBACK
 *
 * La diferencia entre ambas es el tiempo de ida y vuelta de la publicacion
 * (mote -> RPL -> border router -> tunslip6 -> broker y de vuelta el PUBACK).
 * <padre> es el ultimo byte de la direccion del padre RPL preferido: en Cooja
 * coincide con el ID del mote padre, asi que de la cadena de padres sale la
 * cantidad de saltos hasta la raiz.
 */
#include "contiki.h"
#include "net/routing/routing.h"
#include "net/routing/rpl-lite/rpl.h"
#include "net/ipv6/uip.h"
#include "net/ipv6/uip-ds6.h"
#include "mqtt.h"
#include "sys/etimer.h"
#include "sys/node-id.h"
#include "lib/random.h"
#include "sys/log.h"

#include <stdio.h>
#include <string.h>

#define LOG_MODULE "A2-MQTT"
#define LOG_LEVEL LOG_LEVEL_INFO
/*---------------------------------------------------------------------------*/
#define STATE_INIT          0
#define STATE_REGISTERED    1
#define STATE_CONNECTING    2
#define STATE_CONNECTED     3
#define STATE_PUBLISHING    4
#define STATE_DISCONNECTED  5
#define STATE_ERROR      0xFF

#define STATE_MACHINE_PERIODIC  (CLOCK_SECOND >> 1)
#define NET_CONNECT_PERIODIC    (CLOCK_SECOND >> 2)
#define RECONNECT_INTERVAL      (CLOCK_SECOND * 2)
#define CONNECTION_STABLE_TIME  (CLOCK_SECOND * 5)
#define PUBLISH_INTERVAL        (A2_INTERVALO_S * CLOCK_SECOND)

#define BUFFER_SIZE     64
#define APP_BUFFER_SIZE 128
/*---------------------------------------------------------------------------*/
static struct mqtt_connection conn;
static char client_id[BUFFER_SIZE];
static char pub_topic[] = A2_TOPIC;
static char app_buffer[APP_BUFFER_SIZE];

static struct etimer publish_periodic_timer;
static struct timer connection_life;
static uint8_t connect_attempt;
static uint8_t state;
static uint16_t seq_nr_value = 0;
/*---------------------------------------------------------------------------*/
PROCESS(mqtt_cliente_process, "A2 cliente MQTT");
AUTOSTART_PROCESSES(&mqtt_cliente_process);
/*---------------------------------------------------------------------------*/
static bool
have_connectivity(void)
{
  return uip_ds6_get_global(ADDR_PREFERRED) != NULL &&
         uip_ds6_defrt_choose() != NULL;
}
/*---------------------------------------------------------------------------*/
/* Ultimo byte de la direccion del padre preferido (== ID del mote en Cooja). */
static unsigned
padre_id(void)
{
  const uip_ipaddr_t *p = NULL;
  if(curr_instance.used && curr_instance.dag.preferred_parent != NULL) {
    p = rpl_neighbor_get_ipaddr(curr_instance.dag.preferred_parent);
  }
  if(p == NULL) {
    p = uip_ds6_defrt_choose();
  }
  return p == NULL ? 0 : p->u8[15];
}
/*---------------------------------------------------------------------------*/
static unsigned
rank_dag(void)
{
  return curr_instance.used ? DAG_RANK(curr_instance.dag.rank) : 0;
}
/*---------------------------------------------------------------------------*/
static void
mqtt_event(struct mqtt_connection *m, mqtt_event_t event, void *data)
{
  switch(event) {
  case MQTT_EVENT_CONNECTED:
    printf("A2,CONNECTED,%u,%lu\n", node_id, (unsigned long)clock_time());
    timer_set(&connection_life, CONNECTION_STABLE_TIME);
    state = STATE_CONNECTED;
    break;
  case MQTT_EVENT_DISCONNECTED:
  case MQTT_EVENT_CONNECTION_REFUSED_ERROR:
    printf("A2,DISCONNECTED,%u,%lu,%u\n", node_id,
           (unsigned long)clock_time(), *((mqtt_event_t *)data));
    state = STATE_DISCONNECTED;
    process_poll(&mqtt_cliente_process);
    break;
  case MQTT_EVENT_PUBACK:
    printf("A2,PUBACK,%u,%u,%lu\n", node_id, *((uint16_t *)data),
           (unsigned long)clock_time());
    break;
  default:
    LOG_WARN("evento MQTT no manejado: %i\n", event);
    break;
  }
}
/*---------------------------------------------------------------------------*/
static void
publish(void)
{
  uint16_t mid = 0;
  mqtt_status_t st;
  unsigned temp_x10;
  int len;

  seq_nr_value++;

  /* Temperatura aleatoria en [15.0, 34.9] C, en decimas para no usar float. */
  temp_x10 = 150 + (random_rand() % 200);

  len = snprintf(app_buffer, APP_BUFFER_SIZE,
                 "{\"id\":%u,\"seq\":%u,\"temp\":%u.%u,\"t\":%lu}",
                 node_id, seq_nr_value, temp_x10 / 10, temp_x10 % 10,
                 (unsigned long)clock_time());
  if(len < 0 || len >= APP_BUFFER_SIZE) {
    LOG_ERR("payload no cabe (%d)\n", len);
    return;
  }

  st = mqtt_publish(&conn, &mid, pub_topic, (uint8_t *)app_buffer, len,
                    MQTT_QOS_LEVEL_1, MQTT_RETAIN_OFF);
  if(st != MQTT_STATUS_OK) {
    LOG_ERR("mqtt_publish fallo: %d\n", st);
    return;
  }
  printf("A2,PUB,%u,%u,%u,%lu,%u,%u\n", node_id, seq_nr_value, mid,
         (unsigned long)clock_time(), rank_dag(), padre_id());
  LOG_INFO("publish %s %s\n", pub_topic, app_buffer);
}
/*---------------------------------------------------------------------------*/
static void
connect_to_broker(void)
{
  mqtt_connect(&conn, A2_BROKER_IP, A2_BROKER_PORT, A2_KEEP_ALIVE_S,
               MQTT_CLEAN_SESSION_ON);
  state = STATE_CONNECTING;
}
/*---------------------------------------------------------------------------*/
static void
state_machine(void)
{
  switch(state) {
  case STATE_INIT:
    snprintf(client_id, BUFFER_SIZE, "tel329-a2-mote%u", node_id);
    mqtt_register(&conn, &mqtt_cliente_process, client_id, mqtt_event,
                  A2_MAX_TCP_SEGMENT);
    conn.auto_reconnect = 0;
    connect_attempt = 1;
    state = STATE_REGISTERED;
    LOG_INFO("registrado como %s, MQTT v%d\n", client_id, MQTT_PROTOCOL_VERSION);
    /* Continua */
  case STATE_REGISTERED:
    if(have_connectivity()) {
      LOG_INFO("con ruta por defecto; conectando al broker (intento %u)\n",
               connect_attempt);
      connect_to_broker();
    }
    etimer_set(&publish_periodic_timer, NET_CONNECT_PERIODIC);
    return;
  case STATE_CONNECTING:
    break;
  case STATE_CONNECTED:
    /* Sin suscripcion: este cliente solo publica. */
    state = STATE_PUBLISHING;
    /* Continua */
  case STATE_PUBLISHING:
    if(timer_expired(&connection_life)) {
      connect_attempt = 0;
    }
    if(mqtt_ready(&conn) && conn.out_buffer_sent) {
      publish();
      etimer_set(&publish_periodic_timer, PUBLISH_INTERVAL);
      return;
    }
    /* Hay un PUBLISH en vuelo sin PUBACK: no se encola otro, se espera. */
    LOG_INFO("esperando PUBACK (estado MQTT=%d, cola llena=%u)\n",
             conn.state, conn.out_queue_full);
    break;
  case STATE_DISCONNECTED:
    mqtt_disconnect(&conn);
    connect_attempt++;
    {
      clock_time_t interval = connect_attempt < 3 ?
        RECONNECT_INTERVAL << connect_attempt : RECONNECT_INTERVAL << 3;
      LOG_WARN("desconectado; reintento %u en %lu ticks\n", connect_attempt,
               (unsigned long)interval);
      etimer_set(&publish_periodic_timer, interval);
    }
    state = STATE_REGISTERED;
    return;
  case STATE_ERROR:
  default:
    LOG_ERR("estado invalido 0x%02x\n", state);
    return;
  }

  etimer_set(&publish_periodic_timer, STATE_MACHINE_PERIODIC);
}
/*---------------------------------------------------------------------------*/
PROCESS_THREAD(mqtt_cliente_process, ev, data)
{
  PROCESS_BEGIN();

  printf("A2,START,%u,%lu\n", node_id, (unsigned long)clock_time());
  LOG_INFO("broker [%s]:%u tema %s cada %u s, QoS 1\n", A2_BROKER_IP,
           A2_BROKER_PORT, A2_TOPIC, A2_INTERVALO_S);

  state = STATE_INIT;
  etimer_set(&publish_periodic_timer, 0);

  while(1) {
    PROCESS_YIELD();
    if((ev == PROCESS_EVENT_TIMER && data == &publish_periodic_timer) ||
       ev == PROCESS_EVENT_POLL) {
      state_machine();
    }
  }

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
