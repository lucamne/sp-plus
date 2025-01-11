#include "io.h"

#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

static void print_wav(const struct wav_file* w)
{
	printf(	
			"***********************\n"
			"Wav Info:\n"
			"-----------------------\n"
			"path: %s\n"
			"channels: %d\n"
			"sample rate: %dHz\n"
			"bytes per sec: %d\n"
			"bitdepth: %d\n"
			"num samples: %d\n"
			"size: %.1fkB\n"
			"***********************\n",
			w->path,
			w->num_channels,
			w->sample_rate,
			w->bytes_per_sec,
			w->bit_depth,
			w->num_samples,
			((float) w->data_size) / 1000.0f);
}

// log read error and free state ptrs
static void wav_read_error(FILE* f, struct wav_file* w) 
{
	fprintf(stderr, "Error reading wav\n");
	free(w);
	fclose(f);
}

// check endianess of system at runtime
static bool is_little_endian(void)
{
	int n = 1;
	if(*(char *)&n == 1) 
		return true;
	return false;
}

// load a wav file from disk
struct wav_file* load_wav(const char* path)
{
	if (!is_little_endian) {
		fprintf(stderr, "Only little-endian systems are supported\n");
		return NULL;
	}

	FILE* f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Could not open file: %s\n", path);
		return NULL;
	}

	struct wav_file* wav = malloc(sizeof(struct wav_file));
	wav->path = path;
	int32_t buffer[12];

	// read RIFF, WAVE, and fmt headers assuming they exist
	if (fread(buffer, sizeof(*buffer), 5, f) != 5) {
		wav_read_error(f, wav);
		return NULL;
	}
	if (buffer[0] ^ 0x46464952) {		// check for 'RIFF' tag
		wav_read_error(f, wav);
		return NULL;
	}
	/* Spec does not require WAVE tag here
	 * For now I am checking to reduce scope */
	if (buffer[2] ^ 0x45564157) {		// check for 'WAVE' tag
		wav_read_error(f, wav);
		return NULL;
	}
	if (buffer[3] ^ 0x20746D66) {		// check for 'fmt ' tag
		wav_read_error(f, wav);
		return NULL;
	}
	wav->fmt_ck_size = buffer[4];
	// support  only non-extended PCM for now
	if (wav->fmt_ck_size != 16) {
		wav_read_error(f, wav);
		return NULL;
	}

	// read fmt chunk
	if (fread(buffer, 1, wav->fmt_ck_size, f) != (size_t) wav->fmt_ck_size) {
		wav_read_error(f, wav);
		return NULL;
	}
	wav->format = buffer[0] & 0xFFFF;		
	if (wav->format != 1) {			// support only PCM for now
		wav_read_error(f, wav);
		return NULL;
	}
	wav->num_channels = buffer[0] >> 16;	
	wav->sample_rate = buffer[1];
	wav->bytes_per_sec = buffer[2];
	wav->frame_size = buffer[3] & 0xFFFF;
	wav->bit_depth = buffer[3] >> 16;
	if (wav->bit_depth != 16)
		fprintf(stderr, "Warning bitdepth is %d\n", wav->bit_depth);

	// read data chunk assuming no other chunks come next (ie. fact)
	if (fread(buffer, sizeof(*buffer), 2, f) != 2) {
		wav_read_error(f, wav);
		return NULL;
	}
	if (buffer[0] ^ 0x61746164) { 		// check for 'data' tag
		wav_read_error(f, wav);
		return NULL;
	}
	wav->data_size = buffer[1];
	wav->num_samples = wav->data_size / wav->frame_size;
	wav->data = malloc(wav->data_size);
	if (!wav->data) {
		wav_read_error(f, wav);
		return NULL;
	}
	if (fread(wav->data, 1, wav->data_size, f) != (size_t) wav->data_size) {
		wav_read_error(f, wav);
		return NULL;
	}
	fclose(f);

	print_wav(wav);

	return wav;
}

void free_wav(struct wav_file* w)
{
	if (w) {
		if (w->data) free(w->data);
		free(w);
	}
}
