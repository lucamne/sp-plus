#ifndef SIGNAL_CHAIN_H
#define SIGNAL_CHAIN_H

#include "audio_backend.h"
#include "defs.h"
#include "raylib.h"

#include <alsa/asoundlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/*
 * For representing and processing audio within the application
 * Luca Negris - 01/2025
 */

typedef enum {OFF = 0, LOOP, PING_PONG} LOOP_MODE;
// container for audio data
// the source of all playback is a sample
struct sample {
	char* path;
	double* data;		// 16_bit float data
	int32_t start_frame;	// start playback on this frame
	int32_t end_frame;	// end when this frame is reached 
	double next_frame;	// next frame to be played during playback
				// next frame is fractional to allow stretching

	int frame_size;		// size in bytes
	int32_t num_frames;
	float speed;		// playback speed
	int rate;		// sample_rate in Hz

	bool gate;		// trigger sample in gate mode
	bool playing;		// is sample currently playing
	LOOP_MODE loop_mode;
	bool reverse;		// is sample playing from start to end

	int32_t attack;		// attack in frames
	int32_t release;	// release in frames

	bool gate_closed;	// need to know when gate closes to apply release
	int32_t gate_release;	// used to calculate release after gate is closed
	// how many frames have passed since gate was closed
	double gate_release_cnt;
	double gate_close_gain; // gain at time of gate close
};

// Used to route and mix audio data
// Can have many bus inputs but only one sample input
// In practice busses will only have one output
struct bus {
	struct sample* sample_in;	// sample inputs
	struct bus** bus_ins;		// bus inputs
	int num_bus_ins;

	float atten;			// attenuation gain, [0.0, 1.0]
	float pan;			// -1.0 = L, 1.0 = R
	
	bool active;			// should date be grabbed from bus
	bool solo;			// is this bus soloed
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

//------------------------------------------------------------------------------
// Sample Contol Functions: sample.c
// -----------------------------------------------------------------------------

// load wav into provided sample, return 0 iff success
// NOTE: function not responsible for freeing any memory allocated by caller
int load_wav_into_sample(struct sample* s, const char* path);

int trigger_sample(struct sample* s);
// set start frame
int set_start(struct sample* s, int32_t frame);
// set end_frame
int set_end(struct sample* s, int32_t frame);
// set attack in ms
int set_attack(struct sample* s, float ms);
// set release in ms
int set_release(struct sample* s, float ms);
// controls behaviour of sample in gate trigger mode when gate is released
int close_gate(struct sample* s);
// process next frame into out and increment s->next_frame after
int process_next_frame(double out[], struct sample* s);
// stops playback and sets next_frame to correct position based on playback options
int kill_sample(struct sample* s);
// [1, 200], 100 is normal
void set_speed(struct sample* s, const float speed);

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

//------------------------------------------------------------------------------
// Alsa Control Functions: alsa.c
// -----------------------------------------------------------------------------

// opens and prepares alsa device handle for playback
int open_alsa_dev(struct alsa_dev* a_dev, int rate, int num_c);

// starts signal handler and callback which calls process_bus on master
int start_alsa_dev(struct alsa_dev* a_dev, struct bus* master);

//------------------------------------------------------------------------------
// Utility 
// -----------------------------------------------------------------------------

static double gain_to_dbfs(const double d) { return 20.0 * log10(fabs(d)); }
static double dbfs_to_gain(const double d) { return pow(10.0, d / 20.0); }
static float frames_to_ms(const int32_t f) { return 1000.0f * f / SAMPLE_RATE; }
static int32_t ms_to_frames(const float m) { return m * SAMPLE_RATE / 1000.0f; }
static float speed_to_st(const float speed) { return -12 * log2f(1.0f / speed); }
static float st_to_speed(const float st) { return powf(2.0f, st / 12.0f); }
#endif
