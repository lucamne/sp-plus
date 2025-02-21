// internal
#include "sp_plus.h"
#include "sp_types.h"
#include "sp_plus_assert.h"

// external
#include "smarc.h"
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// libc
#include <math.h>
#include <string.h>
#include <stdlib.h>

// paths for testing
#define WAV1 "../test/GREEN.wav"
#define WAV2 "../test/GREEN.wav"
#define FONT1 "../fonts/DejaVuSans-Bold.ttf"

// TODO: may want to move these eventually
// currently used in sp_plus_allocate_state and proccess_input_and_update
static const int NUM_PADS = 8;

// utility stuff used by draw_ui.c and update
static float st_to_speed(const float st) { return powf(2.0f, st / 12.0f); }
static float speed_to_st(float speed) { return -12 * log2f(1.0f / speed); }
static int32_t ms_to_frames(const float m) { return m * SAMPLE_RATE / 1000.0f; }
static float frames_to_ms(const int32_t f) { return 1000.0f * f / SAMPLE_RATE; }

// .c includes
#include "draw_ui.c"



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
int sp_plus_fill_audio_buffer(void *sp_state, void* buffer, int frames)
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

	if (!outbuf || !left_inbuf || !right_inbuf) {
		fprintf(stderr, "Error allocating memory for resampling\n");
		exit(1);
	}

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
// TODO consider moving initialization code
void *sp_plus_allocate_state(void)
{
	struct sp_state *s = calloc(1, sizeof(struct sp_state));
	if (!s) return NULL;

	/* Initializing sp_state */
	// initialize a sample bank with 8 samples
	s->sampler.zoom = 1;
	s->sampler.num_banks = 1;
	s->sampler.banks = calloc(1, sizeof(struct sample **));
	if (!s->sampler.banks) {
		fprintf(stderr, "Error allocating state memory\n");
		exit(1);
	}

	s->sampler.banks[0] = calloc(8, sizeof(struct sample *));
	if (!s->sampler.banks[0]) {
		fprintf(stderr, "Error allocating state memory\n");
		exit(1);
	}
	s->sampler.max_vert = 2000;



	// initialize fonts
	void *ttf_buffer = NULL;
	platform_load_entire_file(&ttf_buffer, FONT1);
	load_font(&(s->fonts[MED]), ttf_buffer, 20);
	platform_free_file_buffer(&ttf_buffer);


	// DEBUGGING CODE

	// load whole file into a buffer for DEBUG
	void *buffer = NULL;
	int64_t buf_bytes = platform_load_entire_file(&buffer, WAV1);
	struct sampler *sampler = &(s->sampler);

	if (buf_bytes) {
		if(s->sampler.banks[0][0] = malloc(sizeof(struct sample))) {
			if (!load_sample_from_wav_buffer(s->sampler.banks[0][0], buffer, buf_bytes)) {
				// TODO consider where path should be set
				s->sampler.banks[0][0]->path = WAV1;
			} else {
				fprintf(stderr, "File load failed: %s\n", WAV1);
			}

			platform_free_file_buffer(&buffer);
		}
	}

	buf_bytes = platform_load_entire_file(&buffer, WAV2);

	if (buf_bytes) {
		if(s->sampler.banks[0][1] = malloc(sizeof(struct sample))) {
			if (!load_sample_from_wav_buffer(s->sampler.banks[0][1],  buffer, buf_bytes)) {
				// TODO consider where path should be set
				s->sampler.banks[0][1]->path = WAV2;
			} else {
				fprintf(stderr, "File load failed: %s\n", WAV2);
			}

			platform_free_file_buffer(&buffer);
		}
	}

	struct bus *b1 = calloc(sizeof(struct bus), 0);
	struct bus *b2 = calloc(sizeof(struct bus), 0);
	if (!b1 || !b2) {
		fprintf(stderr, "Error allocating state memory\n");
		exit(1);
	}

	b1->sample_in = s->sampler.banks[0][PAD_Q];
	b2->sample_in = s->sampler.banks[0][PAD_W];

	s->master.num_bus_ins = 2;
	s->master.bus_ins = malloc(sizeof(struct bus *) * 2);
	if (!s->master.bus_ins) {
		fprintf(stderr, "Error initializing master bus\n");
		exit(1);
	}
	s->master.bus_ins[0] = b1;
	s->master.bus_ins[1] = b2;


	return (void *) s;
}

/* Update */

// for convenience reads bitfield
// TODO: could make macro if cleaner
static char is_key_pressed(struct key_input *input, int key) 
{return input->key_pressed & 0b1 << key ? 1 : 0;}
static char is_key_released(struct key_input *input, int key) 
{return input->key_released & 0b1 << key ? 1 : 0;}
static char is_key_down(struct key_input *input, int key) 
{return input->key_down & 0b1 << key ? 1 : 0;}

