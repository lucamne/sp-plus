// internal
#include "sp_plus.h"
#include "sp_types.h"
#include "sp_plus_assert.h"

// external
#define STB_TRUETYPE_IMPLEMENTATION
#include "stb_truetype.h"

// libc
#include <math.h>
#include <string.h>
#include <stdlib.h>

// paths for testing
#define WAV1 "../wavs/BillieJean.wav"
#define FONT1 "../fonts/DejaVuSans-Bold.ttf"
#define WORKING_DIR "../wavs/"

// TODO: may want to move these eventually
// currently used in sp_plus_allocate_state and proccess_input_and_update
static const int NUM_PADS = 8;

// utility stuff used by draw_ui.c and update
static float st_to_speed(const float st) { return powf(2.0f, st / 12.0f); }
static float speed_to_st(float speed) { return -12 * log2f(1.0f / speed); }
static int32_t ms_to_frames(const float m) { return m * SAMPLE_RATE / 1000.0f; }
static float frames_to_ms(const int32_t f) { return 1000.0f * f / SAMPLE_RATE; }

// .c includes
#include "sp_draw_ui.c"
#include "sp_update.c"

///////////////////////////////////////////////////////////////////////////////
/// Audio playback
///
/// Platform calls these functions through fill_audio_buffer every audio frame

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

// TODO could use this instead of double out[] in other process functions
struct frame_data {
	double l;
	double r;
};

// get next frame and apply envelope gain
static struct frame_data process_next_frame(/*double out[], */struct sample* s)
{
	struct frame_data out = {0};
	if (!s) return out;
	// need to interpolate fractional next_frame
	const int32_t base_frame  = s->next_frame;
	const double ratio = s->next_frame - base_frame;
	
	// left
	out.l = s->data[base_frame * NUM_CHANNELS] * (1 - ratio);
	out.l += s->data[(base_frame + 1) * NUM_CHANNELS] * ratio;
	if (s->gate_closed) {
		out.l *= (s->gate_release - s->gate_release_cnt) /
			s->gate_release;
		out.l *= s->gate_close_gain;
	}
	out.l *= get_envelope_gain(s);

	// right
	out.r = s->data[base_frame * NUM_CHANNELS + 1] * (1 - ratio);
	out.r += s->data[(base_frame + 1) * NUM_CHANNELS + 1] * ratio;
	if (s->gate_closed) {
		out.r *= (s->gate_release - s->gate_release_cnt) /
			s->gate_release;
		out.r *= s->gate_close_gain;
	}
	out.r *= get_envelope_gain(s);

	increment_frame(s);
	return out;
}

// TODO: can probably make this stuff wayyyyy move efficient
// Recurse from master bus down to samples. Get next frame data from samples
// apply bus dsp to output. Each bus will have an array of bus inputs or
// a single sample input.
static struct frame_data process_leaf_nodes(struct bus* b)
{
	struct frame_data out = {0};
	// process sample
	if (b->sample_in) {
		if (b->sample_in->playing) {
			out = process_next_frame(b->sample_in);
		}
	// process bus inputs
	} else {
		for (int i = 0; i < b->num_bus_ins; i++) {
			struct frame_data tmp = process_leaf_nodes(b->bus_ins[i]);
			out.l += tmp.l;
			out.r += tmp.r;
		}
	}

	// apply bus dsp
	out.l *= (1.0 - b->atten);
	out.r *= (1.0 - b->atten);
	out.l *= fmin(1.0 - b->pan, 1.0);
	out.r *= fmin(1.0 + b->pan, 1.0);

	return out;
}

// called by platform in async callback
// relies on other audio playback functions
int sp_plus_fill_audio_buffer(void *sp_state, void* buffer, int frames)
{
	int err;
	err = platform_mutex_lock(((struct sp_state *) sp_state)->mixer.master_mutex);
	ASSERT(!err);

	// TODO use reference instead of copy maybe?
	struct bus *master = &((struct sp_state *) sp_state)->mixer.master;
	// process i frames
	for (int i = 0; i < frames; i++){
		struct frame_data out = process_leaf_nodes(master);
		// alsa expects 16 bit int
		int16_t int_out[NUM_CHANNELS];
		// convert left
		double f = out.l * 32768.0; 
		if (f > 32767.0) f = 32767.0;
		if (f < -32768.0) f = -32768.0;
		int_out[0] = (int16_t) f;
		// convert right
		f = out.r * 32768.0; 
		if (f > 32767.0) f = 32767.0;
		if (f < -32768.0) f = -32768.0;
		int_out[1] = (int16_t) f;

		memcpy((char *)buffer + i * sizeof(int_out), int_out, sizeof(int_out));
	}

	err = platform_mutex_unlock(((struct sp_state *) sp_state)->mixer.master_mutex);
	ASSERT(!err);
}

