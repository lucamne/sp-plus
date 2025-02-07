#include "sp_plus.h"
#include "audio_types.h"
#include "render.h"
#include "smarc.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

// Wav paths for testing
#define WAV1 "../test/GREEN.wav"

// TODO: may want to move this eventually
// currently used in sp_plus_allocate_state and proccess_input_and_update
static const int NUM_PADS = 8;

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
void *sp_plus_allocate_state(void)
{
	struct sp_state *s = calloc(1, sizeof(struct sp_state));
	if (!s) return NULL;

	// initialize a sample bank with 8 samples
	s->sampler.zoom = 1;
	s->sampler.num_banks = 1;
	s->sampler.banks = calloc(1, sizeof(struct sample*));
	s->sampler.banks[0] = calloc(8, sizeof(struct sample));
	s->sampler.max_vert = 4000;

	// load whole file into a buffer for DEBUG
	void *buffer = NULL;
	const int64_t buf_bytes = platform_load_entire_file(&buffer, WAV1);

	if (buf_bytes) {
		if (!load_sample_from_wav_buffer(&(s->sampler.banks[0][0]), buffer, buf_bytes)) {
			s->master.sample_in = &(s->sampler.banks[0][0]);
		} else {
			fprintf(stderr, "File load failed: %s\n", WAV1);
		}

		platform_free_file_buffer(&buffer);
	}


	return (void *) s;
}

/* Update and Rendering */

// for convenience reads bitfield
// TODO: could make macro if cleaner
static char is_key_pressed(struct key_input *input, int key) 
{return input->key_pressed & 0b1 << key ? 1 : 0;}
static char is_key_released(struct key_input *input, int key) 
{return input->key_released & 0b1 << key ? 1 : 0;}
static char is_key_down(struct key_input *input, int key) 
{return input->key_down & 0b1 << key ? 1 : 0;}
// utility stuff
static float st_to_speed(const float st) { return powf(2.0f, st / 12.0f); }
static float speed_to_st(float speed) { return -12 * log2f(1.0f / speed); }
static int32_t ms_to_frames(const float m) { return m * SAMPLE_RATE / 1000.0f; }
static float frames_to_ms(const int32_t f) { return 1000.0f * f / SAMPLE_RATE; }

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

