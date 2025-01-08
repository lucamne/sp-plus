#ifndef BUS_H
#define BUS_H

#include "sample.h"
#include <stdbool.h>

struct bus {
	struct sample* sample_in;	// sample input if any
	struct bus** bus_in;		// bus inputs
	int num_bus_ins;

	float atten;			// attenuation, 0.0 is none 1.0 is max
	float pan;			// 0.0 = L, 1.0 = R
	
	bool active;			// should date be grabbed from bus
	bool solo;			// is this bus soloed
};

struct bus* init_bus(void);

// processes incoming samples and copies the output to destination
// dest must be large enough to hold frames * frame_size
int process_bus(struct bus* master, void* dest, int frames);

#endif
