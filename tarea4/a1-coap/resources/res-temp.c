/*
 * TEL329 - Tarea 4, Actividad 1: el unico recurso del servidor, sensores/temp.
 *
 * Los clientes lo notifican con POST (o PUT) confirmable y payload de texto
 * "id=<nodo> seq=<k> temp=<xx.x>". El handler registra lo que llega con el
 * TTL del paquete IPv6 -- de ahi salen los saltos: cada router que reenvia
 * descuenta 1 del hop limit inicial (UIP_TTL = 64), asi que
 * saltos = UIP_TTL - ttl_recibido + 1 -- y responde 2.04 Changed (RFC 7252
 * 5.9.1.3) con el conteo de saltos en el payload, para que el cliente lo deje
 * en su propio log.
 *
 * El GET devuelve la ultima lectura registrada, por si se consulta el recurso
 * desde afuera (coap-cli, Copper) o desde el shell.
 *
 * UIP_IP_BUF sigue apuntando al datagrama recibido mientras corre el handler:
 * coap_receive() lo llama desde el evento tcpip_event, antes de serializar y
 * enviar la respuesta (que es cuando uip_buf se reutiliza).
 */
#include "contiki.h"
#include "coap-engine.h"
#include "net/ipv6/uip.h"
#include "sys/log.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LOG_MODULE "A1raiz"
#define LOG_LEVEL LOG_LEVEL_APP

static void res_post_handler(coap_message_t *request, coap_message_t *response,
                             uint8_t *buffer, uint16_t preferred_size, int32_t *offset);
static void res_get_handler(coap_message_t *request, coap_message_t *response,
                            uint8_t *buffer, uint16_t preferred_size, int32_t *offset);

RESOURCE(res_temp,
         "title=\"Temperatura de los sensores\";rt=\"Temperature\";ct=0",
         res_get_handler,
         res_post_handler,
         res_post_handler,
         NULL);

static uint32_t total_recibidas;
static char ultima[48] = "sin lecturas";

static void
res_post_handler(coap_message_t *request, coap_message_t *response,
                 uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  const uint8_t *payload = NULL;
  int len = coap_get_payload(request, &payload);
  uint8_t ttl = UIP_IP_BUF->ttl;
  unsigned saltos = (unsigned)(UIP_TTL - ttl) + 1;

  if(len <= 0 || payload == NULL) {
    coap_set_status_code(response, BAD_REQUEST_4_00);
    return;
  }
  if(len >= (int)sizeof(ultima)) {
    len = sizeof(ultima) - 1;
  }
  memcpy(ultima, payload, len);
  ultima[len] = '\0';
  total_recibidas++;

  /* Linea parseable: REG,<payload tal cual>,hops=<n>,ttl=<t>,mid=<mid>,total=<n> */
  LOG_INFO("REG %s hops=%u ttl=%u mid=%u total=%" PRIu32 "\n",
           ultima, saltos, ttl, request->mid, total_recibidas);

  snprintf((char *)buffer, preferred_size, "ok hops=%u", saltos);
  coap_set_header_content_format(response, TEXT_PLAIN);
  coap_set_payload(response, buffer, strlen((char *)buffer));
  coap_set_status_code(response, CHANGED_2_04);
}

static void
res_get_handler(coap_message_t *request, coap_message_t *response,
                uint8_t *buffer, uint16_t preferred_size, int32_t *offset)
{
  snprintf((char *)buffer, preferred_size, "%s total=%" PRIu32, ultima, total_recibidas);
  coap_set_header_content_format(response, TEXT_PLAIN);
  coap_set_payload(response, buffer, strlen((char *)buffer));
}
