all:
	gcc sp-plus.c wav.c alsa.c sample.c bus.c smarc/smarc.c smarc/multi_stage.c smarc/polyfilt.c smarc/filtering.c smarc/remez_lp.c smarc/stage_impl.c -o sp-plus.o -Wextra -g -lasound -lm
