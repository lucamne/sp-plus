#ifndef SYSTEM_H
#define SYSTEM_H

#include "audio_backend.h"

// holds core data structures of system
struct system {
	struct bus master;		// master bus, passed to audio callback
	// struct bus* aux_busses;	// non master busses
	// int num_aux_busses;
	struct sample** banks;		// all loaded samples [BANK][PAD]
	int num_banks;
	// struct alsa_dev playback_dev;// alsa playback device
};

enum { PAD_Q = 0, PAD_W, PAD_E, PAD_R };
typedef enum Marker { PLAY, START, END } Marker;
typedef enum GridMode { GRID_OFF = 0, AUTO, MANUAL} GridMode;
// holds data needed by sampler module
struct sampler {
	struct sample* active_sample;	// current sample to display
	int zoom;			// wave viewer zoom
	Marker zoom_focus;		// focal point of zoom
	int max_vert;			// max vertices to render in wave viewer
	// grid lines stuff
	int subdiv_top;			// grid subdivision e.g. 2/1, 1/1, 1/2
	int subdiv_bot;		
	GridMode grid_mode;
};

void draw(const struct system* sys, const struct sampler* sampler);

#endif
