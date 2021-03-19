.PHONY: all objects clean libs so-lib static-lib

TARGET = libertm
CFLAGS += -fPIC -Wall -g
CFLAGS += -DCONFIG_TARGET_ERTM14

ERTM_UART_LOCATION = ../lib
ERTM_BOARD_LOCATION = ../boards/ertm14/
WRC_INCLUDES = ../include

CFLAGS += -I $(ERTM_UART_LOCATION) -I $(ERTM_BOARD_LOCATION)
CFLAGS += -I $(WRC_INCLUDES)

all: libs main

libs: $(TARGET).a $(TARGET).so display.so

$(TARGET).a $(TARGET).so: $(TARGET).o linux-uart-link.o common-uart-link.o
$(TARGET).so:
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

display.so: display.o
	$(LD) -shared -o $@ $^

clean:
	rm -f *.o *.a *.so
	rm -f serial ertm-serial main
