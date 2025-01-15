#ifndef SYSTEM_H
#define SYSTEM_H

#include "audio_backend.h"

// holds core data structures of system
struct system {
	struct bus master;		// master bus, passed to audio callback
	// struct bus* aux_busses;		// non master busses
	// int num_aux_busses;
	struct sample** banks;	// all loaded samples [BANK][PAD]
	int num_banks;
	// struct alsa_dev playback_dev;	// alsa playback device
};

typedef enum { PLAY, START, END } Marker;
// holds data needed by sampler module
struct sampler {
	struct sample* active_sample;
	float zoom;
	Marker zoom_focus;
};

void draw(const struct system* sys, const struct sampler* sampler);

#endif
