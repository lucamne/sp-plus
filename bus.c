#include "bus.h"
#include "defs.h"

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


static void process_leaf_nodes(struct bus* b, int16_t* out)
{
	struct sample* s = b->sample_in;
	// if bus input is a sample
	if (s && s->playing) {
		out[0] += *s->next_frame;
		out[1] += *(s->next_frame + 1);
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
		process_leaf_nodes(b->bus_ins[i], out);
}

int process_bus(struct bus* master, void* dest, int frames)
{
	// process i frames
	for (int i = 0; i < frames; i++){
		int16_t* out = calloc(2, sizeof(int16_t));
		out[0] = 0;
		out[1] = 0;
		process_leaf_nodes(master, out);
		memcpy((char *)dest + i * FRAME_SIZE, out, FRAME_SIZE);
		free(out);
	}
	return 0;
}
