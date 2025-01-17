#include "defs.h"
#include "audio_backend.h"
#include "file_io.h"
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

// read wav file into provided sample
int load_wav_into_sample(struct sample* s, const char* path)
{
	// zero out value which will not be reset
	// watch for memory leak with data
	s->data = NULL;
	s->playing = false;
	s->frame_increment = 1.0f;
	s->loop_mode = OFF;

	struct wav_file w = {0};
	if (load_wav(&w, path)) return 1;

	// only deal with mono and stereo files for now
	if (w.num_channels != 2 && w.num_channels != 1) {
		printf("%d channel(s) not supported\n", w.num_channels);
		return 1;
	}
	// convert to stereo if necessary
	if (w.num_channels == 1) {
		int16_t* new_data = calloc(2, w.data_size);
		for (int i = 0; i < w.num_samples; i++) {
			new_data[2 * i] = w.data[i];
			new_data[2 * i + 1] = w.data[i];
		}
		free(w.data);
		w.data = new_data;
		w.data_size *= 2;
		w.frame_size *= 2;
	}

	s->num_frames = w.num_samples;
	s->frame_size = w.frame_size;
	s->rate = w.sample_rate;

	// convert data from float to double
	assert(sizeof(double) >= 2);
	s->data = realloc(s->data, sizeof(double) * s->num_frames * NUM_CHANNELS);
	for (int i = 0; i < s->num_frames; i++) {
		// convert left channel
		double f = ((double) w.data[NUM_CHANNELS * i]) / 32768.0;
		if (f > 1.0) f = 1.0;
		else if (f < -1.0) f = -1.0;
		s->data[NUM_CHANNELS * i] = f;
		// convert right channel
		f = ((double) w.data[NUM_CHANNELS * i + 1]) / 32768.0;
		if (f > 1.0) f = 1.0;
		else if (f < -1.0) f = -1.0;
		s->data[NUM_CHANNELS * i + 1] = f;
	}
	// resample if necessary
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
	s->next_frame = 0;
	s->start_frame = 0;
	s->end_frame = s->num_frames;
	free(w.data);

	print_sample(s);
	return 0;
}


int trigger_sample(struct sample* s)
{
	if (!s->reverse) s->next_frame = s->start_frame;
	else s->next_frame = s->end_frame - 1.0;

	if (s->loop_mode && s->playing)
		s->playing = false;
	else
		s->playing = true;
}

int set_start(struct sample* s, int32_t frame)
{
	if (!s) return 1;

	if (frame < 0) s->start_frame = 0;
	else if (frame >= s->end_frame) s->start_frame = s->end_frame - 1;
	else s->start_frame = frame;

	if (!s->playing) kill_sample(s);
	return 0;
}

int set_end(struct sample* s, int32_t frame)
{
	if (!s) return 1;

	if (frame > s->num_frames) s->end_frame = s->num_frames;
	else if (frame <= s->start_frame) s->end_frame = s->start_frame + 1;
	else s->end_frame = frame;

	if (!s->playing) kill_sample(s);
	return 0;
}

static int increment_frame(struct sample* s)
{
	if (!s) return 1;
	// round up or down if fractional difference is very small
	double next_frame = s->next_frame + s->frame_increment;
	const double frac = next_frame - (int) next_frame;
	if (frac < 0.001)
		next_frame = (int) next_frame;
	else if (frac > 0.999)
		next_frame = (int) next_frame + 1.0;

	// control playback behavior when next_frame goes out of bounds
	// sample will either be killed or loop in LOOP or PONG_PONG mode
	// if sample playing forward goes out of bounds
	if (next_frame > s->end_frame - 1.0 ) {
		if (s->loop_mode == PING_PONG) { 
			s->next_frame = s->end_frame - 1.0;
			s->frame_increment *= -1;
		} else if (s->loop_mode == LOOP) {
			s->next_frame = s->start_frame;
		} else {
			kill_sample(s);
		}
	// if sample playing backwards goes out of bounds
	} else if (next_frame < s->start_frame) {
		if (s->loop_mode == PING_PONG) {
			s->next_frame = s->start_frame;
			s->frame_increment *= -1;
		} else if (s->loop_mode == LOOP) {
			s->next_frame = s->end_frame - 1.0;
		} else {
			kill_sample(s);
		}
	} else {
		s->next_frame = next_frame;
	}
	return 0;
}

int process_next_frame(double out[], struct sample* s)
{
	if (!s) return 1;
	// need to interpolate fractional next_frame
	const int32_t base_frame  = (int) s->next_frame;
	const double ratio = s->next_frame - (double) base_frame;
	for (int c = 0; c < NUM_CHANNELS; c++)
	{
		out[c] = s->data[base_frame * NUM_CHANNELS + c] * (1.0 - ratio);
		out[c] += s->data[(base_frame + 1) * NUM_CHANNELS + c] * ratio;
		if (s->attack && s->next_frame - s->start_frame < s->attack) {
			out[c] *= (double) (s->next_frame - s->start_frame) / 
				(double) s->attack;
		} else if (s->release && s->end_frame - s->next_frame <= s->release) {
			out[c] *= (double) (s->end_frame - s->next_frame) / 
				(double) s->release;
		}
	}
	increment_frame(s);
	return 0;
}

int kill_sample(struct sample* s)
{
	if (!s) return 1;
	s->playing = false;
	if (s->reverse)	{
		s->next_frame = s->end_frame - 1.0;
		if (s->frame_increment > 0) s->frame_increment *= -1.0;
	} else {
		s->next_frame = s->start_frame;
		if (s->frame_increment < 0) s->frame_increment *= -1.0;
	}
	return 0;
}
