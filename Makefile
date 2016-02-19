.PHONY: all clean
include custom.mk
RIUC4:=riuc4
RIUC4_SRCS:=$(RIUC4).c

#RIUC:=riuc
RIUC_SRCS:=$(RIUC).c

APP_DIR:=.

C_DIR:=../common
C_SRCS:=ansi-utils.c my-pjlib-utils.c lvcode.c my-openssl.c

USERVER_DIR:=../userver

PROTOCOL_DIR:=../group-man/protocols
GM_P:=gm-proto.u
GMC_P:=gmc-proto.u
ADV_P:=adv-proto.u
GB_P:=gb-proto.u

NODE_DIR:=../group-man
NODE_SRCS:=node.c gb-sender.c

EP_DIR:=../media-endpoint
EP_SRCS:=endpoint.c

GEN_DIR:=gen
GEN_SRCS:=gm-client.c gmc-server.c adv-server.c gb-client.c

SERIAL_DIR := ../serial
SERIAL_SRCS := riuc4_uart.c serial_utils.c

HT_DIR:=../hash-table
HT_SRCS:=hash-table.c

CFLAGS:=-g -DPJ_AUTOCONF=1 -O2 -DPJ_IS_BIG_ENDIAN=0 -DPJ_IS_LITTLE_ENDIAN=1 -fms-extensions
CFLAGS+=-I$(LIBS_DIR)/include
CFLAGS+=-I$(C_DIR)/include
CFLAGS+=-I$(LIBS_DIR)/include/json-c
CFLAGS+=-I$(PROTOCOLS_DIR)/include
CFLAGS+=-I$(GEN_DIR)
CFLAGS+=-I$(NODE_DIR)/include
CFLAGS+=-I$(SERIAL_DIR)/include
CFLAGS+=-I$(EP_DIR)/include
CFLAGS+=-I$(HT_DIR)/include
CFLAGS+=-I$(APP_DIR)/include
CFLAGS+=-D__ICS_INTEL__

LIBS+=-lcrypto

all: gen-gm gen-gmc gen-adv gen-gb $(RIUC) $(RIUC4)

gen-gm: $(PROTOCOL_DIR)/$(GM_P)
	mkdir -p gen
	awk -v base_dir=$(USERVER_DIR) -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

gen-gmc: $(PROTOCOL_DIR)/$(GMC_P)
	mkdir -p gen
	awk -v base_dir=$(USERVER_DIR) -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

gen-adv: $(PROTOCOL_DIR)/$(ADV_P)
	mkdir -p gen
	awk -v base_dir=$(USERVER_DIR) -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

gen-gb: $(PROTOCOL_DIR)/$(GB_P)
	mkdir -p gen
	awk -v base_dir=$(USERVER_DIR) -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

$(RIUC): $(NODE_SRCS:.c=.o) $(GEN_SRCS:.c=.o) $(C_SRCS:.c=.o) $(RIUC_SRCS:.c=.o) $(SERIAL_SRCS:.c=.o) $(EP_SRCS:.c=.o) $(HT_SRCS:.c=.o)
	$(CROSS_TOOL) -o $@ $^ $(LIBS)

$(RIUC4): $(NODE_SRCS:.c=.o) $(GEN_SRCS:.c=.o) $(C_SRCS:.c=.o) $(RIUC4_SRCS:.c=.o) $(SERIAL_SRCS:.c=.o) $(EP_SRCS:.c=.o) $(HT_SRCS:.c=.o)
	$(CROSS_TOOL) -o $@ $^ $(LIBS)

$(NODE_SRCS:.c=.o): %.o: $(NODE_DIR)/src/%.c
	$(CROSS_TOOL) -c -o $@ $^ $(CFLAGS)
$(GEN_SRCS:.c=.o): %.o: $(GEN_DIR)/%.c
	$(CROSS_TOOL) -c -o $@ $^ $(CFLAGS)
$(C_SRCS:.c=.o): %.o: $(C_DIR)/src/%.c
	$(CROSS_TOOL) -c -o $@ $^ $(CFLAGS)
$(ANSI_O_SRCS:.c=.o): %.o: $(ANSI_O_DIR)/src/%.c
	$(CROSS_TOOL) -c -o $@ $^ $(CFLAGS)
$(RIUC_SRCS:.c=.o): %.o: src/%.c
	$(CROSS_TOOL) -c -o $@ $^ $(CFLAGS)
$(RIUC4_SRCS:.c=.o): %.o: src/%.c
	$(CROSS_TOOL) -c -o $@ $^ $(CFLAGS)
$(SERIAL_SRCS:.c=.o) : %.o : $(SERIAL_DIR)/src/%.c
	$(CROSS_TOOL) -o $@ -c $< $(CFLAGS)
$(EP_SRCS:.c=.o) : %.o : $(EP_DIR)/src/%.c
	$(CROSS_TOOL) -o $@ -c $< $(CFLAGS)
$(HT_SRCS:.c=.o) : %.o : $(HT_DIR)/src/%.c
	$(CROSS_TOOL) -o $@ -c $< $(CFLAGS)
clean:
	rm -fr *.o gen gen-gm gen-gmc gen-adv gen-gb $(RIUC) $(RIUC4)
