TARGET = ../sp-plus
SRC := linux_platform.c sp_plus.c

CFLAGS := -g 
# CFLAGS = -03

CFLAGS := $(CFLAGS) -I./external


LINKFLAGS = -lm -lasound -lX11 -L../lib -lsmarc 

all: $(TARGET)

$(TARGET) : $(SRC)
	gcc $(CFLAGS) -o $@ $^ $(LINKFLAGS)

clean:
	rm ../sp-plus
