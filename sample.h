#ifndef SAMPLE_H
#define SAMPLE_H

#include <stdbool.h>

struct sample {
	char* data;
	int data_size;		// size in bytes
	int frame_size;		// size in bytes
	char* next_frame;
	int id;
	int rate;		// sample_rate in Hz
	// playback options
	bool playing;		// is sample currently playing
	bool loop;		// is sample in loop mode
};

struct sample* init_sample(void);

int load_wav_into_sample(const char* path, struct sample* s);

const char* get_next_frame(struct sample* s);
// reset next_frame pointer to start
int reset_sample(struct sample* s);

#endif
