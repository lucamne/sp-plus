#ifndef BUS_H
#define BUS_H

#include "sample.h"
#include <stdbool.h>

// bus can either have a sample input or other busses as inputs, not both
// if a sample is attached then any attached busses will not process
struct bus {
	struct sample* sample_in;	// sample input if any
	struct bus** bus_ins;		// bus inputs
	int num_bus_ins;

	float atten;			// attenuation, 0.0 is none 1.0 is max
	float pan;			// -1.0 = L, 1.0 = R
	
	bool active;			// should date be grabbed from bus
	bool solo;			// is this bus soloed
};

struct bus* init_bus(void);

// adds an input bus to parent
int add_bus_in(struct bus* parent, struct bus* child);

// set atten between 0.0 and 1.0
int set_atten(struct bus* b, float a);

// set atten between -1.0 and 1.0
int set_pan(struct bus* b, float p);

// processes incoming samples and copies the output to destination
// dest must be large enough to hold frames * frame_size
int process_bus(struct bus* master, void* dest, int frames);

#endif
