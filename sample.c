#include "sample.h"
#include "io.h"

#include <stdlib.h>
#include <string.h>

struct sample* init_sample(void)
{
	static int _id = 0;
	struct sample* s = malloc(sizeof(struct sample));
	s->data = NULL;
	s->data_size = 0;
	s->frame_size = 0;
	s->next_frame = NULL;
	s->id  = _id++;
	s->playing = false;
	s->loop = false;
	return s;
}

int load_wav_into_sample(const char* path, struct sample* s)
{
	struct wav_file* w = load_wav(path);
	if (!w)
		return 1;

	s->data_size = w->data_size;
	s->frame_size = w->frame_size;

	s->data = realloc(s->data, s->data_size);
	memcpy(s->data, w->data, s->data_size);
	s->next_frame = s->data;

	free(w);
	return 0;
}

const char* get_next_frame(struct sample* s)
{	
	if (s->next_frame >= s->data + s->data_size) {
		s->next_frame = s->data;
		if (!s->loop)
			s->playing = false;
	}

	if (!s->playing)
		return NULL;

	char* r = s->next_frame;
	s->next_frame += s->frame_size;
	return r;
}

int reset_sample(struct sample* s)
{
	s->next_frame = s->data;
	s->playing = false;
	return 0;
}
