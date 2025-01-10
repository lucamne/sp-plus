#include "defs.h"
#include "signal_chain.h"
#include "io.h"
#include "smarc.h"

#include <stdlib.h>
#include <string.h>

static void print_sample(const struct sample* s)
{
	printf(
			"***********************\n"
			"Sample Info:\n"
			"-----------------------\n"
			"Frame size: %dB\n"
			"Sample Rate: %dHz\n"
			"Num Frames: %d\n"
			"***********************\n",
			s->frame_size,
			s->rate,
			s->num_frames);
}

// change sample's sample rate from rate_in to rate_out
static int resample(struct sample* s, int rate_in, int rate_out)
{
	double bandwidth = 0.95;  // bandwidth
	double rp = 0.1; // passband ripple factor
	double rs = 140; // stopband attenuation
	double tol = 0.000001; // tolerance

	// initialize smarc filter
	struct PFilter* pfilt = smarc_init_pfilter(rate_in, rate_out, 
			bandwidth, rp, rs, tol, NULL, 0);
	if (!pfilt)
		return -1;

	struct PState* pstate[NUM_CHANNELS];
	pstate[0] = smarc_init_pstate(pfilt);
	pstate[1] = smarc_init_pstate(pfilt);

	const int OUT_SIZE = 
		(int) smarc_get_output_buffer_size(pfilt, s->num_frames);
	double* outbuf = malloc(OUT_SIZE * sizeof(double));
	double* left_inbuf = malloc(s->num_frames * sizeof(double));
	double* right_inbuf = malloc(s->num_frames * sizeof(double));

	// extract left and right channels
	for (int i = 0; i < s->num_frames; i++) {
		left_inbuf[i] = s->data[i * NUM_CHANNELS];
		right_inbuf[i] = s->data[i * NUM_CHANNELS + 1];
	}
	s->data = realloc(s->data, sizeof(double) * OUT_SIZE * NUM_CHANNELS);
	if (!s->data)
		return -1;

	// resample left channel
	int w = smarc_resample( pfilt, pstate[0], left_inbuf, 
			s->num_frames, outbuf, OUT_SIZE);
	for (int i = 0; i < w; i++)
		s->data[i * NUM_CHANNELS] = outbuf[i];

	// resample right channel
	w = smarc_resample( pfilt, pstate[1], right_inbuf, 
			s->num_frames, outbuf, OUT_SIZE);
	for (int i = 0; i < w; i++)
		s->data[i * NUM_CHANNELS + 1] = outbuf[i];

	smarc_destroy_pstate(pstate[0]);
	smarc_destroy_pstate(pstate[1]);
	free(left_inbuf);
	free(right_inbuf);
	free(outbuf);
	return w;
}

// create empty sample
struct sample* init_sample(void)
{
	static int _id = 0;
	struct sample* s = malloc(sizeof(struct sample));
	s->data = NULL;
	s->frame_size = 0;
	s->next_frame = NULL;
	s->rate = 0;
	s->playing = true;
	s->loop = false;
	return s;
}

// read wav file and load into existing sample
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
	// format sample data
	assert(sizeof(double) >= 2);
	s->data = realloc(s->data, sizeof(double) * s->num_frames * NUM_CHANNELS);
	// convert data from int to float for later dsp
	for (int i = 0; i < s->num_frames; i++) {
		// convert left channel
		double f = ((double) w->data[NUM_CHANNELS * i]) / 32768.0;
		if (f > 1.0) f = 1.0;
		else if (f < -1.0) f = -1.0;
		s->data[NUM_CHANNELS * i] = f;
		// convert right channel
		f = ((double) w->data[NUM_CHANNELS * i + 1]) / 32768.0;
		if (f > 1.0) f = 1.0;
		else if (f < -1.0) f = -1.0;
		s->data[NUM_CHANNELS * i + 1] = f;
	}

	if (s->rate != SAMPLE_RATE) {
		const int r = resample(s, s->rate, SAMPLE_RATE);
		if (r == -1) {
			fprintf(stderr, "Resampling Error\n");
			return 1;
		}
		s->num_frames = r;
		s->rate = SAMPLE_RATE;
	}
	// initiliaze here because data may have been reallocated
	s->next_frame = s->data;

	free(w);

	print_sample(s);

	return 0;
}


