#ifndef SAMPLE_H
#define SAMPLE_H

#include <stdbool.h>
#include <stdint.h>

struct sample {
	float* data;		// 16_bit float data
	int frame_size;		// size in bytes
	float* next_frame;
	int32_t num_frames;
	int id;
	int rate;		// sample_rate in Hz
	// playback options
	bool playing;		// is sample currently playing
	bool loop;		// is sample in loop mode
};

struct sample* init_sample(void);

int load_wav_into_sample(const char* path, struct sample* s);

#endif
