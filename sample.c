#include "defs.h"
#include "sample.h"
#include "io.h"

#include <stdlib.h>
#include <string.h>

static void print_sample(const struct sample* s)
{
	printf(
			"Frame size: %dB\n"
			"Sample Rate: %dHz\n",
			s->frame_size,
			s->rate);
}


struct sample* init_sample(void)
{
	static int _id = 0;
	struct sample* s = malloc(sizeof(struct sample));
	s->data = NULL;
	s->frame_size = 0;
	s->next_frame = NULL;
	s->id  = _id++;
	s->rate = 0;
	s->playing = true;
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

	s->num_frames = w->num_samples;
	s->frame_size = w->frame_size;
	s->rate = w->sample_rate;

	assert(sizeof(float) >= 2);
	s->data = realloc(s->data, sizeof(float) * s->num_frames * NUM_CHANNELS);
	s->next_frame = s->data;
	// convert data from int to float for later dsp
	for (int i = 0; i < s->num_frames; i++) {
		// convert left channel
		float f = ((float) w->data[2 * i]) / 32768.0f;
		if (f > 1.0f) f = 1.0f;
		else if (f < -1.0f) f = -1.0f;
		s->data[2 * i] = f;
		// convert right channel
		f = ((float) w->data[2 * i + 1]) / 32768.0f;
		if (f > 1.0f) f = 1.0f;
		else if (f < -1.0f) f = -1.0f;
		s->data[2 * i + 1] = f;
	}

	free(w);

	print_sample(s);

	return 0;
}
