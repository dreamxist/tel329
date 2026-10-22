CONTIKI_PROJECT = udp-server udp-client
all: $(CONTIKI_PROJECT)

# Cooja compila con un entorno minimo que NO incluye $HOME, asi que la ruta a
# Contiki-NG se resuelve primero de forma relativa al repo (symlink contiki-ng
# en la raiz, ver README). Los otros candidatos son para compilar a mano.
# Se busca Makefile.include y no el directorio: un symlink colgado SI matchea
# en $(wildcard) y dejaba CONTIKI apuntando a la nada. Pasaba al compilar
# dentro del contenedor, donde el symlink del repo apunta a una ruta del host.
CONTIKI_CANDIDATOS = $(CURDIR)/../../contiki-ng $(CURDIR)/../../../contiki-ng $(HOME)/contiki-ng $(HOME)/Downloads/contiki-ng
CONTIKI ?= $(patsubst %/Makefile.include,%,$(firstword $(wildcard $(foreach d,$(CONTIKI_CANDIDATOS),$(d)/Makefile.include))))

MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE

include $(CONTIKI)/Makefile.include
