/*
 * TEL329 - Tarea 4, Actividad 1: la raiz del DODAG, que ademas es el servidor
 * CoAP de la red.
 *
 * Nodo 1 en el .csc (misma convencion que la Tarea 3). Arranca RPL Lite como
 * raiz y activa un solo recurso, sensores/temp (resources/res-temp.c), que
 * los cinco clientes notifican con POST confirmable. El motor CoAP (proceso
 * coap_engine, puerto 5683) lo inicia contiki-main al incluir el modulo coap;
 * aca solo se enlaza el recurso a su Uri-Path.
 */
#include "contiki.h"
#include "contiki-net.h"
#include "coap-engine.h"
#include "net/routing/routing.h"
#include "net/ipv6/uip-ds6.h"
#include "sys/node-id.h"
#include "sys/log.h"

#define LOG_MODULE "A1raiz"
#define LOG_LEVEL LOG_LEVEL_APP

extern coap_resource_t res_temp;

PROCESS(root_coap_process, "Raiz RPL + servidor CoAP");
AUTOSTART_PROCESSES(&root_coap_process);

PROCESS_THREAD(root_coap_process, ev, data)
{
  static struct etimer et;
  uip_ipaddr_t raiz;

  PROCESS_BEGIN();

  NETSTACK_ROUTING.root_start();
  coap_activate_resource(&res_temp, "sensores/temp");

  LOG_INFO("nodo %u es raiz del DODAG y servidor CoAP (coap://[...]:%u/sensores/temp)\n",
           node_id, COAP_DEFAULT_PORT);

  /* La direccion global tarda un instante en quedar lista; se imprime una vez. */
  etimer_set(&et, 2 * CLOCK_SECOND);
  PROCESS_WAIT_EVENT_UNTIL(etimer_expired(&et));
  if(NETSTACK_ROUTING.get_root_ipaddr(&raiz)) {
    LOG_INFO("direccion de la raiz: ");
    LOG_INFO_6ADDR(&raiz);
    LOG_INFO_("\n");
  }

  while(1) {
    PROCESS_WAIT_EVENT();
  }

  PROCESS_END();
}