static void process_input_and_update(struct sp_state *sp_state, struct key_input *input)
{

	/* sampler updates */

	struct sampler *sampler = &(sp_state->sampler);
	struct sample **banks = sampler->banks;
	struct sample **active_sample = &(sampler->active_sample);
	int cur_bank = sampler->cur_bank;

	const char alt = is_key_down(input, KEY_SHIFT_L) || is_key_down(input, KEY_SHIFT_R); 

	/* sample triggering*/
	// TODO: not all compilers will support binary literal, consider this
	// Q Pad
	if (is_key_pressed(input, KEY_Q)) {
		if (!alt) 
			trigger_sample(&banks[cur_bank][PAD_Q]);
		*active_sample = &banks[cur_bank][PAD_Q];
	} else if (is_key_released(input, KEY_Q)){
		close_gate(&banks[cur_bank][PAD_Q]);
	}
	// W Pad
	if (is_key_pressed(input, KEY_W)) {
		if (!alt) 
			trigger_sample(&banks[cur_bank][PAD_W]);
		*active_sample = &banks[cur_bank][PAD_W];
	} else if (is_key_released(input, KEY_W)){
		close_gate(&banks[cur_bank][PAD_W]);
	}
	// E Pad
	if (is_key_pressed(input, KEY_E)) {
		if (!alt) 
			trigger_sample(&banks[cur_bank][PAD_E]);
		*active_sample = &banks[cur_bank][PAD_E];
	} else if (is_key_released(input, KEY_E)){
		close_gate(&banks[cur_bank][PAD_E]);
	}
	// R Pad
	if (is_key_pressed(input, KEY_R)) {
		if (!alt) 
			trigger_sample(&banks[cur_bank][PAD_R]);
		*active_sample = &banks[cur_bank][PAD_R];
	} else if (is_key_released(input, KEY_R)){
		close_gate(&banks[cur_bank][PAD_R]);
	}

	// Kill all samples
	if (is_key_pressed(input, KEY_X)) {
		for (int b = 0; b < sampler->num_banks; b++) {
			for (int p = 0; p < NUM_PADS; p++) {
				kill_sample(&banks[b][p]);
			}
		}
	}
	// TODO: Test once rendering is implemented
	// waveform viewer zoom
	if (is_key_pressed(input, KEY_EQUAL)) {
		if (sampler->zoom <= 256) sampler->zoom *= 2;
	}
	if (is_key_pressed(input, KEY_MINUS)) {
		sampler->zoom /= 2;
		if (sampler->zoom < 1) sampler->zoom = 1;
	}
	// switch bank
	if (is_key_pressed(input, KEY_B)) {
		if (!alt && sampler->cur_bank + 1 < sampler->num_banks)
			sampler->cur_bank += 1;
		else if (sampler->cur_bank > 0 ) 
			sampler->cur_bank -= 1;
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
	if (is_key_down(input, KEY_U)) {
		int32_t f; 
		int32_t inc = (float) s->num_frames / sampler->zoom / 100;
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
	if (is_key_down(input, KEY_I)) {
		int32_t f; 
		int32_t inc = (float) s->num_frames / sampler->zoom / 100;
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

/* Drawing */
static void draw_waveform(
		const struct sp_state *sp_state, 
		const struct pixel_buffer *buffer, 
		const vec2i wave_origin, const int width, 
		const int max_height)
{
	const struct sampler sampler = sp_state->sampler;
	const struct sample *s = sampler.active_sample;
	if (!s) return;

	// calculate zoom parameters
	const int32_t frames_to_draw = s->num_frames / sampler.zoom;
	const int32_t focused_frame = sampler.zoom_focus == END ? 
		s->end_frame : s->start_frame;

	int32_t first_frame_to_draw;
	if (focused_frame < frames_to_draw / 2)
		first_frame_to_draw = 0;
	else if (	s->num_frames - focused_frame <= 
			frames_to_draw - (frames_to_draw / 2))
		first_frame_to_draw = s->num_frames - frames_to_draw;
	else
		first_frame_to_draw = focused_frame - frames_to_draw / 2;

	// calculate vertices to render and frames per vertex
	int num_vertices;
	float frame_freq;
	if (frames_to_draw > sampler.max_vert) { 
		num_vertices = sampler.max_vert;
		frame_freq = (float) frames_to_draw / (float) sampler.max_vert;
	} else {
		num_vertices = frames_to_draw;
		frame_freq = 1.0f;
	}
	if (num_vertices < 2) return;
	const float vertex_spacing = width / (float) (num_vertices);

	// draw wave lines
	{
		int32_t frame = first_frame_to_draw; 
		double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		int y = roundf((float) sum * (max_height / 2.0f) + wave_origin.y);
		int x = wave_origin.x;
		vec2i last_vertex = {x, y};

		for (int i = 1; i < num_vertices; i++) {
			frame = first_frame_to_draw + i * (int) frame_freq;
			sum = (s->data[frame * NUM_CHANNELS] + s->data[frame * NUM_CHANNELS + 1]) / 2.0;

			y = roundf((float) sum * (max_height / 2.0f) + wave_origin.y);
			x = roundf((float) i * vertex_spacing + wave_origin.x);

			const vec2i curr_vertex = {x, y};
			draw_line(buffer, last_vertex, curr_vertex, WHITE);
			last_vertex = curr_vertex;
		}
	}


	// draw markers
	if (	s->next_frame >= first_frame_to_draw && 
			s->next_frame < first_frame_to_draw + frames_to_draw) {

		const int play_x = 
			roundf(((int) (s->next_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x);
		const vec2i startv = {play_x, roundf(wave_origin.y - max_height / 2.0f)};
		const vec2i endv = {play_x, roundf(wave_origin.y + max_height / 2.0f)};
		draw_line(buffer, startv, endv, RED);
	}
	if (	s->start_frame >= first_frame_to_draw && 
			s->start_frame < first_frame_to_draw + frames_to_draw) {

		const int start_x = 
			roundf(((int) (s->start_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x);
		const vec2i startv = {start_x, roundf(wave_origin.y - max_height / 2.0f)};
		const vec2i endv = {start_x, roundf(wave_origin.y + max_height / 2.0f)};
		draw_line(buffer, startv, endv, GREEN);
	}
	if (	s->end_frame >= first_frame_to_draw && 
			s->end_frame <= first_frame_to_draw + frames_to_draw) {

		const int end_x = 
			roundf(((int) (s->end_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x);
		const vec2i startv = {end_x, roundf(wave_origin.y - max_height / 2.0f)};
		const vec2i endv = {end_x, roundf(wave_origin.y + max_height / 2.0f)};
		draw_line(buffer, startv, endv, GREEN);
	}
}

static void draw_sampler(const struct sp_state *sp_state, const struct pixel_buffer *buffer)
{
	// sets position of sampler on screen
	const vec2i origin = {0, 0};

	const int BORDER_W = 800;
	const int BORDER_H = 450;
	const int VIEWER_W = BORDER_W * 3 / 4;
	const int VIEWER_H = BORDER_H * 2 / 3;
	const vec2i WAVE_ORIGIN = {origin.x + 10, origin.y + VIEWER_H / 2};

	// border pane
	draw_rec_outline(buffer, origin, BORDER_W, BORDER_H, WHITE);
	// waveform viewer pane
	draw_rec_outline(buffer, origin, VIEWER_W, VIEWER_H, WHITE);

	// draw waveform
	draw_waveform(sp_state, buffer, WAVE_ORIGIN, VIEWER_W - 20, VIEWER_H - 20);
}

void sp_plus_update_and_render(
		void *sp_state, 
		char *pixel_buf, 
		int pixel_width,
		int pixel_height,
		int pixel_bytes,
		struct key_input* input)
{
	process_input_and_update(sp_state, input);

	struct pixel_buffer buffer = { pixel_buf, pixel_bytes, pixel_width, pixel_height };
	clear_pixel_buffer(&buffer);

	draw_sampler(sp_state, &buffer);
}
