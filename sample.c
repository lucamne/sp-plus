#include "sample.h"
#include "io.h"

#include <stdlib.h>
#include <string.h>

static void print_sample(const struct sample* s)
{
	printf(
			"\nSize: %dB\n"
			"Frame size: %dB\n"
			"Sample Rate: %dHz\n",
			s->data_size,
			s->frame_size,
			s->rate);
}


struct sample* init_sample(void)
{
	static int _id = 0;
	struct sample* s = malloc(sizeof(struct sample));
	s->data = NULL;
	s->data_size = 0;
	s->frame_size = 0;
	s->next_frame = NULL;
	s->id  = _id++;
	s->rate = 0;
	s->playing = false;
	s->loop = false;
	return s;
}

int load_wav_into_sample(const char* path, struct sample* s)
{
	struct wav_file* w = load_wav(path);
	if (!w)
		return 1;
	// only deal with mono and stereo files for now
	if (w->num_channels != 2 && w->num_channels != 1) {
		printf("%d channel(s) not supported\n", w->num_channels);
		free(w);
		return 1;
	}
	// double samples to convert to stereo
	if (w->num_channels == 1) {
		int16_t* new_data = calloc(2, w->data_size);
		for (int i = 0; i < w->num_samples; i++) {
			new_data[2 * i] = w->data[i];
			new_data[2 * i + 1] = w->data[i];
		}
		free(w->data);
		w->data = new_data;
		w->data_size *= 2;
		w->frame_size *= 2;
	}

	s->data_size = w->data_size;
	s->frame_size = w->frame_size;
	s->num_frames = w->num_samples;
	s->rate = w->sample_rate;

	s->data = realloc(s->data, s->data_size);
	memcpy(s->data, w->data, s->data_size);
	s->next_frame = s->data;

	free(w);

	print_sample(s);

	return 0;
}
