#include "sp_plus.h"
#include "sp_plus_audio_types.h"
#include "smarc.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

// Wav paths for testing
#define WAV1 "../test/GREEN.wav"



/* Audio playback */

static void trigger_sample(struct sample* s)
{
	if (!s->reverse) s->next_frame = s->start_frame;
	else s->next_frame = s->end_frame - 1.0;

	if (s->loop_mode && s->playing) s->playing = false;
	else s->playing = true;

	s->gate_closed = false;
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

// called by platform in async callback
// relies on other audio playback functions
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

/* Loading sample from wav file buffer */

// used for debugging
// TODO: delete and replace with logging
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

// check endianess of system at runtime
static bool is_little_endian(void)
{
	int n = 1;
	if(*(char *)&n == 1) 
		return true;
	return false;
}

// loads sample from a buffer in wav format
// currently supports only non compressed WAV files
// sample should be initialized before being passed to load_wav()
// returns 0 iff success
static int load_sample_from_wav_buffer(struct sample *s, char *buffer, const long buf_bytes)
{
	if (!s) return 1;
	// setup sample
	s->path = NULL;
	s->data = NULL;
	s->start_frame = 0;
	s->end_frame = 0;	
	s->next_frame = 0;
	s->frame_size = 0;		
	s->num_frames = 0;
	s->speed = 1.0;	
	s->rate = 0;
	s->gate = 0;		
	s->playing = 0;
	s->loop_mode = 0;
	s->reverse = 0;
	s->attack = 0;		
	s->release = 0;
	s->gate_closed = 0;	
	s->gate_release = 0;	
	s->gate_release_cnt = 0;
	s->gate_close_gain = 0;


	// TODO support big_endian systems as well
	if (!is_little_endian) {
		fprintf(stderr, "Only little-endian systems are supported\n");
		return 1;
	}

	// any read should not read the end_ptr or anything past it
	// data is stored in 4 byte words
	const char *end_ptr = buffer + buf_bytes;
	const int word = 4;

	/* parse headers */
	// check for 'RIFF' tag bytes [0, 3]
	if (*(int32_t *) buffer ^ 0x46464952) return 1;
	
	// get master chunk_size 
	buffer += word;
	const int64_t mstr_ck_size = *(int32_t *) buffer;
	// verify we won't access data outside buffer
	// TODO more bounds checking should be added in case wav file is not properly formatted
	if (buffer + word + mstr_ck_size > end_ptr) return 1;

	// check for 'WAVE' tag
	// TODO spec does not require wave tag here
	buffer += word;
	if (*(int32_t *) buffer ^ 0x45564157) return 1;

	// check for 'fmt ' tag
	buffer += word;
	if (*(int32_t *) buffer ^ 0x20746D66) return 1;

	// support only non-extended PCM for now
	buffer += word;
	const int32_t fmt_ck_size = *buffer;
	if (fmt_ck_size != 16) return 1;

	/* parse 'fmt ' chunk */
	// check PCM format and number of channels
	buffer += word;
	if ((*(int32_t *) buffer & 0xFFFF) != 1) return 1;
	const int num_channels = *(int32_t *) buffer >> 16;
	if (num_channels != 1 && num_channels != 2) {
		fprintf(stderr, "%d channel(s) not supported\n", num_channels);
		return 1;
	}

	buffer += word;
	const int sample_rate = *(int32_t *) buffer;

	buffer += 2 * word;
	const int frame_size = *(int32_t *) buffer & 0xFFFF;
	const int bit_depth = *(int32_t *) buffer >> 16;
	if (bit_depth != 16)
		fprintf(stderr, "Warning bitdepth is %d\n", bit_depth);

	// TODO if extended format (fmt_ck_size > 16) then more data needs to be read

	/* parse data chunk */
	// seek to 'data' chunk
	buffer += word;
	while (*(int32_t *) buffer ^ 0x61746164) { 		
		// find current chunk size and seek to next chunk
		// bounds checking is performed in case 'data' chunk is never found
		buffer += word;
		if (buffer + word > end_ptr) return 1;
		const int s = *(int32_t *) buffer;
		buffer += word + s;
		if (buffer + word > end_ptr) return 1;
	}

	buffer += word;
	const int32_t data_size = *(int32_t *) buffer;
	const int32_t num_frames = data_size / frame_size;

	/* register sample info and pcm data */
	// register some fields
	s->frame_size = frame_size;
	s->num_frames = num_frames;
	s->end_frame = num_frames;
	s->rate = sample_rate;

	// allocate sample memory
	s->data = malloc(num_frames * NUM_CHANNELS * sizeof(double));
	if (!s->data) {
		fprintf(stderr, "Sample memory allocation error\n");
		return 1;
	}

	// convert samples from int to double and mono to stereo if necassary
	// then store data in s->data
	buffer += word;
	for (int i = 0; i < num_frames; i++) {
		double l;
		double r;
		// left = right if data is mono
		if (num_channels == 1)
		{
			l = ((double) ((int16_t *) buffer)[i]) / 32768.0;
			l = r;
		} else {
			l = ((double) ((int16_t *) buffer)[NUM_CHANNELS * i]) / 32768.0;

			r = ((double) ((int16_t *) buffer)[NUM_CHANNELS * i + 1]) / 32768.0;
		}
		// bounds checking
		if (l > 1.0) l = 1.0;
		else if (l < -1.0) l = -1.0;
		if (r > 1.0) r = 1.0;
		else if (r < -1.0) r = -1.0;

		s->data[NUM_CHANNELS * i] = l;
		s->data[NUM_CHANNELS * i + 1] = r;
	}

	// resample to SAMPLE_RATE constant if necessary
	if (s->rate != SAMPLE_RATE) {
		const int r = resample(s, s->rate, SAMPLE_RATE);
		if (r == -1) {
			fprintf(stderr, "Resampling Error\n");
			free(s->data);
			return 1;
		}
		s->num_frames = r;
		s->end_frame = r;
		s->rate = SAMPLE_RATE;
	}

	print_sample(s);
	return 0;
}

/* Initialization */

// state allocation service called by platform
// use this function for initializing debugging data
void *allocate_sp_state(void)
{
	struct sp_state *s = calloc(1, sizeof(struct sp_state));
	if (!s) return NULL;

	// initialize a sample bank with 8 samples
	s->sampler.num_banks = 1;
	s->sampler.banks = calloc(1, sizeof(struct sample*));
	s->sampler.banks[0] = calloc(8, sizeof(struct sample));

	// load whole file into a buffer
	void *buffer = NULL;
	const int64_t buf_bytes = platform_load_entire_file(&buffer, WAV1);
	if (buf_bytes) {
		if (!load_sample_from_wav_buffer(&(s->sampler.banks[0][0]), buffer, buf_bytes)) {
			s->master.sample_in = &(s->sampler.banks[0][0]);
			trigger_sample((s->master.sample_in));
		}
		platform_free_file_buffer(&buffer);
	}


	return (void *) s;
}

/* Update and Rendering */

void update_and_render_sp_plus(
		void *sp_state, 
		char *pixel_buf, 
		int pixel_width,
		int pixel_height,
		int pixel_bytes)
{
}
