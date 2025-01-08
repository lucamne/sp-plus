#include "bus.h"


struct bus* init_bus(void)
{
	struct bus* b = malloc(sizoef(struct bus));
	if (!b)
		return NULL;
	b->sample_in = NULL;
	b->bus_in = NULL;
	b->num_bus_ins = 0;
	b->atten = 0.0f;
	b->pan = 0.5f;
	b->active = true;
	b->solo = false;

	return b;
}

static int16_t g_process_out [NUM_CHANNELS];

static void process_recurse(struct bus* b)
{
	struct sample* s = b->sample_in;
	if (s && s->playing) {
		g_process_out[0] = *s->next_frame;
		g_process_out[1] += *(s->next_frame + 1);
		// increment frame pointer or reset to beginning
		if (s->next_frame + 2 - s->data >= s->num_frames * 2) {
			s->next_frame = s->data;
			s->playing = s->loop ? true : false;
		} else {
			s->next_frame += 2;
		}
		return f;
	}
	// if bus not connected to a sample
	struct p_frame f;
	int16_t l = 0;
	int16_t r = 0;
	for (int i = 0; i < b->num_bus_ins; i++) {
		f = process_recurse(b->bus_in[i]);
		l += f.left;
		r += f.right;
	}
	return f;
}



int process_bus(struct bus* master, void* dest, int frames)
{
	g_process_out = {0, 0};
	for (int i = 0; i < frames; i++) {
		const struct p_frame f = p_frame_process_recurse(
