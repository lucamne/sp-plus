#include "bus.h"
#include "defs.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

struct bus* init_bus(void)
{
	struct bus* b = malloc(sizeof(struct bus));
	if (!b)
		return NULL;
	b->sample_in = NULL;
	b->bus_ins = NULL;
	b->num_bus_ins = 0;
	b->atten = 0.0f;
	b->pan = 0.5f;
	b->active = true;
	b->solo = false;

	return b;
}

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

int set_atten(struct bus* b, float a)
{
	b->atten = a;
	if (b->atten < 0.0f)
		b->atten = 0.0f;
	else if (b->atten > 1.0f)
		b->atten = 1.0f;
	return 0;
}

int set_pan(struct bus* b, float p)
{
	b->pan = p;
	if (b->pan < -1.0f)
		b->pan = -1.0f;
	else if (b->pan > 1.0f)
		b->pan = 1.0f;
	return 0;
}

static int16_t out[NUM_CHANNELS] = {0, 0};

static void process_leaf_nodes(struct bus* b)
{
	// if bus input is a sample
	struct sample* s = b->sample_in;
	if (s && s->playing) {
		int left = *s->next_frame;
		int right = *(s->next_frame + 1);
		// apply attenuation
		left = (int) ((1.0f - b->atten) * (float) left);
		right = (int) ((1.0f - b->atten) * (float) right);
		// apply pan
		left = (int) ((float) left * fmin(1.0f - (float) b->pan, 1.0f));
		right = (int) ((float) right * fmin(1.0f + (float) b->pan, 1.0f));
		out[0] += (int16_t) left;
		out[1] += (int16_t) right;

		// increment frame pointer or reset to beginning
		if (s->next_frame + 2 >= s->data + s->num_frames * 2) {
			s->next_frame = s->data;
			s->playing = s->loop ? true : false;
		} else {
			s->next_frame += 2;
		}
		return;
	}

	for (int i = 0; i < b->num_bus_ins; i++)
		process_leaf_nodes(b->bus_ins[i]);
}

int process_bus(struct bus* master, void* dest, int frames)
{
	// process i frames
	for (int i = 0; i < frames; i++){
		out[0] = 0;
		out[1] = 0;
		process_leaf_nodes(master);
		memcpy((char *)dest + i * FRAME_SIZE, out, FRAME_SIZE);
	}
	return 0;
}
