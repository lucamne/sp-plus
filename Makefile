TARGET = sp-plus
SRC := sp-plus.c wav.c alsa.c sample.c bus.c
SRC := $(patsubst %,src/%,$(SRC))

CFLAGS := -g
# CFLAGS = -03

LINKFLAGS = -lm -lasound -L./lib -lsmarc

all: $(TARGET)

$(TARGET) : $(SRC)
	gcc $(CFLAGS) -o $@ $^ $(LINKFLAGS)
