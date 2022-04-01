CONTIKI_PROJECT = QL_TSCH
all: $(CONTIKI_PROJECT)

PLATFORMS_ONLY = cooja

CONTIKI=../..

MAKE_MAC = MAKE_MAC_TSCH

# MODULES += os/net/mac/tsch/sixtop

# include $(CONTIKI)/Makefile.dir-variables
# MODULES += $(CONTIKI_NG_SERVICES_DIR)/shell

include $(CONTIKI)/Makefile.include