//////////////////////////////////////////////////////////////////////////////////////////
/// Initialization
///
/// Platform calls this function at program start

// state allocation service called by platform
// use this function for initializing debugging data
// TODO consider moving initialization code
void *sp_plus_allocate_state(void)
{
	struct sp_state *s = calloc(1, sizeof(struct sp_state));
	if (!s) return NULL;

	// init mixer
	s->mixer.master.label = malloc(strlen("master") + 1);
	strcpy(s->mixer.master.label, "master");
	s->mixer.master.type = MASTER;

	s->mixer.master_mutex = platform_init_mutex();
	if (!s->mixer.master_mutex) {
		fprintf(stderr, "Error allocating state memory\n");
		exit(1);
	}

	s->mixer.bus_list = malloc(sizeof(struct bus *));
	*s->mixer.bus_list = &s->mixer.master;
	s->mixer.num_bus = 1;

	s->mixer.next_label = 1;

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

	// init shell
	s->shell.input_size = 64;
	s->shell.input_buff = malloc(s->shell.input_size);
	shell_print("SAMPLER >", s);

	// initialize fonts
	void *ttf_buffer = NULL;
	platform_load_entire_file(&ttf_buffer, FONT1);
	load_font(&(s->fonts[MED]), ttf_buffer, 20);
	platform_free_file_buffer(&ttf_buffer);


	// DEBUG CODE

	/*
	// Load in a wav file
	// TODO may want to create a sample load function that handles all of this
	struct sampler *sampler = &(s->sampler);
	struct sample *new_samp = load_sample_from_wav(WAV1);

	if (new_samp) {
	sampler->banks[0][PAD_Q] = new_samp;
	}

	struct bus *b1 = calloc(sizeof(struct bus), 0);
	if (!b1) {
	fprintf(stderr, "Error allocating state memory\n");
	exit(1);
	}

	b1->sample_in = s->sampler.banks[0][PAD_Q];

	s->master.num_bus_ins = 1;
	s->master.bus_ins = malloc(sizeof(struct bus *) * 1);
	if (!s->master.bus_ins) {
	fprintf(stderr, "Error initializing master bus\n");
	exit(1);
	}
	s->master.bus_ins[0] = b1;
	*/

	// set working directory and retrieve files
	load_directory_to_browser(&s->file_browser, WORKING_DIR);


	// pass state to platform
	return (void *) s;
}

//////////////////////////////////////////////////////////////////////////////////////////
/// Service Entry Point
///
/// Platform calls this function every frame

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

	struct sp_state *sp = (struct sp_state *) sp_state;

	/// Update State

	// change control mode
	if (is_key_pressed(input, KEY_TAB)) {
		if (++(sp->control_mode) > FILE_BROWSER)
			sp->control_mode = 0;

		switch (sp->control_mode) {
			case FILE_BROWSER:
				shell_print("FILE BROWSER >", sp);
				break;
			case MIXER:
				shell_print("MIXER >", sp);
				break;
			case SHELL:
				shell_print(">", sp);
				break;
			case SAMPLER:
			default:
				shell_print("SAMPLER >", sp);
		}
	}

	switch (sp->control_mode) {
		case FILE_BROWSER:
			update_file_browser(sp, input);
			break;
		case MIXER:
			update_mixer(sp, input);
			break;
		case SHELL:
			update_shell(sp, input);
			break;
		case SAMPLER:
		default:
			update_sampler(sp, input);
	}

	/// Draw UI
	struct pixel_buffer buffer = { 
		pixel_buf, 
		pixel_bytes, 
		pixel_width, 
		pixel_height};

	clear_pixel_buffer(&buffer);

	draw_sampler(sp, &buffer);
	draw_file_browser(sp, &buffer);
	draw_mixer(sp, &buffer);
	draw_shell(sp, &buffer);
}
