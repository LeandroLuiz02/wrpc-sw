.PHONY: all objects clean libs so-lib static-lib

TARGET = libertm
CFLAGS += -fPIC -Wall -g

all: libs serial

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


clean:
	rm -f *.o *.a *.so
	rm -f serial
