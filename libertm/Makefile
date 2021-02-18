.PHONY: all objects clean libs so-lib static-lib

TARGET = libertm
CFLAGS += -fPIC -Wall -g

ERTM_UART_LOCATION = ../lib
ERTM_BOARD_LOCATION = ../boards/ertm14/

CFLAGS += -I $(ERTM_UART_LOCATION) -I $(ERTM_BOARD_LOCATION)

all: libs serial ertm-serial

libs: static-lib so-lib
static-lib: $(TARGET).a
so-lib: $(TARGET).so

$(TARGET).so: $(TARGET).o
	$(LD) -shared -o $@ $^
$(TARGET).a: $(TARGET).o
	$(AR) r $@ $^
$(TARGET).o: $(TARGET).h

serial: serial.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS)
serial.o: $(TARGET).h

ertm-serial: ertm-serial.o linux-uart-link.o common-uart-link.o
	$(CC) $(CFLAGS) -o $@ $^ $(LDLIBS) $(LDFLAGS) -lm

common-uart-link.o: $(ERTM_BOARD_LOCATION)/common-uart-link.c
	$(CC) $(CFLAGS) -o $@ -c $^

ertm14-uart-link.o: $(ERTM_UART_LOCATION)/ertm14-uart-link.c
	$(CC) $(CFLAGS) -c -o $@ $^ $(LDLIBS) $(LDFLAGS)

clean:
	rm -f *.o *.a *.so
	rm -f serial
