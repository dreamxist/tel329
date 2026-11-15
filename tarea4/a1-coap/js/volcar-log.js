/*
 * Volcado del Mote output a COOJA.testlog (<tiempo_us> n<id> <mensaje>, igual
 * que las Tareas 1 y 3) MAS la captura del radio a un pcap.
 *
 * Cooja en --no-gui aborta con "No plugin controlling simulation" si nadie
 * maneja la simulacion: este script es ese alguien.
 *
 * El pcap: Cooja headless NO arranca el RadioLogger (es un VisPlugin, ver
 * tools/cooja/doc/ChangeLog.md "Avoid starting the AWT thread in headless
 * mode"), asi que su opcion "6LoWPAN Analyzer with PCAP" no sirve sin GUI.
 * Este script hace lo mismo que PcapExporter.java pero desde Nashorn: se
 * engancha a los triggers de transmision del radio medium y escribe cada
 * trama 802.15.4 con el mismo encabezado (magic a1b2c3d4, linktype 195,
 * timestamp = inicio de la transmision en tiempo simulado).
 *
 * OJO con la primera linea: el limite de tiempo tiene que ser un ENTERO
 * LITERAL en milisegundos; Cooja lo saca del texto con una regex
 * (ScriptParser.java). correr.sh reescribe ese numero y la ruta del pcap.
 */
TIMEOUT(600000, finalizar());

var PCAP = "__PCAP__";
if(PCAP.indexOf("__") === 0) {
  PCAP = "a1-coap.pcap"; /* relativo al cwd de Cooja si nadie lo reemplazo */
}
var CLIENTES = [2, 3, 4, 5, 6];
var RX_MINIMO = 25; /* respuestas por cliente para dar por terminada la corrida */

var FileOutputStream = Java.type("java.io.FileOutputStream");
var DataOutputStream = Java.type("java.io.DataOutputStream");
var BiConsumer = Java.type("java.util.function.BiConsumer");

var out = new DataOutputStream(new FileOutputStream(PCAP));
out.writeInt(0xa1b2c3d4 | 0); /* magic; |0 lo deja como int de 32 bits */
out.writeShort(2);           /* version mayor */
out.writeShort(4);           /* version menor */
out.writeInt(0);             /* thiszone */
out.writeInt(0);             /* sigfigs */
out.writeInt(4096);          /* snaplen */
out.writeInt(195);           /* LINKTYPE_IEEE802_15_4 (con FCS segun Wireshark; Cooja no la incluye) */
out.flush();

var tramas = 0;
var medio = sim.getRadioMedium();
var trigger = new BiConsumer(function(evento, obj) {
  /* lastConnection se anula en TRANSMISSION_STARTED y se fija justo antes
     de TRANSMISSION_FINISHED: con esto basta para contar cada trama una vez. */
  var conn = medio.getLastConnection();
  if(conn === null) {
    return;
  }
  var pkt = conn.getSource().getLastPacketTransmitted();
  if(pkt === null) {
    return;
  }
  var data = pkt.getPacketData();
  var ts = conn.getStartTime(); /* microsegundos simulados */
  out.writeInt(Math.floor(ts / 1000000));
  out.writeInt(ts % 1000000);
  out.writeInt(data.length);
  out.writeInt(data.length);
  out.write(data, 0, data.length);
  out.flush();
  tramas++;
});
medio.getRadioTransmissionTriggers().addTrigger(global, trigger);

var rx = {};
for(var i = 0; i < CLIENTES.length; i++) {
  rx[CLIENTES[i]] = 0;
}

function todosListos() {
  for(var i = 0; i < CLIENTES.length; i++) {
    if(rx[CLIENTES[i]] < RX_MINIMO) {
      return false;
    }
  }
  return true;
}

function finalizar() {
  try {
    medio.getRadioTransmissionTriggers().deleteTriggers(global);
    out.close();
  } catch(e) {
    log.log("--- no pude cerrar el pcap: " + e + "\n");
  }
  log.log("--- fin: " + tramas + " tramas en " + PCAP + ", tiempo simulado " + time + " us\n");
  log.testOK();
}

while(true) {
  log.log(time + " n" + id + " " + msg + "\n");
  if(msg.indexOf("] RX id=") >= 0 && rx[id] !== undefined) {
    rx[id]++;
    if(todosListos()) {
      finalizar();
    }
  }
  YIELD();
}
