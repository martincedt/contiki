CONTIKI_PROJECT = node
CONTIKI=../../../../..
CFLAGS += -DPROJECT_CONF_H=\"project-conf.h\"

TARGET ?= jn516x
JN516x_WITH_DONGLE = 1

CONTIKI_WITH_IPV6 = 1

PROJECTDIRS += .. ../tools
PROJECT_SOURCEFILES += rich.c #plexi.c
CFLAGS += -DWITH_COAP
CFLAGS += -DREST=coap_rest_implementation
CFLAGS += -DUIP_CONF_TCP=0
APPS = orchestra
APPS += json
APPS += er-coap
APPS += rest-engine

MODULES += core/net/mac/tsch

all: $(CONTIKI_PROJECT)

include $(CONTIKI)/Makefile.include
