CONTIKI_PROJECT = unicast-etimer
all: $(CONTIKI_PROJECT)

# Cooja compila con un entorno minimo que NO incluye $HOME, asi que la ruta a
# Contiki-NG se resuelve primero de forma relativa al repo (symlink contiki-ng
# en la raiz, ver README). Los otros dos candidatos son para compilar a mano.
CONTIKI ?= $(firstword $(wildcard $(CURDIR)/../../contiki-ng $(CURDIR)/../contiki-ng $(HOME)/contiki-ng $(HOME)/Downloads/contiki-ng))

MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include
