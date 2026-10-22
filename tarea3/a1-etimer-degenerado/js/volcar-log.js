/*
 * Volcado del Mote output a COOJA.testlog, con el mismo formato que la
 * evidencia de la Tarea 1: <tiempo_us> n<id> <mensaje>.
 *
 * Cooja en --no-gui aborta con "No plugin controlling simulation" si nadie
 * maneja la simulacion: este script es ese alguien. Por eso existe.
 *
 * El corte por cantidad de lineas es el que importa en los casos degenerados:
 * con el etimer en 0 ticks el cliente genera miles de lineas por segundo
 * simulado.
 *
 * OJO con la primera linea: el limite de tiempo simulado tiene que ser un
 * ENTERO LITERAL. Cooja no evalua ese argumento, lo saca del texto del script
 * con una expresion regular que solo acepta digitos (ScriptParser.java:66). Si
 * se escribe una variable o una multiplicacion, no matchea, la llamada queda
 * en el codigo y revienta -- porque ahi ese nombre es una variable booleana,
 * no una funcion. El sintoma es un log con una sola linea.
 *
 * correr-casos.sh reescribe ese numero por caso. Tal cual, el archivo corre.
 */
TIMEOUT(120000, log.testOK());

var MAX_LINEAS = 4000;
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
