TARGET = ../build/sp-plus
SRC := linux_platform.c sp_plus.c

CFLAGS := -g -Wextra
# CFLAGS = -03

CFLAGS := $(CFLAGS) -I./external


LINKFLAGS = -lm -lasound -lX11 -L../lib -lsmarc 

all: $(TARGET)

$(TARGET) : $(SRC)
	mkdir -p ../build
	gcc $(CFLAGS) -o $@ $^ $(LINKFLAGS)

clean:
	rm ../build/sp-plus