static void close_gate(struct sample* s)
{
	if (!s) return;
	// check if gate should release
	// probably need this playing check in case of a sample switch with no trigger (SHIFT)
	if (!(s->gate && s->playing && !s->gate_closed)) return;

	s->gate_release_cnt = 0.0;
	s->gate_closed = true;
	s->gate_close_gain = get_envelope_gain(s);
	// if sample is playing forward compare to release
	if (s->speed > 0) {
		s->gate_release = s->release;
		if (s->end_frame - s->next_frame < s->release)
			s->gate_release_cnt = s->release - (s->end_frame - s->next_frame);
		// if sample is playing backward compare to attack
	} else {
		s->gate_release = s->attack;
		if (s->next_frame - s->start_frame < s->attack)
			s->gate_release_cnt = s->release - (s->next_frame - s->start_frame);
	}
}

// compresses envelope times if active length is shrunk
static void squeeze_envelope(struct sample *s )
{
	if (s->end_frame - s->start_frame > s->attack + s->release) return;
	float r = (float) s->attack / (s->release + s->attack);
	s->attack = (s->end_frame - s->start_frame) * r;
	s->release = s->end_frame - s->start_frame - s->attack;
}

static inline void process_pad_press(struct sp_state *sp_state, struct key_input *input, int key, int pad)
{
	int alt = is_key_down(input, KEY_SHIFT_L) || is_key_down(input, KEY_SHIFT_R); 
	struct sampler *sampler = &sp_state->sampler;
	struct sample ***banks = sampler->banks;
	int curr_bank = sampler->curr_bank;

	// if pad is pressed then either trigger sample
	// store info for a move mode
	// or switch to the sample view without a trigger
	if (is_key_pressed(input, key)) {
		switch (sampler->move_mode) {
			case MOVE:
				if (!sampler->pad_src) {
					sampler->pad_src = banks[curr_bank] + pad;
				} else {
					// move sample pad from pad_src to this pad
					struct sample *tmp = banks[curr_bank][pad];
					banks[curr_bank][pad] = *sampler->pad_src;
					*sampler->pad_src = tmp;

					sampler->move_mode = NONE;
					sampler->pad_src = NULL;
				}
				break;
			case COPY:
				if (!sampler->pad_src) {
					sampler->pad_src = banks[curr_bank] + pad;
				} else {
					// copy sample pad from pad_src to this pad
					// if new pad is empty then allocate another sample 
					// otherwise free audio data of sample at dest pad
					struct sample **dest_pad = banks[curr_bank] + pad;
					if (!*dest_pad){
						*dest_pad = malloc(sizeof(struct sample));
						struct bus *new_bus = calloc(1, sizeof(struct bus));
						// attach new sample to bus
						new_bus->sample_in = *dest_pad;
						sp_state->master.bus_ins = realloc(sp_state->master.bus_ins, sizeof(struct bus *) * ++(sp_state->master.num_bus_ins));
						sp_state->master.bus_ins[sp_state->master.num_bus_ins - 1] = new_bus;
					} else {
						if ((*dest_pad)->data) free((*dest_pad)->data);
					}

					**dest_pad = **sampler->pad_src;
					if((*dest_pad)->playing) kill_sample(*dest_pad);
					// copy data
					int64_t data_size = sizeof(double) * (*dest_pad)->num_frames * NUM_CHANNELS;
					(*dest_pad)->data = malloc(data_size);
					memcpy((*dest_pad)->data, (*sampler->pad_src)->data, data_size);

					sampler->move_mode = NONE;
					sampler->pad_src = NULL;
				}
				break;

			case NONE:
				if (!alt && banks[curr_bank][pad]) 
					trigger_sample(banks[curr_bank][pad]);
		}

		sampler->active_sample = banks[curr_bank][pad];
		sampler->curr_pad = pad;
	} else if (is_key_released(input, key)){
		close_gate(banks[curr_bank][pad]);
	}
}

