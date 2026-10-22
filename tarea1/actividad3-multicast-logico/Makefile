CONTIKI_PROJECT = multicast-logico
all: $(CONTIKI_PROJECT)

# Cooja compila con un entorno minimo que NO incluye $HOME, asi que la ruta a
# Contiki-NG se resuelve primero de forma relativa al repo (symlink contiki-ng
# en la raiz, ver README). Los otros dos candidatos son para compilar a mano.
# Se busca Makefile.include y no el directorio: un symlink colgado SI matchea
# en $(wildcard) y dejaba CONTIKI apuntando a la nada. Pasaba al compilar
# dentro del contenedor, donde el symlink del repo apunta a una ruta del host.
CONTIKI_CANDIDATOS = $(CURDIR)/../../contiki-ng $(CURDIR)/../contiki-ng $(HOME)/contiki-ng $(HOME)/Downloads/contiki-ng
CONTIKI ?= $(patsubst %/Makefile.include,%,$(firstword $(wildcard $(foreach d,$(CONTIKI_CANDIDATOS),$(d)/Makefile.include))))

MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_NULLNET

include $(CONTIKI)/Makefile.include
