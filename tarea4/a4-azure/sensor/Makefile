CONTIKI_PROJECT = sensor-azure
all: $(CONTIKI_PROJECT)

CONTIKI ?= ../../..

include $(CONTIKI)/Makefile.dir-variables
# httpd-ws es el servidor HTTP que Contiki-NG trae en el árbol (os/net/app-layer),
# descendiente de httpd-simple/websense que usa Kurniawan (2018).
MODULES += $(CONTIKI_NG_APP_LAYER_DIR)/httpd-ws

# El sensor es un nodo RPL hoja; el border router (examples/rpl-border-router) arma el DAG.
MAKE_ROUTING ?= MAKE_ROUTING_RPL_LITE

PLATFORMS_EXCLUDE = sky z1

include $(CONTIKI)/Makefile.include
