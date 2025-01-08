#include "io.h"
#include "defs.h"
#include "sample.h"
#include "bus.h"

#include <time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_WAV "test/StarWars.wav"

int main(int argc, char** argv)
{
	struct sample* samp = init_sample();
	load_wav_into_sample(TEST_WAV, samp);

	struct bus* master = init_bus();
	master->sample_in = samp;

	struct alsa_dev* a_dev = 
		open_alsa_dev(22050, 2);
	if (!a_dev) {
		printf("Error opening audio device\n");
		return 0;
	}

	int err = start_alsa_dev(a_dev, rb);
	if (err) {
		printf("Error starting audio\n");
		return 0;
	}
	int bytes_seen = 0;
	while (1) {
		if (bytes_seen + 8820 >= samp->data_size)
			break;

		if (44100 - rb->in_use >= 8820) {
			const int n = write_to_buf(rb, samp->data + bytes_seen, 4410);
			bytes_seen += n;
		} 
	}
	return 0;
}
