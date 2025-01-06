#ifndef IO_H
#define IO_H

#include "ring_buffer.h"

#include <stdint.h>
#include <alsa/asoundlib.h>
#include <signal.h>

// WAV: wav.c
struct wav_file {
	// fmt chunk
	int32_t fmt_ck_size;		// fmt chunk size in bytes
	int16_t format;			// audio format tag
	int16_t num_channels;
	int32_t sample_rate;
	int32_t bytes_per_sec;
	int16_t frame_size;		// num_channels*bits_per_sample/8
	int16_t bit_depth;
	int16_t cb_size;		// for extended formats
	int16_t valid_bits_per_sample;
	int32_t channel_mask;
	int32_t sub_format[4];

	// data chunk
	int32_t data_size;		// data size in bytes
	int64_t* data;			// raw data (little endian)
	int num_samples;		// data_ck_size / block_size
};

// returns NULL on failure
struct wav_file* load_wav(const char* path);

void print_wav(const struct wav_file* w);

// ALSA: alsa.c
struct alsa_dev {
	snd_pcm_t* pcm;			// pcm handle
	snd_pcm_hw_params_t* params;	// params handle
	char* dev_id;			// name of pcm device (plughw:i,j)
	int rate;			// sample rate in Hz
	int num_channels;		// number of channels
	int buffer_frames;		// number of frames in buffer
	int frame_size;			// frame size in bytes
};

// returns an alsa pcm device for playback, returns NULL on failure
struct alsa_dev* open_alsa_dev(int r, int num_c, int buf_frames, int f_size);
// starts an alsa playback device which pulls from ring buffer, returns 1 on failure
int start_alsa_dev(struct alsa_dev* a_dev, struct ring_buf* buf);
// assumes 22050hz, 16bit, 2 channel
void play_clip (void* clip, int clip_size);



#endif
