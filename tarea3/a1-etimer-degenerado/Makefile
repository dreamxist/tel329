CONTIKI_PROJECT = udp-server udp-client
all: $(CONTIKI_PROJECT)

# Cooja compila con un entorno minimo que NO incluye $HOME, asi que la ruta a
# Contiki-NG se resuelve primero de forma relativa al repo (symlink contiki-ng
# en la raiz, ver README). Los otros candidatos son para compilar a mano.
CONTIKI ?= $(firstword $(wildcard $(CURDIR)/../../contiki-ng $(CURDIR)/../../../contiki-ng $(HOME)/contiki-ng $(HOME)/Downloads/contiki-ng))

MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE

# Periodo del etimer del cliente. CASO_ID se pasa como numero, no como string,
# para no depender del escapado del comando de compilacion de Cooja.
#
#   make CASO=base    10 * CLOCK_SECOND
#   make CASO=1e-5    1e-5 * CLOCK_SECOND
#   make CASO=1e-10   1e-10 * CLOCK_SECOND
#   make CASO=1e-20   1e-20 * CLOCK_SECOND
#
# OJO: make no detecta un cambio de CFLAGS. Al cambiar de caso hay que correr
# `make distclean` antes, o el binario anterior se queda pegado.
CASO ?= base

ifeq ($(CASO),base)
CFLAGS += -DCASO_ID=0
else ifeq ($(CASO),1e-5)
CFLAGS += -DCASO_ID=5
else ifeq ($(CASO),1e-10)
CFLAGS += -DCASO_ID=10
else ifeq ($(CASO),1e-20)
CFLAGS += -DCASO_ID=20
else
$(error CASO invalido '$(CASO)': usar base, 1e-5, 1e-10 o 1e-20)
endif

include $(CONTIKI)/Makefile.include
