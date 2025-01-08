all:
	gcc sp-plus.c wav.c alsa.c sample.c bus.c -o sp-plus.o -Wextra -g -lasound -lm
