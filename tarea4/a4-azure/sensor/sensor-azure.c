/*
 * TEL329 - Tarea 4, Actividad 4: mote sensor para Azure IoT Hub.
 *
 * Adaptación del "websense-Cloud" de Kurniawan (2018), Practical Contiki-NG,
 * cap. 8. El original usaba httpd-simple.c copiado al proyecto; Contiki-NG
 * trae en el árbol os/net/app-layer/httpd-ws, que expone el mismo modelo
 * (script como protothread + SEND_STRING) y pide al proyecto definir
 * httpd_ws_get_script(). Responde a cualquier GET con
 * {"temp":<15..39>,"hum":<80..89>} generado con random_rand().
 */
#include "contiki.h"
#include "contiki-net.h"
#include "lib/random.h"
#include "httpd-ws.h"

#include <stdio.h>
#include <string.h>

#include "sys/log.h"
#define LOG_MODULE "Sensor"
#define LOG_LEVEL LOG_LEVEL_INFO

static const char json_content_type[] = "application/json";
static char json_buf[40];

/*---------------------------------------------------------------------------*/
static
PT_THREAD(generate_json(struct httpd_ws_state *s))
{
  PSOCK_BEGIN(&s->sout);

  {
    unsigned temperature = 15 + (random_rand() % 25);
    unsigned humidity = 80 + (random_rand() % 10);
    snprintf(json_buf, sizeof(json_buf),
             "{\"temp\":%u,\"hum\":%u}", temperature, humidity);
    LOG_INFO("GET %s -> %s\n", s->filename, json_buf);
  }

  SEND_STRING(&s->sout, json_buf, strlen(json_buf));

  PSOCK_END(&s->sout);
}
/*---------------------------------------------------------------------------*/
/* Hook que httpd-ws espera del proyecto: elige el script segun la ruta.
 * Cualquier ruta devuelve el JSON; asi el middleware no depende del path. */
httpd_ws_script_t
httpd_ws_get_script(struct httpd_ws_state *s)
{
  s->content_type = json_content_type;
  return generate_json;
}
/*---------------------------------------------------------------------------*/
PROCESS(sensor_azure_process, "Sensor Azure (httpd-ws)");
AUTOSTART_PROCESSES(&sensor_azure_process);

PROCESS_THREAD(sensor_azure_process, ev, data)
{
  PROCESS_BEGIN();

  process_start(&httpd_ws_process, NULL);
  LOG_INFO("HTTP server listening on port 80, serving {\"temp\",\"hum\"}\n");

  PROCESS_END();
}
/*---------------------------------------------------------------------------*/
