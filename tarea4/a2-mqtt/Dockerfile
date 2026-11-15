# Opcional: la misma imagen oficial con tcpdump ya instalado, para que
# correr.sh no tenga que bajarlo en cada corrida (mosquitto ya viene).
#
#   docker build -t tel329-a2 tarea4/a2-mqtt
#   docker run --rm --privileged ... tel329-a2 ./correr.sh
FROM contiker/contiki-ng
RUN sudo apt-get update -qq && sudo apt-get install -y -qq tcpdump && sudo rm -rf /var/lib/apt/lists/*
