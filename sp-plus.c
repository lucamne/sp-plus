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
	print_wav(wav);

	return 0;
}
