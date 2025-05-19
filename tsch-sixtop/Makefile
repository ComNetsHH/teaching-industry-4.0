CONTIKI_PROJECT = tsch-sixtop
all: $(CONTIKI_PROJECT)

CONTIKI = ../..
PROJECT_SOURCEFILES += sf-simple.c
MODULES += os/services/shell
MODULES += os/net/mac/tsch/sixtop
MAKE_MAC = MAKE_MAC_TSCH


MAKE_MAC = MAKE_MAC_TSCH

WERROR=0

include $(CONTIKI)/Makefile.dir-variables
#MODULES += $(CONTIKI_NG_MAC_DIR)/tsch/sixtop

include $(CONTIKI)/Makefile.include
