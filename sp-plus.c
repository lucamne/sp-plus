#include "io.h"
#include "defs.h"
#include "sample.h"
#include "ring_buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_WAV "test/StarWars.wav"

int main(int argc, char** argv)
{
	struct sample* samp = init_sample();
	load_wav_into_sample(TEST_WAV, samp);

	struct ring_buf* rb = init_ring_buf(22050); 
	if (!rb) {
		printf("Error initializing ring buffer");
		return 0;
	}
	struct alsa_dev* a_dev = 
		open_alsa_dev(22050, 1, 22050 / 2, 2);
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
	while (bytes_seen + 2 <= samp->data_size) {
		const int n = write_to_buf(rb, samp->data + bytes_seen, 2);
		bytes_seen += n;
	}
	

	// play_clip(samp->data, samp->data_size);

	return 0;
}
