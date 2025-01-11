#ifndef SIGNAL_CHAIN_H
#define SIGNAL_CHAIN_H

#include <stdbool.h>
#include <stdint.h>

/*
 * For representing and processing audio within the application
 * Luca Negris - 01/2025
 */

// container for audio data
// the source of all playback is a sample
struct sample {
	double* data;		// 16_bit float data
	int frame_size;		// size in bytes
	double* next_frame;	// next frame to be played during playback
	int32_t num_frames;
	int rate;		// sample_rate in Hz
	// playback options
	bool playing;		// is sample currently playing
	bool loop;		// is sample in loop mode
};

// Used to route and mix audio data
// Can have many bus inputs but only one sample input
// In practice busses will only have one output
struct bus {
	struct sample* sample_in;	// sample input if any
	struct bus** bus_ins;		// bus inputs
	int num_bus_ins;

	float atten;			// attenuation, 0.0 is none 1.0 is max
	float pan;			// -1.0 = L, 1.0 = R
	
	bool active;			// should date be grabbed from bus
	bool solo;			// is this bus soloed
};

//------------------------------------------------------------------------------
// Sample Contol Functions: sample.c
// -----------------------------------------------------------------------------

// returns empty sample or NULL on error
struct sample* init_sample(void);
void free_sample(struct sample* s);
// loads wav from path and returns a new sample
struct sample* load_wav_into_sample(const char* path);

int trigger_sample(struct sample* s);

//------------------------------------------------------------------------------
// Bus Control Functions: bus.c
// -----------------------------------------------------------------------------

// returns empty bus or NULL on error
struct bus* init_bus(void);
// adds an input bus to parent, return 0 iff successful
int add_bus_in(struct bus* parent, struct bus* child);

// set playback settings
int set_atten(struct bus* b, float a);
int set_pan(struct bus* b, float p);

// processes audio through bus and copies the output to destination
// dest must be large enough to hold frames * frame_size
// return 0 iff successful
int process_bus(struct bus* master, void* dest, int frames);

#endif
