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

// Used to route and mix audio data
// busses will have one output allowing mixer to be represented as a tree
//
// busses can have no inputs, a list of bus inputs, or one sample input
// these restrictions are enforced by audio playback loop
struct bus {
	char *label; 			// bus label
	enum {				// bus type
		UNASSIGNED,		// affects modification of bus
		MASTER,
		AUX,
		SAMPLE
	} type;

	struct sample *sample_in;	// sample inputs
	struct bus** bus_ins;		// bus inputs
	int num_bus_ins;

	struct bus *output_bus;		// output bus, used for mixer tree manipulation

	float atten;			// attenuation gain, [0.0, 1.0]
	float pan;			// -1.0 = L, 1.0 = R
	
	// bool active;			// should data be grabbed from bus
	// bool solo;			// is this bus soloed
};


#define R_BUFF_MAX 64			// bytes to allocate when allocating rename buff
struct mixer {
	struct bus master;		// bus tree root
					// gets passed to playback code
	void *master_mutex;		// mutex for bus tree
	
	struct bus **bus_list;		// pointers to busses to be used by ui
					// need a lock for this if update
					// and ui code run conccurently

	int num_bus;			// number of busses
	int next_label;			// give a new bus this number

 	int selected_bus;		// currently hovered bus

	enum {				// controls update pattern for mixer update
		NORMAL = 0,
		DELETE,
		RENAME
	} update_mode;

	char *r_buff;			// buffer used when capturing input to rename bus
	int r_buff_pos;			// next byte to write in r_buff
};

// container for audio data
// the source of all playback is a sample
struct sample {
	char* name;

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

	struct bus *output_bus;	// used for manipulating mixer structure
};


struct glyph {
	unsigned char *bitmap;
	int w;
	int h;
	int x_off;
	int y_off;
};

#define NUM_FONTS 1
enum font_types {MED};
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

struct shell {
	char *input_buff;
	int input_pos;
	int input_size;

	char *output_buff;
	int output_size;
};

// program state held by platform code
struct sp_state {
	struct mixer mixer;
	struct sampler sampler;
	struct shell shell;
	struct file_browser file_browser;

	struct font fonts[NUM_FONTS]; // array of fonts

	enum {
		SAMPLER = 0,
		MIXER,
		SHELL,
		FILE_BROWSER
	} control_mode;
};

#endif
