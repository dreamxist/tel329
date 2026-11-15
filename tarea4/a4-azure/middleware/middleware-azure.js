// TEL329 - Tarea 4, Actividad 4: middleware sensor Contiki-NG -> nube.
// Adaptación moderna del middleware-azure.js de Kurniawan (2018): sin `request`
// (deprecado) ni Express (no aportaba nada al flujo), con fetch nativo de Node 22,
// API de promesas del SDK azure-iot-device y credenciales por variables de entorno.
//
// Dos destinos (--destino o DESTINO):
//   azure (por defecto)  azure-iot-device-mqtt -> Azure IoT Hub, como el libro.
//   local                paquete `mqtt` (MQTT 5, QoS 1) -> broker Mosquitto en MQTT_URL,
//                        que cumple el rol del IoT Hub: recibe la telemetría
//                        device-to-cloud y se la entrega al suscriptor.
import iotDevice from 'azure-iot-device';
import iotMqtt from 'azure-iot-device-mqtt';
import mqtt from 'mqtt';

// Los SDK de Azure IoT son CommonJS: desde ESM se toma el default y se desestructura.
const { Client, Message } = iotDevice;
const { Mqtt } = iotMqtt;

function argValue(name) {
  const i = process.argv.indexOf(name);
  if (i !== -1) return process.argv[i + 1];
  return process.argv.find((a) => a.startsWith(`${name}=`))?.split('=')[1];
}

const dryRun = process.argv.includes('--dry-run');
const destino = argValue('--destino') ?? process.env.DESTINO ?? 'azure';
const sensorUrl = process.env.SENSOR_URL;
const connectionString = process.env.AZURE_DEVICE_CONNECTION_STRING;
const mqttUrl = process.env.MQTT_URL ?? 'mqtt://127.0.0.1:1883';
const pollMs = Number(process.env.POLL_MS ?? 3000);
const TEMPERATURE_ALERT_C = 30;

if (destino !== 'azure' && destino !== 'local') {
  console.error(`Destino desconocido: ${destino}. Usar --destino azure|local`);
  process.exit(1);
}
if (!sensorUrl) {
  console.error('Falta SENSOR_URL (ej. http://[fd00::302:304:506:708]/). Ver .env.example');
  process.exit(1);
}
if (destino === 'azure' && !dryRun && !connectionString) {
  console.error('Falta AZURE_DEVICE_CONNECTION_STRING. Ver .env.example, o corré con --dry-run para probar solo sensor -> middleware, o con --destino local para el broker MQTT local.');
  process.exit(1);
}

// El deviceId sale de la propia cadena de conexión (DeviceId=...) para no duplicarlo.
const deviceId = /DeviceId=([^;]+)/.exec(connectionString ?? '')?.[1] ?? process.env.DEVICE_ID ?? 'contiki-ng-01';
const topic = `usm/tel329/cloud/${deviceId}/telemetry`;

async function readSensor() {
  const controller = new AbortController();
  const timer = setTimeout(() => controller.abort(), Math.min(pollMs, 2500));
  try {
    const res = await fetch(sensorUrl, { signal: controller.signal });
    if (!res.ok) throw new Error(`HTTP ${res.status} desde el sensor`);
    const body = await res.text();
    const obj = JSON.parse(body);
    if (typeof obj.temp !== 'number' || typeof obj.hum !== 'number') {
      throw new Error(`JSON inesperado del sensor: ${body}`);
    }
    return { raw: body, temperature: obj.temp, humidity: obj.hum };
  } finally {
    clearTimeout(timer);
  }
}

const alertOf = (temperature) => (temperature > TEMPERATURE_ALERT_C ? 'true' : 'false');

function buildMessage({ temperature, humidity }) {
  const data = JSON.stringify({ deviceId, temperature, humidity });
  const message = new Message(data);
  message.properties.add('temperatureAlert', alertOf(temperature));
  message.contentType = 'application/json';
  message.contentEncoding = 'utf-8';
  return message;
}

