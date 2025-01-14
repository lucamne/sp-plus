#ifndef IO_H
#define IO_H

#include <stdint.h>
#include <signal.h>

/*
 * Handles file wav file loading
 * Luca Negris - 01/2025
 */

// container for wav file spec info
struct wav_file {
	const char* path;
	int32_t fmt_ck_size;		// fmt chunk size in bytes
	int16_t format;			// audio format tag
	int num_channels;
	int32_t sample_rate;		// sample_rate (Hz)
	int32_t bytes_per_sec;
	int frame_size;			// num_channels*bits_per_sample/8
	int bit_depth;
	int cb_size;			// for extended formats
	int valid_bits_per_sample;
	int32_t channel_mask;
	int32_t sub_format[4];

	// data chunk
	int32_t data_size;		// data size in bytes
	int16_t* data;			// raw data (little endian)
	int32_t num_samples;		// data_ck_size / block_size
};


/**** Wav: wav.c ****/

// reads wav file into wav from path
int load_wav(struct wav_file* wav, const char* path);

#endif
