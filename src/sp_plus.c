#include "sp_plus.h"
#include "sp_plus_audio.h"
#include "smarc.h"

#include <math.h>
#include <string.h>
// TODO remove when not needed
#include <assert.h>

// TODO file loading interface needs to be created in platform code
#include "file.c"
// Wav paths for testing
#define WAV1 "../test/GREEN.wav"

static void trigger_sample(struct sample* s)
{
	if (!s->reverse) s->next_frame = s->start_frame;
	else s->next_frame = s->end_frame - 1.0;

	if (s->loop_mode && s->playing) s->playing = false;
	else s->playing = true;

	s->gate_closed = false;
}

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
	s->speed = 1.0f;
	s->loop_mode = LOOP_OFF;
	s->gate_closed = false;

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
	s->path = malloc(sizeof(char) * (strlen(path) + 1));
	strcpy(s->path, path);

	free(w.data);

	print_sample(s);
	return 0;
}
static int kill_sample(struct sample* s)
{
	if (!s) return 1;
	s->playing = false;
	s->gate_closed = false;
	if (s->reverse)	{
		s->next_frame = s->end_frame - 1;
		if (s->speed > 0) s->speed *= -1;
	} else {
		s->next_frame = s->start_frame;
		if (s->speed < 0) s->speed *= -1;
	}
	return 0;
}

static int increment_frame(struct sample* s)
{
	if (!s) return 1;
	// round up or down if fractional difference is very small
	double next_frame = s->next_frame + s->speed; 
	const double frac = next_frame - (int) next_frame;
	if (fabs(frac) < 0.001)
		next_frame = (int) next_frame;
	else if (fabs(frac) > 0.999)
		next_frame = (int) next_frame + 1;


	// control playback behavior when next_frame goes out of bounds
	// or when sample is in gate trigger mode and the gate is closed
	// Sample will either be killed or loop in LOOP or PONG_PONG mode

	// if sample playing forward goes out of bounds
	if (next_frame > s->end_frame - 1 ) {
		if (s->loop_mode == PING_PONG) { 
			s->next_frame = s->end_frame - 1;
			s->speed *= -1;
		} else if (s->loop_mode == LOOP) {
			s->next_frame = s->start_frame;
		} else {
			kill_sample(s);
		}
	// if sample playing backwards goes out of bounds
	} else if (next_frame < s->start_frame) {
		if (s->loop_mode == PING_PONG) {
			s->next_frame = s->start_frame;
			s->speed *= -1;
		} else if (s->loop_mode == LOOP) {
			s->next_frame = s->end_frame - 1;
		} else {
			kill_sample(s);
		}
	// logic for release of gate in gate trigger mode
	} else if (	s->gate_closed && 
			s->gate_release_cnt + fabs(next_frame - s->next_frame) > 
			s->gate_release) {
		kill_sample(s);
	} else {
		if (s->gate_closed) {
			s->gate_release_cnt += fabs(next_frame - s->next_frame);
		}
		s->next_frame = next_frame;
	}
	return 0;
}

static double get_envelope_gain(struct sample* s)
{
	double g = 1.0;
	if (s->attack && s->next_frame - s->start_frame < s->attack) {
		g = (s->next_frame - s->start_frame) / s->attack;
	} else if (s->release && s->end_frame - s->next_frame <= s->release) {
		g = (s->end_frame - s->next_frame) / s->release;
	}
	return g;
}

// get next frame and apply envelope gain
static int process_next_frame(double out[], struct sample* s)
{
	if (!s) return 1;
	// need to interpolate fractional next_frame
	const int32_t base_frame  = s->next_frame;
	const double ratio = s->next_frame - base_frame;
	for (int c = 0; c < NUM_CHANNELS; c++)
	{
		out[c] = s->data[base_frame * NUM_CHANNELS + c] * (1 - ratio);
		out[c] += s->data[(base_frame + 1) * NUM_CHANNELS + c] * ratio;
		if (s->gate_closed) {
			out[c] *= (s->gate_release - s->gate_release_cnt) /
				s->gate_release;
			out[c] *= s->gate_close_gain;
		}
		out[c] *= get_envelope_gain(s);
	}
	increment_frame(s);
	return 0;
}

// TODO: can probably make this stuff wayyyyy move efficient
// Recurse from master bus down to samples. Get next frame data from samples
// apply bus dsp to output. Each bus will have an array of bus inputs or
// a single sample input.
static void process_leaf_nodes(double out[], struct bus* b)
{
	// process sample input
	struct sample* s = b->sample_in;
	if (s && s->playing) {
		double tmp_out[NUM_CHANNELS] = { 0.0, 0.0 };
		process_next_frame(tmp_out, s);
		out[0] += tmp_out[0];
		out[1] += tmp_out[1];
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
int fill_audio_buffer(void *sp_state, void* buffer, int frames)
{
	struct bus master = ((struct sp_state *) sp_state)->master;
	static double out[NUM_CHANNELS] = {0, 0};
	// process i frames
	for (int i = 0; i < frames; i++){
		out[0] = 0;
		out[1] = 0;
		process_leaf_nodes(out, &master);
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

		memcpy((char *)buffer + i * sizeof(int_out), int_out, sizeof(int_out));
	}
	return 0;
}

void *allocate_sp_state(void)
{
	struct sp_state *s = calloc(1, sizeof(struct sp_state));
	if (!s) return NULL;

	// initialize a sample bank with 8 samples
	s->sampler.num_banks = 1;
	s->sampler.banks = calloc(1, sizeof(struct sample*));
	s->sampler.banks[0] = calloc(8, sizeof(struct sample));

	// load in a file
	// file_handle *file = file_open(WAV1);
	// load_wav(sample, file);

	s->master.sample_in = &(s->sampler.banks[0][0]);
	trigger_sample((s->master.sample_in));



	return (void *) s;
}

void update_and_render_sp_plus(
		void *sp_state, 
		char *pixel_buf, 
		int pixel_width,
		int pixel_height,
		int pixel_bytes)
{
}
