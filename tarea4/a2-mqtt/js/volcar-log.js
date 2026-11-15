/*
 * Volcado del Mote output a COOJA.testlog, mismo formato que las tareas
 * anteriores: <tiempo_us> n<id> <mensaje>. El tiempo es SIMULADO, en
 * microsegundos: de ahi salen t_publish y t_puback de la Actividad 3.
 *
 * Cooja en --no-gui aborta con "No plugin controlling simulation" si nadie
 * maneja la simulacion; este script es ese alguien.
 *
 * El argumento de TIMEOUT tiene que ser un ENTERO LITERAL (Cooja lo lee del
 * texto con una regex, ScriptParser.java). correr.sh lo reescribe segun la
 * duracion pedida. Tal cual, el archivo corre 300 s simulados.
 */
TIMEOUT(300000, log.testOK());

/* Tiempo real: el broker (mosquitto) y tunslip6 corren en el reloj del host. */
sim.setSpeedLimit(1.0);

var MAX_LINEAS = 60000;
var lineas = 0;

while(true) {
  log.log(time + " n" + id + " " + msg + "\n");
  lineas++;
  if(lineas >= MAX_LINEAS) {
    log.log("--- corte por limite de lineas: " + MAX_LINEAS + "\n");
    log.testOK();
  }
  YIELD();
}
