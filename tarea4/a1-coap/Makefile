CONTIKI_PROJECT = root-coap cliente-coap
all: $(CONTIKI_PROJECT)

# Cooja compila con un entorno minimo que NO incluye $HOME, asi que la ruta a
# Contiki-NG se resuelve primero de forma relativa al repo (symlink contiki-ng
# en la raiz, ver README de la raiz). Los otros candidatos son para compilar a
# mano. Se busca Makefile.include y no el directorio: un symlink colgado SI
# matchea en $(wildcard) y dejaba CONTIKI apuntando a la nada (pasa dentro del
# contenedor, donde el symlink del repo apunta a una ruta del host).
CONTIKI_CANDIDATOS = $(CURDIR)/../../contiki-ng $(CURDIR)/../../../contiki-ng $(HOME)/contiki-ng
CONTIKI ?= $(patsubst %/Makefile.include,%,$(firstword $(wildcard $(foreach d,$(CONTIKI_CANDIDATOS),$(d)/Makefile.include))))

# Misma pila que la Tarea 3: CSMA + IPv6/6LoWPAN + RPL Lite. Encima, CoAP.
MAKE_MAC = MAKE_MAC_CSMA
MAKE_NET = MAKE_NET_IPV6
MAKE_ROUTING = MAKE_ROUTING_RPL_LITE

# El recurso sensores/temp vive en resources/, igual que en el ejemplo
# coap-example-server: el build compila solo todo lo que hay ahi.
MODULES_REL += ./resources

include $(CONTIKI)/Makefile.dir-variables
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/coap

include $(CONTIKI)/Makefile.include
