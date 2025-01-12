#include "playback.h"
#include "defs.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

// attach child to the input of parent
int add_bus_in(struct bus* parent, struct bus* child)
{
	parent->bus_ins = realloc(
			parent->bus_ins, 
			++(parent->num_bus_ins) * sizeof(struct bus*));
	if (!parent->bus_ins)
		return 1;

	parent->bus_ins[parent->num_bus_ins - 1] = child;
	return 0;
}

// set bus attenuation
int set_atten(struct bus* b, float a)
{
	b->atten = a;
	if (b->atten < 0.0f)
		b->atten = 0.0f;
	else if (b->atten > 1.0f)
		b->atten = 1.0f;
	return 0;
}

// set bus pan 
int set_pan(struct bus* b, float p)
{
	b->pan = p;
	if (b->pan < -1.0f)
		b->pan = -1.0f;
	else if (b->pan > 1.0f)
		b->pan = 1.0f;
	return 0;
}

/*
 * Recurse from master bus down to samples. Get next frame data from samples
 * apply bus dsp to output. Each bus will have an array of bus inputs or
 * a single sample input.
 */
static double out[NUM_CHANNELS] = {0, 0};
static void process_leaf_nodes(double out[], struct bus* b)
{
	// process sample input
	struct sample* s = b->sample_in;
	if (s && s->playing) {
		// apply sample dsp
		out[0] += *(s->next_frame);
		out[1] += *(s->next_frame + 1);
		// increment frame pointer or reset to beginning
		if (s->next_frame + NUM_CHANNELS >= 
				s->data + s->num_frames * NUM_CHANNELS) {
			s->next_frame = s->data;
			s->playing = s->loop ? true : false;
		} else {
			s->next_frame += NUM_CHANNELS;
		}
		return;
	}
	// process bus inputs
	for (int i = 0; i < b->num_bus_ins; i++)
		process_leaf_nodes(out, b->bus_ins[i]);

	// apply bus dsp
	out[0] *= (1.0 - b->atten);
	out[1] *= (1.0 - b->atten);
	out[0] *= fmin(1.0 - b->pan, 1.0);
	out[1] *= fmin(1.0 + b->pan, 1.0);
}

// Process the next n frames and copy result to dest
// In practice this is used for the alsa callback function
int process_bus(struct bus* master, void* dest, int frames)
{
	// process i frames
	for (int i = 0; i < frames; i++){
		out[0] = 0;
		out[1] = 0;
		process_leaf_nodes(out, master);
		// alsa expects 16 bit int
		int16_t int_out[NUM_CHANNELS];
		// convert left
		double f = out[0] * 32768.0; 
		if (f > 32767.0) f = 32767.0;
		if (f < -32768.0) f = -32768.0;
		int_out[0] = (int16_t) f;
		// convert right
		f = out[1] * 32768.0; 
		if (f > 32767.0) f = 32767.0;
		if (f < -32768.0) f = -32768.0;
		int_out[1] = (int16_t) f;

		memcpy((char *)dest + i * FRAME_SIZE, int_out, FRAME_SIZE);
	}
	return 0;
}