static void process_input_and_update(struct sp_state *sp_state, struct key_input *input)
{

	/* sampler updates */

	struct sampler *sampler = &(sp_state->sampler);
	struct sample ***banks = sampler->banks;
	struct sample **active_sample = &(sampler->active_sample);

	const char alt = is_key_down(input, KEY_SHIFT_L) || is_key_down(input, KEY_SHIFT_R); 


	// What to do on pad press
	// TODO: not all compilers will support binary literal, consider this
	// Q Pad
	process_pad_press(sp_state, input, KEY_Q, PAD_Q);
	// W Pad
	process_pad_press(sp_state, input, KEY_W, PAD_W);
	// E Pad
	process_pad_press(sp_state, input, KEY_E, PAD_E);
	// R Pad
	process_pad_press(sp_state, input, KEY_R, PAD_R);
	// A Pad
	process_pad_press(sp_state, input, KEY_A, PAD_A);
	// S Pad
	process_pad_press(sp_state, input, KEY_S, PAD_S);
	// D Pad
	process_pad_press(sp_state, input, KEY_D, PAD_D);
	// F Pad
	process_pad_press(sp_state, input, KEY_F, PAD_F);

	// Move sample
	if (is_key_pressed(input, KEY_M)) {
		sampler->move_mode = MOVE;
		// TODO should always be NULL, but setting just in case for now
		sampler->pad_src = NULL;
	}

	// Move sample
	if (is_key_pressed(input, KEY_C)) {
		sampler->move_mode = COPY;
		// TODO should always be NULL, but setting just in case for now
		sampler->pad_src = NULL;
	}

	// Kill all samples
	if (is_key_pressed(input, KEY_X)) {
		for (int b = 0; b < sampler->num_banks; b++) {
			for (int p = PAD_Q; p <= PAD_F; p++) {
				if (sampler->banks[b][p]) kill_sample(sampler->banks[b][p]);
			}
		}
	}

	// waveform viewer zoom
	if (is_key_pressed(input, KEY_EQUAL)) {
		if (sampler->zoom <= 1024) sampler->zoom *= 2;
	}
	if (is_key_pressed(input, KEY_MINUS)) {
		sampler->zoom /= 2;
		if (sampler->zoom < 1) sampler->zoom = 1;
	}
	// switch bank
	if (is_key_pressed(input, KEY_B)) {
		if (!alt && sampler->curr_bank + 1 < sampler->num_banks)
			sampler->curr_bank += 1;
		else if (sampler->curr_bank > 0 ) 
			sampler->curr_bank -= 1;
	}

	/* Active sample playback options */
	struct sample *s = *active_sample;
	if (!s) return;
	// playback speed / pitch
	if (is_key_pressed(input, KEY_O)) {
		const float st = speed_to_st(fabs(s->speed));
		float speed;
		if (alt) speed = st_to_speed(st - 1);
		else speed = st_to_speed(st + 1);

		static const float MAX_SPEED = 4.1f;
		if (speed > 0.01f && speed < MAX_SPEED) { 
			const char sign = s->speed > 0.0f ? 1 : -1;
			s->speed = sign * speed;
		}
	}
	// move start
	if (input->num_key_press[KEY_U]) {
		int32_t f; 
		int32_t inc = (float) input->num_key_press[KEY_U] * s->num_frames / sampler->zoom / 100;
		if (inc == 0) inc = 1;
		if (alt) f = s->start_frame - inc; 
		else f = s->start_frame + inc;

		if (f < 0) s->start_frame = 0;
		else if (f >= s->end_frame) s->start_frame = s->end_frame - 1;
		else s->start_frame = f;

		squeeze_envelope(s);
		if (!s->playing) kill_sample(s);
		sampler->zoom_focus = START;
	}
	// move end
	if (input->num_key_press[KEY_I]) {
		int32_t f; 
		int32_t inc = (float) input->num_key_press[KEY_I] * s->num_frames / sampler->zoom / 100;
		if (inc == 0) inc = 1;
		if (alt) f = s->end_frame - inc; 
		else f = s->end_frame + inc;

		if (f > s->num_frames) s->end_frame = s->num_frames;
		else if (f <= s->start_frame) s->end_frame = s->start_frame + 1;
		else s->end_frame = f;

		squeeze_envelope(s);
		if (!s->playing) kill_sample(s);
		sampler->zoom_focus = END;
	}

	// set envelope
	if (is_key_down(input, KEY_J)) {
		float ms = frames_to_ms(s->attack);
		if (alt) ms -= 2;
		else ms += 2;

		if (ms >= 0) { 
			const int32_t frames = ms_to_frames(ms);
			if (s->end_frame - s->start_frame - s->release >= frames) {
				s->attack = frames;
			}
		}
	}
	if (is_key_down(input, KEY_K)) {
		float ms = frames_to_ms(s->release);
		if (alt) ms -= 2; 
		else ms += 2;

		if (ms >= 0) {
			const int32_t frames = ms_to_frames(ms);
			if (s->end_frame - s->start_frame - s->attack >= frames) {
				s->release = frames;
			}
		}
	}

	// set playback modes
	if (is_key_pressed(input, KEY_G)) {
		s->gate = !s->gate;
	}
	if (is_key_pressed(input, KEY_V)) {
		s->reverse = !s->reverse;
		s->speed *= -1.0;
	}
	if (is_key_pressed(input, KEY_L)) {
		if (s->loop_mode == PING_PONG) s->loop_mode = LOOP_OFF;
		else s->loop_mode += 1;
	}
}

/* Rendering */

void sp_plus_update_and_render(
		void *sp_state, 
		char *pixel_buf, 
		int pixel_width,
		int pixel_height,
		int pixel_bytes,
		struct key_input* input)
{
	ASSERT(sp_state);
	ASSERT(pixel_buf);

	/* update state */
	process_input_and_update(sp_state, input);

	/* render ui */
	struct pixel_buffer buffer = { 
		pixel_buf, 
		pixel_bytes, 
		pixel_width, 
		pixel_height};

	clear_pixel_buffer(&buffer);
	// fill_pixel_buffer(&buffer, RED);

	draw_sampler(sp_state, &buffer);
}
