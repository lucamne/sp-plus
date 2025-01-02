#include "sampler.h"
#include <stdio.h>

#define TEST_WAV "test/StarWars.wav"

int main(int argc, char** argv)
{
	struct sampler* s = init_sampler(); 
	if (!s) {
		fprintf(stderr, "Error initializing sampler\n");
		return 0;
	}

	if (add_sample(s)) {
		fprintf(stderr, "Error adding sample\n");
		return 0;
	}

	FILE* f = fopen(TEST_WAV, "r");
	if (!f) {
		printf("Error loading file: %s\n", TEST_WAV);
		return 0;
	}


	if (load_wav(f, s->bank[0])) {
		printf("Error loading file: %s\n", TEST_WAV);
		return 0;
	}
	fclose(f);

	int counter = 0;
	for (int i = 0; i < s->bank[0]->size; i++) {
		printf("%d\n", s->bank[0]->data[i]);
		if (s->bank[0]->data[i] == 0)
			counter++;
	}
	printf("%d\n", counter);

	return 0;
}
