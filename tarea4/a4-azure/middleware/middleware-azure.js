// TEL329 - Tarea 4, Actividad 4: middleware sensor Contiki-NG -> Azure IoT Hub.
// Adaptación moderna del middleware-azure.js de Kurniawan (2018): sin `request`
// (deprecado) ni Express (no aportaba nada al flujo), con fetch nativo de Node 22,
// API de promesas del SDK azure-iot-device y credenciales por variables de entorno.
import iotDevice from 'azure-iot-device';
import iotMqtt from 'azure-iot-device-mqtt';

// Los SDK de Azure IoT son CommonJS: desde ESM se toma el default y se desestructura.
const { Client, Message } = iotDevice;
const { Mqtt } = iotMqtt;

const dryRun = process.argv.includes('--dry-run');
const sensorUrl = process.env.SENSOR_URL;
const connectionString = process.env.AZURE_DEVICE_CONNECTION_STRING;
const pollMs = Number(process.env.POLL_MS ?? 3000);
const TEMPERATURE_ALERT_C = 30;

if (!sensorUrl) {
  console.error('Falta SENSOR_URL (ej. http://[fd00::302:304:506:708]/). Ver .env.example');
  process.exit(1);
}
if (!dryRun && !connectionString) {
  console.error('Falta AZURE_DEVICE_CONNECTION_STRING. Ver .env.example, o corré con --dry-run para probar solo sensor -> middleware.');
  process.exit(1);
}

// El deviceId sale de la propia cadena de conexión (DeviceId=...) para no duplicarlo.
const deviceId = /DeviceId=([^;]+)/.exec(connectionString ?? '')?.[1] ?? 'contiki-ng-01';

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

function buildMessage({ temperature, humidity }) {
  const data = JSON.stringify({ deviceId, temperature, humidity });
  const message = new Message(data);
  message.properties.add('temperatureAlert', temperature > TEMPERATURE_ALERT_C ? 'true' : 'false');
  message.contentType = 'application/json';
  message.contentEncoding = 'utf-8';
  return message;
}

async function tick(client) {
  let reading;
  try {
    reading = await readSensor();
  } catch (err) {
    console.error(`[sensor] ${err.name === 'AbortError' ? 'timeout' : err.message}`);
    return;
  }
  console.log(`[sensor] ${sensorUrl} -> ${reading.raw}`);
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
}

async function main() {
  let client = null;
  if (dryRun) {
    console.log(`Modo --dry-run: sin conexión a Azure. deviceId=${deviceId} sensor=${sensorUrl} cada ${pollMs} ms`);
  } else {
    client = Client.fromConnectionString(connectionString, Mqtt);
    await client.open();
    console.log(`Conectado a IoT Hub por MQTT como ${deviceId}; consultando ${sensorUrl} cada ${pollMs} ms`);
  }
  console.log('Contiki-NG Azure Middleware started.');
  await tick(client);
  const interval = setInterval(() => tick(client), pollMs);
  const stop = async () => {
    clearInterval(interval);
    if (client) await client.close().catch(() => {});
    process.exit(0);
  };
  process.on('SIGINT', stop);
  process.on('SIGTERM', stop);
}

main().catch((err) => {
  console.error(`No se pudo iniciar: ${err.message}`);
  process.exit(1);
});
