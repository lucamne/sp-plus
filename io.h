#ifndef IO_H
#define IO_H

#include "bus.h"

#include <stdint.h>
#include <alsa/asoundlib.h>
#include <signal.h>

/* WAV: wav.c */
struct wav_file {
	// fmt chunk
	const char* path;
	int32_t fmt_ck_size;		// fmt chunk size in bytes
	int16_t format;			// audio format tag
	int num_channels;
	int32_t sample_rate;
	int32_t bytes_per_sec;
	int frame_size;		// num_channels*bits_per_sample/8
	int bit_depth;
	int cb_size;		// for extended formats
	int valid_bits_per_sample;
	int32_t channel_mask;
	int32_t sub_format[4];

	// data chunk
	int32_t data_size;		// data size in bytes
	int16_t* data;			// raw data (little endian)
	int32_t num_samples;		// data_ck_size / block_size
};
// returns NULL on failure
struct wav_file* load_wav(const char* path);

void print_wav(const struct wav_file* w);

/* ALSA: alsa.c */
struct alsa_dev {
	snd_pcm_t* pcm;			// pcm handle
	char* dev_id;			// name of pcm device (plughw:i,j)
	int rate;			// sample rate in Hz
	int num_channels;		// number of channels
	snd_pcm_uframes_t buffer_size;	// buffer size in frames
	snd_pcm_uframes_t period_size;	// period size in frames
	
};

// returns an alsa pcm device for playback, returns NULL on failure
struct alsa_dev* open_alsa_dev(int r, int num_c);
// starts an alsa playback device which pulls from ring buffer, returns 1 on failure
int start_alsa_dev(struct alsa_dev* a_dev, struct bus* master);
// assumes 22050hz, 16bit, 2 channel
void play_clip (void* clip, int clip_size);


#endif
