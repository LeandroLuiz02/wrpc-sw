.PHONY: all objects clean libs so-lib static-lib

TARGET = libertm
CFLAGS += -fPIC -Wall -g

ERTM_UART_LOCATION = ../lib
ERTM_BOARD_LOCATION = ../boards/ertm14/
WRC_INCLUDES = ../include

CFLAGS += -I $(ERTM_UART_LOCATION) -I $(ERTM_BOARD_LOCATION)
CFLAGS += -I $(WRC_INCLUDES)

all: libs serial ertm-serial main

libs: static-lib so-lib
static-lib: $(TARGET).a
so-lib: $(TARGET).so

$(TARGET).a $(TARGET).so: $(TARGET).o linux-uart-link.o common-uart-link.o
$(TARGET).so:
	$(LD) -shared -o $@ $^
$(TARGET).a:
	$(AR) r $@ $^
$(TARGET).o: $(TARGET).h

main: main.o libertm.a display.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS) -lm

serial: serial.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS)
serial.o: $(TARGET).h

ertm-serial: ertm-serial.o libertm.a display.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS) -lm

common-uart-link.o: $(ERTM_BOARD_LOCATION)/common-uart-link.c
	$(CC) $(CFLAGS) -o $@ -c $^

ertm14-uart-link.o: $(ERTM_UART_LOCATION)/ertm14-uart-link.c
	$(CC) $(CFLAGS) -c -o $@ $^ $(LDLIBS) $(LDFLAGS)

linux-uart-link.o: $(ERTM_BOARD_LOCATION)/ertm14-uart-link.h
ertm-serial.o: $(WRC_INCLUDES)/hw/wrc_diags_regs.h
ertm-serial.o: $(ERTM_BOARD_LOCATION)/psnmp-proto.h $(ERTM_BOARD_LOCATION)/board-state.h
ertm-serial.o: $(ERTM_BOARD_LOCATION)/ertm14-uart-link.h $(ERTM_BOARD_LOCATION)/ertm15_rf_distr.h
ertm-serial.o: private.h libertm.h
libertm.o: libertm.h private.h

display.so: display.o
	$(LD) -shared -o $@ $^

clean:
	rm -f *.o *.a *.so
	rm -f serial ertm-serial main
