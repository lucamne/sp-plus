#include "io.h"
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

	struct ring_buf* rb = init_ring_buf(samp->rate * samp->frame_size / 2); 
	if (!rb) {
		printf("Error initializing ring buffer");
		return 0;
	}

	play_clip(samp->data, samp->data_size);

	return 0;
}
