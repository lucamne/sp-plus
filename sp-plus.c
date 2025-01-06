#include "io.h"
#include "sample.h"
#include "ring_buffer.h"

#include <stdio.h>

#define TEST_WAV "test/StarWars.wav"

int main(int argc, char** argv)
{
	struct sample* samp = init_sample();
	load_wav_into_sample(TEST_WAV, samp);

	struct ring_buffer* rb = init_ring_buf(2000); 
	if (!rb)
		printf("Error initializing ring buffer");

	char* tmp = calloc(1, samp->data_size);
	const int transfer_size = samp->frame_size;
	while (1) {
		

	play_clip(samp->data, samp->data_size);

	return 0;
}
