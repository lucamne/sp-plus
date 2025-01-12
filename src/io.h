#ifndef IO_H
#define IO_H

#include "playback.h"

#include <stdint.h>
#include <alsa/asoundlib.h>
#include <signal.h>

/*
 * Handles file loading and interface with ALSA api
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

// handle to alsa device connection
struct alsa_dev {
	snd_pcm_t* pcm;			// pcm handle
	char* dev_id;			// name of pcm device (plughw:i,j)
	int rate;			// sample rate in Hz
	int num_channels;		// number of channels
	snd_pcm_uframes_t buffer_size;	// buffer size in frames
	snd_pcm_uframes_t period_size;	// period size in frames
	
};

/**** Wav: wav.c ****/

// reads wav file into wav from path
int load_wav(struct wav_file* wav, const char* path);

/**** ALSA: alsa.c ****/

// opens and prepares alsa device handle for playback
int open_alsa_dev(struct alsa_dev* a_dev, int rate, int num_c);

// starts signal handler and callback which calls process_bus on master
int start_alsa_dev(struct alsa_dev* a_dev, struct bus* master);

#endif
