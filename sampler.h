#include <stdio.h>
#include <stdint.h>

#define DEFAULT_SAMPLE_SIZE 1323000 // 30 seconds at 44100 hz

struct sample {
	int16_t* data; 		// interleaved audio data
	int size;		// num of samples per channel
	int num_channels;
};

struct sampler {
	struct sample** bank;	// array of all samples
	int bank_size;
};

struct sampler* init_sampler(void);

int add_sample(struct sampler* s);

/* If new size is smaller than current, sample is truncated
 * If new size is larger, newly allocated memory is initialized to 0 */
int resize_sample(struct sample* samp, int s);

int load_wav(FILE* f, struct sample* s);
