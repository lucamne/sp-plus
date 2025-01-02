#include <stdio.h>
#include <stdint.h>

#define DEFAULT_SAMPLE_SIZE 1323000 	// 30 seconds at 44100 hz

struct sample {
	int16_t* data; 			// interleaved audio data
	int num_samples;		// num of samples per channel
	int num_channels;
};

struct sampler {
	struct sample* bank;		// array of all samples
	int bank_size;
};

struct sampler* init_sampler(void);
