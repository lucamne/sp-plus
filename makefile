all:
	gcc sp-plus.c sampler.c io.c alsa-play.c -o sp-plus.o -Wextra -g -lasound
