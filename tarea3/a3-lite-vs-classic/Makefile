CONTIKI_PROJECT = udp-server udp-client
all: $(CONTIKI_PROJECT)

# Cooja compila con un entorno minimo que NO incluye $HOME, asi que la ruta a
# Contiki-NG se resuelve primero de forma relativa al repo (symlink contiki-ng
# en la raiz, ver README). Los otros candidatos son para compilar a mano.
CONTIKI ?= $(firstword $(wildcard $(CURDIR)/../../contiki-ng $(CURDIR)/../../../contiki-ng $(HOME)/contiki-ng $(HOME)/Downloads/contiki-ng))

MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_IPV6

# El eje de la actividad. Los dos escenarios se compilan desde aca:
#
#   make                                        -> RPL Lite (default)
#   make MAKE_ROUTING=MAKE_ROUTING_RPL_CLASSIC  -> RPL Classic
#
# OJO: make no detecta el cambio de routing en los objetos ya compilados.
# Correr `make distclean` al cambiar de escenario.
MAKE_ROUTING ?= MAKE_ROUTING_RPL_LITE

# Shell de Contiki-NG: es lo que da el comando `ping` para medir ICMP y
# `routes` para ver la tabla. Arranca solo, no hay que llamarlo desde la app.
# Queda en los dos firmwares; en la raiz es el que se usa.
include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell

include $(CONTIKI)/Makefile.include
