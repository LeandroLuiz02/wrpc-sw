.PHONY: all objects clean libs so-lib static-lib

TARGET = libertm
CFLAGS += -fPIC -Wall -g
CFLAGS += -DCONFIG_TARGET_ERTM14

ERTM_UART_LOCATION = ../lib
ERTM_BOARD_LOCATION = ../boards/ertm14/
WRC_INCLUDES = ../include

CFLAGS += -I $(ERTM_UART_LOCATION) -I $(ERTM_BOARD_LOCATION)
CFLAGS += -I $(WRC_INCLUDES)

GIT_VER ?= $(shell git describe --always --dirty | sed  's;^wr-switch-sw-;;')
GIT_USR ?= $(shell git config --get-all user.name)
GIT_URL ?= $(shell git remote show -n gitlab| sed -n 's; *Fetch URL: *\(.*\);\1;p')

LIBS = $(TARGET).a $(TARGET).so libertm_display.so

all: libs main
libs: $(LIBS)

$(TARGET).a $(TARGET).so: $(TARGET).o linux-uart-link.o common-uart-link.o
$(TARGET).so:
	echo $(GIT_VER) $(GIT_USR) $(GIT_URL)
	$(LD) -shared -o $@ $^
$(TARGET).a:
	$(AR) r $@ $^
$(TARGET).o: $(TARGET).h

main: main.o libertm.a display.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS) -lm

common-uart-link.o: $(ERTM_BOARD_LOCATION)/common-uart-link.c
	$(CC) $(CFLAGS) -o $@ -c $^
ertm14-uart-link.o: $(ERTM_UART_LOCATION)/ertm14-uart-link.c
	$(CC) $(CFLAGS) -c -o $@ $^ $(LDLIBS) $(LDFLAGS)
linux-uart-link.o: $(ERTM_BOARD_LOCATION)/common-uart-link.h
libertm.o: libertm.h private.h

libertm_display.so: display.o
	$(LD) -shared -o $@ $^

clean:
	rm -f *.o *.a *.so
	rm -f serial ertm-serial main

include deploy.mk
