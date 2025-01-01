#define DEFAULT_SAMPLE_SIZE 2646000 // 60 seconds at 44100 hz

struct sample {
	float* left; 	// left channel data
	float* right;		// right channel data
	int size;		// size of audio data arrays
};

struct sampler {
	struct sample* bank;	// array of all samples
	int bank_size;
};

int init_sampler(struct sampler* s);

int add_sample(struct sampler* s);
