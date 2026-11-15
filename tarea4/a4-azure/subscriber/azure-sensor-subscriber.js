// TEL329 - Tarea 4, Actividad 4: suscriptor a la telemetría del IoT Hub.
// Reemplaza el paquete `azure-event-hubs` (archivado) de Kurniawan (2018) por
// @azure/event-hubs v6: EventHubConsumerClient se suscribe a todas las particiones
// del endpoint compatible con Event Hubs del IoT Hub y recibe desde "ahora".
import { EventHubConsumerClient, latestEventPosition } from '@azure/event-hubs';

const connectionString = process.env.EVENTHUB_COMPATIBLE_CONNECTION_STRING;
const consumerGroup = process.env.EVENTHUB_CONSUMER_GROUP || '$Default';

if (!connectionString) {
  console.error('Falta EVENTHUB_COMPATIBLE_CONNECTION_STRING (IoT Hub > Built-in endpoints > Event Hub compatible endpoint). Ver .env.example');
  process.exit(1);
}
if (!/^Endpoint=sb:\/\//.test(connectionString) || !/EntityPath=/.test(connectionString)) {
  console.error('La cadena no parece la del endpoint compatible con Event Hubs: debe empezar con Endpoint=sb:// e incluir EntityPath=<hub>. No sirve la cadena del dispositivo (HostName=...).');
  process.exit(1);
}

const client = new EventHubConsumerClient(consumerGroup, connectionString);

const subscription = client.subscribe(
  {
    processEvents: async (events, context) => {
      for (const event of events) {
        console.log(`Message received (partition ${context.partitionId}, deviceId=${event.systemProperties?.['iothub-connection-device-id'] ?? '?'}):`);
        console.log(JSON.stringify(event.body));
        if (event.properties && Object.keys(event.properties).length > 0) {
          console.log(`properties: ${JSON.stringify(event.properties)}`);
        }
        console.log('');
      }
    },
    processError: async (err, context) => {
      console.error(`Error en partición ${context.partitionId ?? '?'}: ${err.message}`);
    },
  },
  { startPosition: latestEventPosition },
);

console.log(`Suscrito al IoT Hub (consumer group ${consumerGroup}); esperando mensajes... Ctrl+C para salir.`);

const stop = async () => {
  await subscription.close();
  await client.close();
  process.exit(0);
};
process.on('SIGINT', stop);
process.on('SIGTERM', stop);
