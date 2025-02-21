#ifndef SP_TYPES_H
#define SP_TYPES_H

#include "stb_truetype.h"

#include <stdint.h>
#include <stdbool.h>

// TODO should not be named audio_types

// registers sampler module state
enum Pad { PAD_Q = 0, PAD_W, PAD_E, PAD_R, PAD_A, PAD_S, PAD_D, PAD_F };
struct sampler {
	struct sample ***banks;		// referenecs to loaded samples [BANK][PAD]
	int num_banks;

	struct sample *active_sample;	// current sample to display
	int curr_pad;			// current pad to display (corresponds to active_sample)
	int curr_bank;			// currently selected sample bank

	int zoom;			// wave viewer zoom
	enum { 
		PLAY, 
		START, 
		END 
	} zoom_focus;			// focal point of zoom

	enum {
		NONE,
		MOVE,
		COPY
	} move_mode;
	struct sample **pad_src;	// pad to move or copy sample from

	int max_vert;			// max vertices to render in wave viewer
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
	
	// bool active;			// should data be grabbed from bus
	// bool solo;			// is this bus soloed
};

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
	enum {
		LOOP_OFF = 0, 
		LOOP, 
		PING_PONG
	} loop_mode;
	bool reverse;		// is sample playing from start to end

	int32_t attack;		// attack in frames
	int32_t release;	// release in frames

	bool gate_closed;	// need to know when gate closes to apply release
	int32_t gate_release;	// used to calculate release after gate is closed
	// how many frames have passed since gate was closed
	double gate_release_cnt;
	double gate_close_gain; // gain at time of gate close
};


struct glyph {
	unsigned char *bitmap;
	int w;
	int h;
	int x_off;
	int y_off;
};

#define NUM_FONTS 1
enum FontTypes {MED};
struct font {
	struct glyph *glyphs;
	int height;		// font height approx in pixels
};

// program state held by platform code
struct sp_state {
	struct bus master;
	struct sampler sampler;
	struct font fonts[NUM_FONTS]; // array of fonts
};


#endif
