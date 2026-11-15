CONTIKI_PROJECT = mqtt-cliente
all: $(CONTIKI_PROJECT)

# Cooja compila con un entorno minimo que NO incluye $HOME (ver README de la
# raiz): la ruta a Contiki-NG se resuelve primero relativa al repo (symlink
# contiki-ng en la raiz) y los otros candidatos quedan para la linea de
# comandos. Se busca Makefile.include y no el directorio, porque un symlink
# colgado SI matchea en $(wildcard). Dentro del contenedor correr.sh pasa
# CONTIKI explicito.
CONTIKI_CANDIDATOS = $(CURDIR)/../../contiki-ng $(CURDIR)/../../../contiki-ng $(HOME)/contiki-ng
CONTIKI ?= $(patsubst %/Makefile.include,%,$(firstword $(wildcard $(foreach d,$(CONTIKI_CANDIDATOS),$(d)/Makefile.include))))

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/mqtt

MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE

# cooja es el plan A (red RPL real detras del border router); native es el
# plan B documentado en el README (un proceso por cliente, sin RPL).
PLATFORMS_ONLY = cooja native

include $(CONTIKI)/Makefile.include
