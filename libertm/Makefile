.PHONY: all objects clean libs so-lib static-lib

TARGET = libertm
CFLAGS += -fPIC -Wall

all: libs

libs: static-lib so-lib
static-lib: $(TARGET).a
so-lib: $(TARGET).so

$(TARGET).so: $(TARGET).o
	$(LD) -shared -o $@ $^
$(TARGET).a: $(TARGET).o
	$(AR) r $@ $^
$(TARGET).o: $(TARGET).h

clean:
	rm -f $(TARGET).o $(TARGET).a $(TARGET).so
