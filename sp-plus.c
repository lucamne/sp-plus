#include "sampler.h"
#include "io.h"

#include <stdio.h>

#define TEST_WAV "test/StarWars.wav"

int main(int argc, char** argv)
{
	struct sampler* s = init_sampler(); 
	if (!s) {
		fprintf(stderr, "Error initializing sampler\n");
		return 0;
	}

	struct wav_file* wav = load_wav(TEST_WAV);
	/*
	for (int i = 0; i < wav->num_samples; i ++) {
		if (wav->data[i] != 0)
			printf("sample: %d, val: %d\n", i, wav->data[i]);
	}
	*/
	return 0;
}
