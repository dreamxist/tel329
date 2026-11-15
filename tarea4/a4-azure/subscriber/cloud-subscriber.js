// TEL329 - Tarea 4, Actividad 4: suscriptor a la "nube" local (broker MQTT).
// Cumple el rol de azure-sensor-subscriber.js cuando el destino del middleware es
// `local`: se suscribe a la telemetría de todos los dispositivos en Mosquitto e
// imprime cada mensaje con su hora de llegada y la latencia middleware -> suscriptor
// (llegada menos el `timestamp` que el middleware pone al publicar). Al salir
// (Ctrl+C / SIGTERM) imprime n, mediana y máximo de esas latencias.
import mqtt from 'mqtt';

const mqttUrl = process.env.MQTT_URL ?? 'mqtt://127.0.0.1:1883';
const topicFilter = process.env.MQTT_TOPIC ?? 'usm/tel329/cloud/+/telemetry';
const latencias = [];

const client = await mqtt.connectAsync(mqttUrl, {
  protocolVersion: 5,
  clientId: `cloud-subscriber-${process.pid}`,
  clean: true,
});
await client.subscribeAsync(topicFilter, { qos: 1 });
console.log(`Suscrito a ${topicFilter} en ${mqttUrl} (MQTT 5, QoS 1); esperando mensajes... Ctrl+C para salir.`);

client.on('message', (topic, payload, packet) => {
  const llegada = Date.now();
  const deviceId = topic.split('/')[3];
  let latencia = 'n/d';
  try {
    const body = JSON.parse(payload.toString());
    const t = Date.parse(body.timestamp);
    if (Number.isFinite(t)) {
      latencias.push(llegada - t);
      latencia = `${llegada - t} ms`;
    }
  } catch {
    // cuerpo no JSON: se imprime igual, sin latencia
  }
  console.log(`Message received ${new Date(llegada).toISOString()} (topic ${topic}, deviceId=${deviceId}, latencia ${latencia}):`);
  console.log(payload.toString());
  const props = packet.properties?.userProperties;
  if (props && Object.keys(props).length > 0) console.log(`properties: ${JSON.stringify(props)}`);
  console.log('');
});
client.on('error', (err) => console.error(`[mqtt] ${err.message}`));

function resumen() {
  if (latencias.length === 0) return 'sin mensajes';
  const s = [...latencias].sort((a, b) => a - b);
  const mid = s.length >> 1;
  const mediana = s.length % 2 ? s[mid] : (s[mid - 1] + s[mid]) / 2;
  const media = s.reduce((a, b) => a + b, 0) / s.length;
  return `n=${s.length} min=${s[0]} mediana=${mediana} media=${media.toFixed(2)} max=${s[s.length - 1]} (ms, resolución 1 ms)`;
}

const stop = async () => {
  console.log(`Latencia middleware -> suscriptor: ${resumen()}`);
  await client.endAsync().catch(() => {});
  process.exit(0);
};
process.on('SIGINT', stop);
process.on('SIGTERM', stop);
