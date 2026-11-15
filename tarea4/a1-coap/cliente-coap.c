/*
 * TEL329 - Tarea 4, Actividad 1: el cliente que sensa y notifica por CoAP.
 *
 * Nodos 2..6 en el .csc. Se une al DODAG (RPL Lite) y, cuando tiene ruta a la
 * raiz, cada SEND_INTERVAL manda un POST confirmable a coap://[raiz]/sensores/temp
 * con "id=<nodo> seq=<k> temp=<xx.x>", la temperatura con random_rand().
 *
 * Se usa la API no bloqueante (coap_send_request, coap-callback-api.h) en vez
 * de COAP_BLOCKING_REQUEST: el callback llega con el instante de la respuesta
 * y permite medir el tiempo de respuesta (RTT CoAP) sin que el proceso quede
 * clavado en la macro. Hay UNA peticion en vuelo por cliente: si la anterior
 * aun no termino (CoAP retransmite el CON hasta COAP_MAX_RETRANSMIT veces
 * antes de darse por vencido, RFC 7252 4.2), la siguiente ronda se salta y se
 * registra.
 *
 * Marcas de tiempo: TX y RX se imprimen como lineas separadas para que Cooja
 * les anteponga el tiempo simulado en microsegundos (de ahi sale el CSV); el
 * rtt_ms que imprime el mote sale de clock_time() y sirve de verificacion.
 */
#include "contiki.h"
#include "contiki-net.h"
#include "coap-engine.h"
#include "coap-callback-api.h"
#include "coap-log.h"
#include "net/routing/routing.h"
#include "sys/node-id.h"
#include "random.h"
#include "sys/log.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define LOG_MODULE "A1cli"
#define LOG_LEVEL LOG_LEVEL_APP

#define SEND_INTERVAL (10 * CLOCK_SECOND)
#define URI_TEMP "sensores/temp"

/* Temperatura en decimas de grado, uniforme en [15.0, 44.9]. */
#define TEMP_MIN_DECIMAS 150
#define TEMP_RANGO_DECIMAS 300

static coap_endpoint_t server_ep;
static coap_message_t request[1];
static coap_callback_request_state_t req_state;
static char payload[48];

static uint32_t seq;
static uint32_t ok_count, timeout_count, skipped_count;
static clock_time_t t_envio;
static uint8_t pendiente;

static unsigned
leer_temperatura_decimas(void)
{
  return TEMP_MIN_DECIMAS + (random_rand() % TEMP_RANGO_DECIMAS);
}

static void
respuesta_cb(coap_callback_request_state_t *cb_state)
{
  coap_request_state_t *st = &cb_state->state;
  clock_time_t ahora = clock_time();
  unsigned long rtt_ms = (unsigned long)((ahora - t_envio) * 1000UL / CLOCK_SECOND);

  switch(st->status) {
  case COAP_REQUEST_STATUS_RESPONSE: {
    coap_message_t *resp = st->response;
    const uint8_t *chunk = NULL;
    int len = coap_get_payload(resp, &chunk);
    /* RX id=<n> seq=<k> code=<c.dd> mid=<mid> rtt_ms=<ms> '<payload>' */
    LOG_INFO("RX id=%u seq=%" PRIu32 " code=%u.%02u mid=%u rtt_ms=%lu '%.*s'\n",
             node_id, seq, resp->code >> 5, resp->code & 0x1f, resp->mid,
             rtt_ms, len > 0 ? len : 0, chunk ? (const char *)chunk : "");
    ok_count++;
    break;
  }
  case COAP_REQUEST_STATUS_TIMEOUT:
    LOG_INFO("TIMEOUT id=%u seq=%" PRIu32 " tras %lu ms\n", node_id, seq, rtt_ms);
    timeout_count++;
    pendiente = 0;
    break;
  case COAP_REQUEST_STATUS_FINISHED:
    /* Llega despues de RESPONSE, con response == NULL: libera el turno. */
    pendiente = 0;
    break;
  default:
    LOG_WARN("estado inesperado %d en seq=%" PRIu32 "\n", st->status, seq);
    pendiente = 0;
    break;
  }
}

PROCESS(cliente_coap_process, "Cliente CoAP sensor de temperatura");
AUTOSTART_PROCESSES(&cliente_coap_process);

PROCESS_THREAD(cliente_coap_process, ev, data)
{
  static struct etimer et;
  static uint8_t anunciado;
  uip_ipaddr_t raiz;

  PROCESS_BEGIN();

  LOG_INFO("nodo %u cliente CoAP, recurso %s cada %u s\n",
           node_id, URI_TEMP, (unsigned)(SEND_INTERVAL / CLOCK_SECOND));

  /* Arranque desfasado para que los cinco no disparen a la vez. */
  etimer_set(&et, CLOCK_SECOND + (random_rand() % SEND_INTERVAL));

  while(1) {
    PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
    etimer_set(&et, SEND_INTERVAL);

    if(!NETSTACK_ROUTING.node_is_reachable() ||
       !NETSTACK_ROUTING.get_root_ipaddr(&raiz)) {
      LOG_INFO("todavia sin ruta a la raiz\n");
      continue;
    }

    if(pendiente) {
      skipped_count++;
      LOG_INFO("SKIP id=%u seq=%" PRIu32 " sigue en vuelo (saltadas=%" PRIu32 ")\n",
               node_id, seq, skipped_count);
      continue;
    }

    /* El endpoint del servidor es la raiz que RPL ya conoce: nada hardcodeado. */
    uip_ipaddr_copy(&server_ep.ipaddr, &raiz);
    server_ep.port = UIP_HTONS(COAP_DEFAULT_PORT);
    server_ep.secure = 0;
    if(!anunciado) {
      LOG_INFO("servidor CoAP: ");
      LOG_INFO_COAP_EP(&server_ep);
      LOG_INFO_("\n");
      anunciado = 1;
    }

    seq++;
    unsigned t = leer_temperatura_decimas();
    snprintf(payload, sizeof(payload), "id=%u seq=%" PRIu32 " temp=%u.%u",
             node_id, seq, t / 10, t % 10);

    coap_init_message(request, COAP_TYPE_CON, COAP_POST, 0);
    coap_set_header_uri_path(request, URI_TEMP);
    coap_set_header_content_format(request, TEXT_PLAIN);
    coap_set_payload(request, (uint8_t *)payload, strlen(payload));

    t_envio = clock_time();
    if(coap_send_request(&req_state, &server_ep, request, respuesta_cb)) {
      pendiente = 1;
      /* TX id=<n> seq=<k> mid=<mid> len=<bytes> '<payload>' */
      LOG_INFO("TX id=%u seq=%" PRIu32 " mid=%u len=%u '%s'\n",
               node_id, seq, request->mid, (unsigned)strlen(payload), payload);
    } else {
      LOG_WARN("sin transaccion libre para seq=%" PRIu32 "\n", seq);
    }

    if(seq % 10 == 0) {
      LOG_INFO("resumen id=%u enviadas=%" PRIu32 " ok=%" PRIu32 " timeout=%" PRIu32
               " saltadas=%" PRIu32 "\n", node_id, seq, ok_count, timeout_count,
               skipped_count);
    }
  }

  PROCESS_END();
}
