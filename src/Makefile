TARGET = ../sp-plus
SRC := sp-plus.c file.c alsa.c sample.c bus.c gui.c update.c

CFLAGS := -g 
# CFLAGS = -03

CFLAGS := $(CFLAGS) -I./external


LINKFLAGS = -lm -lasound -L../lib -lsmarc -lraylib

all: $(TARGET)

$(TARGET) : $(SRC)
	gcc $(CFLAGS) -o $@ $^ $(LINKFLAGS)

clean:
	rm ../sp-plus