// Sumidero Azure IoT Hub: el comportamiento original, sin cambios.
async function openAzure() {
  let client = null;
  if (dryRun) {
    console.log(`Modo --dry-run: sin conexión a Azure. deviceId=${deviceId} sensor=${sensorUrl} cada ${pollMs} ms`);
  } else {
    client = Client.fromConnectionString(connectionString, Mqtt);
    await client.open();
    console.log(`Conectado a IoT Hub por MQTT como ${deviceId}; consultando ${sensorUrl} cada ${pollMs} ms`);
  }
  return {
    async send(reading) {
      const message = buildMessage(reading);
      const alert = message.properties.getValue('temperatureAlert');
      if (dryRun) {
        console.log(`[dry-run] enviaría a IoT Hub: ${message.getData()} properties={temperatureAlert:${alert}}`);
        return;
      }
      try {
        await client.sendEvent(message);
        console.log(`[iothub] enviado: ${message.getData()} temperatureAlert=${alert}`);
      } catch (err) {
        console.error(`[iothub] error al enviar: ${err.message}`);
      }
    },
    close: async () => client?.close().catch(() => {}),
  };
}

// Sumidero local: MQTT 5 hacia Mosquitto. temperatureAlert va como user property
// (el análogo MQTT 5 de la propiedad de aplicación de IoT Hub: fuera del cuerpo).
async function openLocal() {
  if (dryRun) {
    console.log(`Modo --dry-run: sin conexión al broker. tema=${topic} sensor=${sensorUrl} cada ${pollMs} ms`);
    return {
      async send({ temperature, humidity }) {
        const data = JSON.stringify({ deviceId, temperature, humidity, timestamp: new Date().toISOString() });
        console.log(`[dry-run] publicaría en ${topic}: ${data} userProperties={temperatureAlert:${alertOf(temperature)}}`);
      },
      close: async () => {},
    };
  }
  const client = await mqtt.connectAsync(mqttUrl, {
    protocolVersion: 5,
    clientId: `middleware-${deviceId}`,
    clean: true,
    reconnectPeriod: 1000,
  });
  client.on('error', (err) => console.error(`[mqtt] ${err.message}`));
  console.log(`Conectado a ${mqttUrl} (MQTT 5) como middleware-${deviceId}; tema ${topic}; consultando ${sensorUrl} cada ${pollMs} ms`);
  return {
    async send({ temperature, humidity }) {
      const alert = alertOf(temperature);
      const data = JSON.stringify({ deviceId, temperature, humidity, timestamp: new Date().toISOString() });
      const t0 = performance.now();
      try {
        const packet = await client.publishAsync(topic, data, {
          qos: 1,
          properties: { contentType: 'application/json', userProperties: { temperatureAlert: alert } },
        });
        const ms = (performance.now() - t0).toFixed(2);
        console.log(`[mqtt] PUBLISH mid=${packet?.messageId ?? '?'} ${data} temperatureAlert=${alert} -> PUBACK en ${ms} ms`);
      } catch (err) {
        console.error(`[mqtt] error al publicar: ${err.message}`);
      }
    },
    close: async () => client.endAsync().catch(() => {}),
  };
}

async function main() {
  const sink = destino === 'local' ? await openLocal() : await openAzure();
  console.log(`Contiki-NG ${destino === 'local' ? 'local MQTT' : 'Azure'} Middleware started.`);
  const tick = async () => {
    let reading;
    try {
      reading = await readSensor();
    } catch (err) {
      console.error(`[sensor] ${err.name === 'AbortError' ? 'timeout' : err.message}`);
      return;
    }
    console.log(`[sensor] ${sensorUrl} -> ${reading.raw}`);
    await sink.send(reading);
  };
  await tick();
  const interval = setInterval(tick, pollMs);
  const stop = async () => {
    clearInterval(interval);
    await sink.close();
    process.exit(0);
  };
  process.on('SIGINT', stop);
  process.on('SIGTERM', stop);
}

main().catch((err) => {
  console.error(`No se pudo iniciar: ${err.message}`);
  process.exit(1);
});
