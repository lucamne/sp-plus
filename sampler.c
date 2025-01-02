#include "sampler.h"

#include <limits.h>
#include <stdlib.h>
#include <assert.h>

int resize_sample(struct sample* samp, int s)
{
	samp->data = realloc(samp->data, sizeof(int16_t) * s * samp->num_channels);
	if(!samp->data) {
		fprintf(stderr, "Failed to resize sample\n");
		return 1;
	}
	
	// initiliaze new memory to 0
	if (s > samp->size) {
		for (int i = samp->size * samp->num_channels; i < s * samp->num_channels; i++) {
			samp->data[i] = 0;
		}
	}
	samp->size = s;

	return 0;
}

struct sampler* init_sampler(void)
{
	struct sampler* s = malloc(sizeof(struct sampler));
	assert(s);

	s->bank = NULL;
	s->bank_size = 0;

	return s;
}

int add_sample(struct sampler* s)
{
	s->bank = realloc(s->bank,sizeof(struct sample*) * (s->bank_size + 1));
	if (!s->bank)
		return 1;

	struct sample* const new_samp = malloc(sizeof(struct sample));
	if (!new_samp)
		return 1;

	new_samp->num_channels = 1;
	new_samp->size = 0;
	new_samp->data = NULL;

	s->bank[s->bank_size++] = new_samp; 

	return 0;
}

int load_wav(FILE* f, struct sample* s)
{
	uint32_t buffer = 0;

	fread(&buffer, 4, 1, f); 		// FileTypeBlocID
	if (0x46464952 ^ buffer) 		// "RIFF"
		return 1;

	fread(&buffer, 4, 1, f); 		// FileSize
	fread(&buffer, 4, 1, f); 		// FileFormatID
	if (0x45564157 ^ buffer) 		// "WAVE"
		return 1;

	fread(&buffer, 4, 1, f); 		// FormatBlocID
	if (0x20746D66 ^ buffer) 		// "fmt "
		return 1;

	fread(&buffer, 4, 1, f); 		// BlocSize;		
	fread(&buffer, 2, 1, f); 		// AudioFormat
	const int audio_format = buffer;
	fread(&buffer, 2, 1, f); 		// NbrChannels
	const int num_chan = buffer;
	fread(&buffer, 4, 1, f); 		// Frequency (Hz)
	fread(&buffer, 4, 1, f); 		// BytePerSec 
	fread(&buffer, 2, 1, f); 		// BytePerBloc 
	const int byte_per_bloc = buffer;
	fread(&buffer, 2, 1, f); 		// BitsPerSample 
	const int bit_depth = buffer;
	fread(&buffer, 4, 1, f); 		// DataBlocID 
	if (0x61746164 ^ buffer) 		// "data"
		return 1;

	fread(&buffer, 4, 1, f); 		// DataSize 
	if (buffer > INT_MAX) {
		fprintf(stderr, "Sample is too large\n");
		return 1;
	}
	const int data_size = buffer;

	if (bit_depth != 16)
		printf("Warning bit depth is: %d\n", bit_depth);

	s->num_channels = num_chan;
	if (data_size > s->size) {
		if (resize_sample(s, data_size))
			return 1;
	}

	fread(s->data, byte_per_bloc, data_size, f);
	// if format is float we need to convert to back to 16 bit int
	if (audio_format == 3) {
		printf("converting data from float to int...\n");
		for (int i = 0; i < data_size; i++) {
			float f = 0x0 | s->data[i];
			f *= 32768;
			if (f > 32767)
				f = 32767;
			else if (f < -32767)
				f = -32767;

			s->data[i] = (int16_t) f;
		}
	}
	
	return 0;
}
