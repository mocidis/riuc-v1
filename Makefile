.PHONY: all clean
APP:=riuc
APP_SRCS:=riuc.c

C_DIR:=../common
C_SRCS:=ansi-utils.c

USERVER_DIR:=../userver

PROTOCOL_DIR:=../group-man/protocols
GM_P:=gm-proto.u
GMC_P:=gmc-proto.u
ADV_P:=adv-proto.u
GB_P:=gb-proto.u

NODE_DIR:=../group-man
NODE_SRCS:=node.c gb-sender.c

GEN_DIR:=gen
GEN_SRCS:=gm-client.c gmc-server.c adv-server.c gb-client.c

JSONC_DIR:=../json-c/output

SERIAL_DIR := ../serial
SERIAL_SRCS := riuc4_uart.c serial_utils.c

CFLAGS:=$(shell pkg-config --cflags libpjproject) -fms-extensions
CFLAGS+=-I$(C_DIR)/include
CFLAGS+=-I../json-c/output/include/json-c
CFLAGS+=-I$(PROTOCOLS_DIR)/include
CFLAGS+=-I$(GEN_DIR)
CFLAGS+=-I$(NODE_DIR)/include
CFLAGS+=-I$(SERIAL_DIR)/include

LIBS:= $(shell pkg-config --libs libpjproject) $(JSONC_DIR)/lib/libjson-c.a -lpthread

all: gen-gm gen-gmc gen-adv gen-gb  $(APP)

gen-gm: $(PROTOCOL_DIR)/$(GM_P)
	mkdir -p gen
	awk -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

gen-gmc: $(PROTOCOL_DIR)/$(GMC_P)
	mkdir -p gen
	awk -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

gen-adv: $(PROTOCOL_DIR)/$(ADV_P)
	mkdir -p gen
	awk -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

gen-gb: $(PROTOCOL_DIR)/$(GB_P)
	mkdir -p gen
	awk -f $(USERVER_DIR)/gen-tools/gen.awk $<
	touch $@

$(APP): $(NODE_SRCS:.c=.o) $(GEN_SRCS:.c=.o) $(C_SRCS:.c=.o) $(APP_SRCS:.c=.o) $(SERIAL_SRCS:.c=.o)
	gcc -o $@ $^ $(LIBS)

$(NODE_SRCS:.c=.o): %.o: $(NODE_DIR)/src/%.c
	gcc -c -o $@ $^ $(CFLAGS)
$(GEN_SRCS:.c=.o): %.o: $(GEN_DIR)/%.c
	gcc -c -o $@ $^ $(CFLAGS)
$(C_SRCS:.c=.o): %.o: $(C_DIR)/src/%.c
	gcc -c -o $@ $^ $(CFLAGS)
$(ANSI_O_SRCS:.c=.o): %.o: $(ANSI_O_DIR)/src/%.c
	gcc -c -o $@ $^ $(CFLAGS)
$(APP_SRCS:.c=.o): %.o: src/%.c
	gcc -c -o $@ $^ $(CFLAGS)
$(SERIAL_SRCS:.c=.o) : %.o : $(SERIAL_DIR)/src/%.c
	gcc -o $@ -c $< $(CFLAGS)

clean:
	rm -fr *.o gen gen-gm gen-gmc gen-adv gen-gb $(APP)
