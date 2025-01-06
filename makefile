all:
	gcc sp-plus.c wav.c alsa.c sample.c ring_buffer.c -o sp-plus.o -Wextra -g -lasound
