#include "bus.h"


struct bus* init_bus(void)
{
	struct bus* b = malloc(sizoef(struct bus));
	if (!b)
		return NULL;
	b->sample_in = NULL;
	b->bus_in = NULL;
	b->atten = 0.0f;
	b->pan = 0.5f;
	b->active = true;
	b->solo = false;

	return b;
}

static int process_helper

int process_bus(struct bus* master, void* dest, int frames)
{
	for (int i = 0; i < frames; i++) {
		int left = 0;
		int right = 0;
		// get next frame from sample
		struct sample* s = master->sample_in;
		if (s && s->playing) {
			left += *s->next_frame;
			right += *(s->next_frame + 1);
			// increment frame pointer or reset to beginning
			if (s->next_frame + 2 - s->data >= s->num_frames * 2) {
				s->next_frame = s->data;
				s->playing = s->loop ? true : false;
			} else {
				s->next_frame += 2;
			}
		}
		// iterate child busses
