#ifndef SP_TYPES_H
#define SP_TYPES_H

#include "stb_truetype.h"

#include <stdint.h>
#include <stdbool.h>

// TODO might want to make a pad struct that holds reference to a sample 
// and contains information about its bank and pad

// registers sampler module state
enum Pad { PAD_Q = 0, PAD_W, PAD_E, PAD_R, PAD_A, PAD_S, PAD_D, PAD_F };
struct sampler {
	struct sample ***banks;		// referenecs to loaded samples [BANK][PAD]
	int num_banks;

	struct sample *active_sample;	// current sample to display
	int curr_bank;			// currently selected sample bank

	enum {
		NONE,
		SWAP,
		COPY
	} move_mode;

	struct sample **pad_src;	// pad to move or copy sample from
	int pad_src_bank;		// tracks the bank of the pad src
	int pad_src_pad;		// tracks the pad of the pad src

	// ui state info
	int curr_pad;			// current pad to display (corresponds to active_sample)
	int zoom;			// wave viewer zoom
					//
	enum { 
		PLAY, 
		START, 
		END 
	} zoom_focus;			// focal point of zoom

	int max_vert;			// max vertices to render in wave viewer
};

// TODO will change when more mixer code is written
// Used to route and mix audio data
// In practice busses will only have one output
struct bus {
	struct sample** sample_ins;	// sample inputs
	int num_sample_ins;

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

// file item information
struct file_item {
	char *name;		// name is relative to file_browser working directory
				// TODO might change that in the future
	int is_dir;		// true if file is a directory
};

struct file_browser {
	char *dir;
	struct file_item *files;
	int num_files;

	int selected_file;	// index of currently selected file
	int loading_to_pad;	// true iff waiting for pad destination to load file
};

// program state held by platform code
struct sp_state {
	struct bus master;
	struct sampler sampler;
	struct file_browser file_browser;
	struct font fonts[NUM_FONTS]; // array of fonts

	enum {
		SAMPLER = 0,
		FILE_BROWSER
	} control_mode;
};


#endif
