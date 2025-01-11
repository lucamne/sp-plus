TARGET = ../sp-plus
SRC := sp-plus.c wav.c alsa.c sample.c bus.c

CFLAGS := -g
# CFLAGS = -03

CFLAGS := -I./external


LINKFLAGS = -lm -lasound -L../lib -lsmarc -lraylib

all: $(TARGET)

$(TARGET) : $(SRC)
	gcc $(CFLAGS) -o $@ $^ $(LINKFLAGS)

clean:
	rm ../sp-plus
