# Enrutador 6LoWPAN (`rpl-border-router`)

El enrutador es el `examples/rpl-border-router` de Contiki-NG tal cual viene en el árbol (no se copia ni se
modifica: la Actividad 4 lo da por "ya estudiado"). Acá quedan las dos formas de levantarlo y el puente TCP
que usamos para que el middleware corra en el Mac.

## Opción A: Cooja + Serial Socket + tunslip6 (red 6LoWPAN simulada)

Es la topología que pide el enunciado: sensor y border router como motes en Cooja, y `tunslip6` sacando el
tráfico al mundo exterior por `tun0`. Resumen del tutorial oficial
(`~/contiki-ng/doc/tutorials/Cooja-simulating-a-border-router.md`):

1. Abrir Cooja dentro del contenedor (`contiker` con X11) y crear una simulación.
2. Agregar un tipo de mote con `examples/rpl-border-router/border-router.c` (p. ej. plataforma Cooja o Zoul) y otro con
   `tarea4/a4-azure/sensor/sensor-azure.c`; poner un border router y uno o más sensores en rango.
3. Click derecho sobre el border router > *More tools...* > *Serial Socket (SERVER)*; puerto por defecto `60001`; *Start*.
4. Arrancar la simulación.
5. En otra terminal del contenedor:
   ```bash
   cd /home/user/contiki-ng/examples/rpl-border-router
   make TARGET=zoul connect-router-cooja     # corre tunslip6 -a 127.0.0.1 -p 60001 y crea tun0 con fd00::1/64
   ```
6. Cuando RPL converge, cada sensor queda alcanzable desde el contenedor como `fd00::2xx:x:x:x` (IID derivado del
   link-layer address del mote). El border router expone en `http://[fd00::<br>]/` la lista de vecinos y rutas (ese es
   el webserver `httpd-simple` que Kurniawan reusa como *websense*).
7. `curl -g 'http://[fd00::<sensor>]/'` devuelve el JSON del sensor y el middleware apunta `SENSOR_URL` ahí.

## Opción B: TARGET=native con tun (lo que se verificó en esta entrega)

Cooja necesita X11 y una sesión interactiva; para verificar la cadena de forma reproducible el sensor se compiló
para `TARGET=native`, donde la plataforma nativa de Contiki-NG crea ella misma el `tun0` (prefijo `fd00::1/64`)
y el mote queda en `fd00::302:304:506:708` (IID de la MAC por defecto `01:02:...:08` con el bit U/L invertido).
En este modo el host **es** el enrutador: el stack uIP del sensor habla IPv6 directamente con el kernel del
contenedor por el tun, y no hay salto 6LoWPAN/RPL (RPL Lite se compila igual, pero el nodo no tiene vecinos radio).

```bash
# desde la raíz del worktree
docker run -d --name a4sensor --privileged -p 127.0.0.1:8080:8080 -e CCACHE_DISABLE=1 \
  -v ~/contiki-ng:/home/user/contiki-ng -v "$PWD":/work -w /work/tarea4/a4-azure/sensor \
  contiker/contiki-ng sleep infinity
docker exec a4sensor make CONTIKI=/home/user/contiki-ng TARGET=native
docker exec -d a4sensor sh -c './build/native/sensor-azure.native > /work/tarea4/evidencia/a4-sensor.log 2>&1'
docker exec a4sensor ping6 -c 3 fd00::302:304:506:708
docker exec a4sensor python3 -c "import urllib.request as u; print(u.urlopen('http://[fd00::302:304:506:708]/').read().decode())"
```

El border router también compila para native en el contenedor (`a4-border-router-build.log`); se usaría con un
`slip-radio` real, que no tenemos.

### Puente TCP hacia el Mac (`tcp-forward.py`)

El tun vive dentro del contenedor y Node 22 vive en el Mac, así que el middleware no puede alcanzar
`fd00::302:...` directo. `tcp-forward.py` es un reenvío TCP de 30 líneas que escucha en `0.0.0.0:8080` (publicado
con `-p 127.0.0.1:8080:8080`) y abre una conexión al mote por cada conexión entrante; HTTP pasa intacto.

```bash
docker exec -d a4sensor python3 /work/tarea4/a4-azure/router/tcp-forward.py 8080 fd00::302:304:506:708 80
curl -s -i http://127.0.0.1:8080/          # desde el Mac
```

Con Cooja/tunslip6 (opción A) el puente es igual, cambiando la IPv6 destino por la del sensor en la red simulada.
