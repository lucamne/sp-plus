#include "smarc.h"

// for convenience reads input bitfield
// TODO: could make macro if cleaner
static char is_key_pressed(struct key_input *input, int key) 
{return input->key_pressed & 1ULL << key ? 1 : 0;}
static char is_key_released(struct key_input *input, int key) 
{return input->key_released & 1ULL << key ? 1 : 0;}
static char is_key_down(struct key_input *input, int key) 
{return input->key_down & 1ULL << key ? 1 : 0;}

/////////////////////////////////////////////////////////////////////////////////////////
/// Sampler Update

// compresses envelope times if active length is shrunk
static inline void squeeze_envelope(struct sample *s )
{
	if (s->end_frame - s->start_frame > s->attack + s->release) return;
	float r = (float) s->attack / (s->release + s->attack);
	s->attack = (s->end_frame - s->start_frame) * r;
	s->release = s->end_frame - s->start_frame - s->attack;
}

static inline double get_envelope_gain(struct sample* s)
{
	double g = 1.0;
	if (s->attack && s->next_frame - s->start_frame < s->attack) {
		g = (s->next_frame - s->start_frame) / s->attack;
	} else if (s->release && s->end_frame - s->next_frame <= s->release) {
		g = (s->end_frame - s->next_frame) / s->release;
	}
	return g;
}

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

static inline void trigger_sample(struct sample* s)
{
	if (!s->reverse) s->next_frame = s->start_frame;
	else s->next_frame = s->end_frame - 1.0;

	if (s->loop_mode && s->playing) s->playing = false;
	else s->playing = true;

	s->gate_closed = false;
}

static inline int kill_sample(struct sample* s)
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

// find bus with sample s and replace with null
static inline void detach_sample_from_mixer(struct sample *s, struct sp_state *sp_state)
{
	ASSERT(s && sp_state);

	int err;
	err = platform_mutex_lock(sp_state->mixer.master_mutex);
	ASSERT(!err);

	struct bus *b = s->output_bus;
	if (b) {
		if (b->sample_in == s) {
			b->sample_in = NULL;
		}
	}

	err = platform_mutex_unlock(sp_state->mixer.master_mutex);
	ASSERT(!err);
}

// replace sample_in of bus b with s
static inline void attach_sample_to_bus(struct sample *s, struct bus *b, struct sp_state *sp_state)
{
	ASSERT(s && b && sp_state);

	struct mixer *mixer = &(sp_state->mixer);
	int err;
	err = platform_mutex_lock(mixer->master_mutex);
	ASSERT(!err);

	// attach bus to tree
	s->output_bus = b;
	b->sample_in = s;

	// TODO multi sample_in code can be deleted eventually
	/*
	   b->sample_ins = realloc(b->sample_ins, sizeof(struct sample *) * ++(b->num_sample_ins));
	   b->sample_ins[b->num_sample_ins - 1] = s;
	   */

	err = platform_mutex_unlock(mixer->master_mutex);
	ASSERT(!err);
}

// attachs child bus to parent bus in mixing tree
static void attach_bus(struct bus *child, struct bus *parent, const struct sp_state *sp_state)
{
	int err = platform_mutex_lock(sp_state->mixer.master_mutex);
	ASSERT(!err);

	parent->bus_ins = realloc(parent->bus_ins, sizeof(struct bus *) * ++(parent->num_bus_ins));
	parent->bus_ins[parent->num_bus_ins - 1] = child;
	child->output_bus = parent;

	err = platform_mutex_unlock(sp_state->mixer.master_mutex);
	ASSERT(!err);
}

// detach bus from mixing tree and remove from bus list
static void detach_bus_from_mixer(struct bus *b, struct sp_state *sp_state)
{
	int err = platform_mutex_lock(sp_state->mixer.master_mutex);
	ASSERT(!err);

	struct bus *parent = b->output_bus;
	for (int i = 0; i < parent->num_bus_ins; i++) {
		if (parent->bus_ins[i] == b) {
			parent->bus_ins[i] = parent->bus_ins[parent->num_bus_ins - 1];
			break;
		}
	}
	parent->bus_ins = realloc(parent->bus_ins, sizeof(struct bus *) * --(parent->num_bus_ins));
	b->output_bus = NULL;


	err = platform_mutex_unlock(sp_state->mixer.master_mutex);
	ASSERT(!err);
}

// allocates and inits a new bus structure
static struct bus *init_bus(struct sp_state *sp_state)
{
	struct bus *new_bus = calloc(1, sizeof(*new_bus));
	if (!new_bus) return NULL;

	// setup bus label
	int MAX_LABEL_LEN = 10;
	new_bus->label = malloc(MAX_LABEL_LEN);
	snprintf(new_bus->label, MAX_LABEL_LEN, "bus %d", sp_state->mixer.next_label++);

	// add bus to bus list
	sp_state->mixer.bus_list = realloc(sp_state->mixer.bus_list, 
			sizeof(struct bus *) * ++(sp_state->mixer.num_bus));
	sp_state->mixer.bus_list[sp_state->mixer.num_bus - 1] = new_bus;

	return new_bus;
}

// frees a bus and removes it from bus list
static void free_bus(struct bus *b, struct sp_state *sp_state)
{
	ASSERT(b && sp_state);
	struct mixer *m = &sp_state->mixer;

	for (int i = 0; i < m->num_bus; i++) {
		// remove b from bus_list
		if (m->bus_list[i] == b) 
			m->bus_list[i] = m->bus_list[m->num_bus - 1];
		// find child busses and set their bus output to NULL
		if (m->bus_list[i]->output_bus == b)
			m->bus_list[i]->output_bus = NULL;
	}
	m->bus_list = realloc(m->bus_list, sizeof(struct bus *) * --(m->num_bus));

	if (b->label) free(b->label);
	if (b->bus_ins) free(b->bus_ins);
	free(b);
}

// unloads sample from sampler and removes sample bus
static inline void unload_sample(struct sample *s, struct sp_state *sp_state)
{
	ASSERT(s);
	detach_sample_from_mixer(s, sp_state);

	// detach output_bus
	struct bus *out_bus = s->output_bus;
	detach_bus_from_mixer(out_bus, sp_state);

	// free output bus
	free_bus(out_bus, sp_state);

	// free sample
	if (s->name) free(s->name);
	if (s->data) free(s->data);
	free(s);
}

static void process_pad_press(struct sp_state *sp_state, struct key_input *input, int key, int pad)
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
			case SWAP:
				if (!sampler->pad_src) {
					sampler->pad_src = banks[curr_bank] + pad;
					sampler->pad_src_bank = curr_bank;
					sampler->pad_src_pad = pad;
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
					sampler->pad_src_bank = curr_bank;
					sampler->pad_src_pad = pad;
				} else {
					// if src pad is occupied
					// copy sample pad from pad_src to this pad
					// if new pad is empty then allocate another sample 
					// otherwise free audio data of sample at dest pad
					if (*sampler->pad_src) {
						struct sample **dest_pad = banks[curr_bank] + pad;

						// copy pad_src to new sample
						struct sample *new_samp = malloc(sizeof(*new_samp));
						*new_samp = **sampler->pad_src;

						// copy name
						new_samp->name = malloc(strlen((*sampler->pad_src)->name) + 1);
						strcpy(new_samp->name, (*sampler->pad_src)->name);

						// copy data
						int64_t data_size = sizeof(double) * new_samp->num_frames * NUM_CHANNELS;
						new_samp->data = malloc(data_size);
						memcpy(new_samp->data, (*sampler->pad_src)->data, data_size);

						// copied should start not playing
						if(new_samp->playing) kill_sample(new_samp);

						// create bus for new sample and copy some data from src_bus
						struct bus *src_bus = (*sampler->pad_src)->output_bus;
						struct bus *new_bus = init_bus(sp_state);
						new_bus->pan = src_bus->pan;
						new_bus->atten = src_bus->atten;
						new_bus->type = SAMPLE;

						// attach new_bus and new_samp
						attach_bus(new_bus, src_bus->output_bus, sp_state);
						attach_sample_to_bus(new_samp, new_bus, sp_state);

						// if dest_pad is occupied detach the sample from mixer and free the sample
						if (*dest_pad) unload_sample(*dest_pad, sp_state);

						// assign to pad
						*dest_pad = new_samp;
					}

					sampler->move_mode = NONE;
					sampler->pad_src = NULL;
				}
				break;

			case NONE:
			default:
				if (!alt && banks[curr_bank][pad]) 
					trigger_sample(banks[curr_bank][pad]);
		}

		sampler->active_sample = banks[curr_bank][pad];
		sampler->curr_pad = pad;
	} else if (is_key_released(input, key)){
		close_gate(banks[curr_bank][pad]);
	}
}

static void update_sampler(struct sp_state *sp_state, struct key_input *input)
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
		sampler->move_mode = SWAP;
		// TODO should always be NULL, but setting just in case for now
		sampler->pad_src = NULL;
	}

	// copy sample
	if (is_key_pressed(input, KEY_C)) {
		sampler->move_mode = COPY;
		// TODO should always be NULL, but setting just in case for now
		sampler->pad_src = NULL;
	}

	// cancel copy/move
	if (is_key_pressed(input, KEY_ESCAPE)) {
		sampler->move_mode = NONE;
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

	// kill active sample
	if (is_key_pressed(input, KEY_Z)) {
		kill_sample(s);
	}

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

//////////////////////////////////////////////////////////////////////////////////////
/// File Browser Update

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

// check endianess of system at runtime
static bool is_little_endian(void)
{
	int n = 1;
	if(*(char *)&n == 1) 
		return true;
	return false;
}

static inline void invalid_wav_file(void *buffer, struct sample *s, const char *path) 
{
	fprintf(stderr, "Unsupported file: %s\n", path);
	platform_free_file_buffer(&buffer);
	free(s);
}

// loads sample from a buffer in wav format
// currently supports only non compressed WAV files
// sample should be initialized before being passed to load_wav()
// returns 0 iff success
static struct sample *load_sample_from_wav(const char *path)
{
	// TODO support big_endian systems as well
	if (!is_little_endian) {
		fprintf(stderr, "Only little-endian systems are supported\n");
		return NULL;
	}

	// load file
	void *file_buffer = NULL;
	long buf_bytes = platform_load_entire_file(&file_buffer, path);
	if (!buf_bytes) {
		fprintf(stderr, "Failed to open: %s\n", path);
		return NULL;
	}
	char *buffer = (char *) file_buffer;


	// init sample
	struct sample *new_samp = calloc(1, sizeof(struct sample));
	if (!new_samp) {
		fprintf(stderr, "Error allocating sample");
		platform_free_file_buffer(file_buffer);
	}

	new_samp->speed = 1.0;	

	// extract file name
	int name_start = 0;
	for (int i = 0; i < (int) strlen(path) - 1; i++)
	{
		// TODO this might not work for windows be careful when porting
		if (path[i] == '/') name_start = i + 1;
	}

	new_samp->name = malloc(strlen(path) - name_start + 1);
	if (new_samp->name) strcpy(new_samp->name, path + name_start);


	// any read should not read the end_ptr or anything past it
	// data is stored in 4 byte words
	const char *end_ptr = buffer + buf_bytes;
	const int word = 4;

	/* parse headers */
	// check for 'RIFF' tag bytes [0, 3]
	if (*(int32_t *) buffer ^ 0x46464952) {
		invalid_wav_file(file_buffer, new_samp, path);
		return NULL;
	}

	// get master chunk_size 
	buffer += word;
	const int64_t mstr_ck_size = *(int32_t *) buffer;
	// verify we won't access data outside buffer
	// TODO more bounds checking should be added in case wav file is not properly formatted
	if (buffer + word + mstr_ck_size > end_ptr) {
		invalid_wav_file(file_buffer, new_samp, path);
		return NULL;
	}


	// check for 'WAVE' tag
	// TODO spec does not require wave tag here
	buffer += word;
	if (*(int32_t *) buffer ^ 0x45564157) {
		invalid_wav_file(file_buffer, new_samp, path);
		return NULL;
	}


	// check for 'fmt ' tag
	buffer += word;
	if (*(int32_t *) buffer ^ 0x20746D66) {
		invalid_wav_file(file_buffer, new_samp, path);
		return NULL;
	}


	// support only non-extended PCM for now
	buffer += word;
	const int32_t fmt_ck_size = *buffer;
	if (fmt_ck_size != 16) {
		invalid_wav_file(file_buffer, new_samp, path);
		return NULL;
	}


	/* parse 'fmt ' chunk */
	// check PCM format and number of channels
	buffer += word;
	if ((*(int32_t *) buffer & 0xFFFF) != 1) {
		invalid_wav_file(file_buffer, new_samp, path);
		return NULL;
	}

	const int num_channels = *(int32_t *) buffer >> 16;
	if (num_channels != 1 && num_channels != 2) {
		fprintf(stderr, "%d channel(s) not supported\n", num_channels);
		invalid_wav_file(file_buffer, new_samp, path);
		return NULL;
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
		if (buffer + word > end_ptr) {
			invalid_wav_file(file_buffer, new_samp, path);
			return NULL;
		}

		const int s = *(int32_t *) buffer;
		buffer += word + s;
		if (buffer + word > end_ptr) {
			invalid_wav_file(file_buffer, new_samp, path);
			return NULL;
		}

	}

	buffer += word;
	const int32_t data_size = *(int32_t *) buffer;
	const int32_t num_frames = data_size / frame_size;

	/* register sample info and pcm data */
	// register some fields
	new_samp->frame_size = frame_size;
	new_samp->num_frames = num_frames;
	new_samp->end_frame = num_frames;
	new_samp->rate = sample_rate;

	// allocate sample memory
	new_samp->data = malloc(num_frames * NUM_CHANNELS * sizeof(double));
	if (!new_samp->data) {
		fprintf(stderr, "Sample memory allocation error\n");
		platform_free_file_buffer(&file_buffer);
		free(new_samp);
		return NULL;
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
			r = l;
		} else {
			l = ((double) ((int16_t *) buffer)[NUM_CHANNELS * i]) / 32768.0;

			r = ((double) ((int16_t *) buffer)[NUM_CHANNELS * i + 1]) / 32768.0;
		}
		// bounds checking
		if (l > 1.0) l = 1.0;
		else if (l < -1.0) l = -1.0;
		if (r > 1.0) r = 1.0;
		else if (r < -1.0) r = -1.0;

		new_samp->data[NUM_CHANNELS * i] = l;
		new_samp->data[NUM_CHANNELS * i + 1] = r;
	}

	// resample to SAMPLE_RATE constant if necessary
	if (new_samp->rate != SAMPLE_RATE) {
		const int r = resample(new_samp, new_samp->rate, SAMPLE_RATE);
		if (r == -1) {
			fprintf(stderr, "Resampling Error\n");
			platform_free_file_buffer(&file_buffer);
			free(new_samp);
			free(new_samp->data);
			return NULL;
		}
		new_samp->num_frames = r;
		new_samp->end_frame = r;
		new_samp->rate = SAMPLE_RATE;
	}

	// clean up
	platform_free_file_buffer(&file_buffer);
	print_sample(new_samp);
	return new_samp;
}

static int load_directory_to_browser(struct file_browser *fb, const char *dir)
{
	if (fb->dir) free(fb->dir);

	// TODO See about removing the need for this check eventually
	fb->dir = platform_get_realpath(dir);
	if (!fb->dir) {
		fprintf(stderr, "Error allocating file browser memory\n");
		return -1;
	}

	SP_DIR *dir_handle = platform_opendir(fb->dir);
	if(dir) {
		fb->selected_file = 0;
		fb->num_files = platform_num_valid_items_in_dir(dir_handle);
		fb->files = realloc(fb->files, sizeof(struct file_item) * fb->num_files);
		if (fb->num_files && !fb->files) {
			fprintf(stderr, "Error allocating file browser memory\n");
			fb->num_files = 0;
			free(fb->dir);
			return -1;
		}

		for (int i = 0; i < fb->num_files; i++) {
			if (platform_read_next_valid_item(dir_handle, &fb->files[i].name, &fb->files[i].is_dir))
				break;
		}

		if (platform_closedir(dir_handle)) {
			fprintf(stderr, "Error closing directory\n");
		}
	} else {
		fprintf(stderr, "Error opening directory: %s", fb->dir);
		return -1;
	}
}

static void load_sample_from_browser(struct sp_state *sp_state, int pad)
{
	struct file_browser *fb = &sp_state->file_browser;
	struct sampler *sampler = &(sp_state->sampler);

	const char *file = fb->files[fb->selected_file].name;
	const char *dir = fb->dir;

	// +2 to add '/' charactor and null temination character
	char *path = malloc(sizeof(char) * (strlen(file) + strlen(dir) + 2));
	if (!path) {
		fprintf(stderr, "Error loading file\n");
		return;
	}
	strcpy(path, dir);
	strcat(path, "/");
	strcat(path, file);

	struct sample *new_samp = load_sample_from_wav(path);
	free(path);

	if (new_samp) {
		struct sample **dest_pad = sampler->banks[sampler->curr_bank] + pad;

		// if target pad is occupied delete that sample
		if (*dest_pad) unload_sample(*dest_pad, sp_state);

		// create new bus and attach to master
		struct bus *new_bus = init_bus(sp_state);
		if (new_bus) {
			new_bus->label = malloc(strlen(new_samp->name) + 1);
			strcpy(new_bus->label, new_samp->name);
			new_bus->type = SAMPLE;
			attach_bus(new_bus, &sp_state->mixer.master, sp_state);

			// assign new sample to pad and attach to bus
			*dest_pad = new_samp;
			attach_sample_to_bus(*dest_pad, new_bus, sp_state);

			// set current sampler paramaters
			sp_state->sampler.active_sample = new_samp;
			sp_state->sampler.curr_pad = pad;
		} else {
			fprintf(stderr, "Error initializing bus\n");
		}

	}
	fb->loading_to_pad = 0;
}

static void update_file_browser(struct sp_state *sp_state, struct key_input *input)
{
	struct file_browser *fb = &sp_state->file_browser;
	const char alt = is_key_down(input, KEY_SHIFT_L) || is_key_down(input, KEY_SHIFT_R); 

	// If in file load process then poll for pad destination
	// This code modifies sampler state to switch banks and choose a pad
	// rather than doing some complicated mode switching
	if (fb->loading_to_pad) {
		struct sampler *sampler = &(sp_state->sampler);

		// switch bank
		if (is_key_pressed(input, KEY_B)) {
			if (!alt && sampler->curr_bank + 1 < sampler->num_banks)
				sampler->curr_bank += 1;
			else if (sampler->curr_bank > 0 ) 
				sampler->curr_bank -= 1;
		}

		// if a pad is pressed try and load sample
		if (is_key_pressed(input, KEY_Q)) load_sample_from_browser(sp_state, PAD_Q);
		else if (is_key_pressed(input, KEY_W)) load_sample_from_browser(sp_state, PAD_W);
		else if (is_key_pressed(input, KEY_E)) load_sample_from_browser(sp_state, PAD_E);
		else if (is_key_pressed(input, KEY_R)) load_sample_from_browser(sp_state, PAD_R);
		else if (is_key_pressed(input, KEY_A)) load_sample_from_browser(sp_state, PAD_A);
		else if (is_key_pressed(input, KEY_S)) load_sample_from_browser(sp_state, PAD_S);
		else if (is_key_pressed(input, KEY_D)) load_sample_from_browser(sp_state, PAD_D);
		else if (is_key_pressed(input, KEY_F)) load_sample_from_browser(sp_state, PAD_F);

		else if (is_key_pressed(input, KEY_ESCAPE)) fb->loading_to_pad = 0;

	} else {

		// scroll down through file list
		if (input->num_key_press[KEY_DOWN]) {
			if (fb->selected_file < fb->num_files - 1) fb->selected_file++;
		}

		// scroll up through file list
		if (input->num_key_press[KEY_UP]) {
			if (fb->selected_file > 0) fb->selected_file--;
		}

		// load wav or enter directory
		if (is_key_pressed(input, KEY_RIGHT) && fb->num_files) {

			if (fb->files[fb->selected_file].is_dir) {
				// if selected file is a directory

				// +2 to make space for null-terminating char and '/' after directory name
				int dir_len = strlen(fb->dir) + strlen(fb->files[fb->selected_file].name) + 2;
				char *dir = malloc(sizeof(char) * dir_len);

				if (dir) {
					// assemble dir string
					strcpy(dir, fb->dir);
					strcat(dir, "/");
					strcat(dir, fb->files[fb->selected_file].name);
					load_directory_to_browser(fb, dir);
					free(dir);
				}
			} else {
				// otherwise attempt to load wav file
				fb->loading_to_pad = 1;
			}
		}

		// back out to parent directory
		if (is_key_pressed(input, KEY_LEFT)) {
			char *parent_dir = platform_get_parent_dir(fb->dir);
			load_directory_to_browser(fb, parent_dir);
			free(parent_dir);
		}
	}

}


////////////////////////////////////////////////////////////////////////////////
/// MIXER

static void update_mixer(struct sp_state *sp_state, struct key_input *input)
{
	struct mixer *mixer = &sp_state->mixer;
	const char alt = is_key_down(input, KEY_SHIFT_L) || is_key_down(input, KEY_SHIFT_R); 

	switch (mixer->update_mode) {
		case DELETE:
			{
				// delete bus if y is pressed 
				// cancel if another key is pressed
				if (is_key_pressed(input, KEY_Y)) {
					struct bus *b = mixer->bus_list[mixer->selected_bus];
					detach_bus_from_mixer(b, sp_state);
					free_bus(b, sp_state);
					mixer->update_mode = NORMAL;

				} else if (input->key_pressed){
					mixer->update_mode = NORMAL;
				}

				/*
				struct shell *shell = &sp_state->shell;
				// if a y was entered
				if (shell->input_size == 0 || (shell->input == 1 && (shell->input[0] == 'y' || shell->input[1] == 'Y'))) {
					struct bus *b = mixer->bus_list[mixer->selected_bus];
					detach_bus_from_mixer(b, sp_state);
					free_bus(b, sp_state);
					mixer->update_mode = NORMAL;
				} else {
					mixer->update_mode = NORMAL;
				}
				*/
			} break;

		case RENAME:
			{

				// get letter input 
				for (int i = KEY_A; i <= KEY_Z; i++) {
					for (int c = input->num_key_press[i]; c > 0; c--) {
						// save one byte from terminating char
						if (mixer->r_buff_pos < R_BUFF_MAX - 1) {
							if (alt) {
								mixer->r_buff[mixer->r_buff_pos++] = 'A' + i;
							} else {
								mixer->r_buff[mixer->r_buff_pos++] = 'a' + i;
							}
						}
					}
				}

				//TODO get number input

				// check for enter
				if (is_key_pressed(input, KEY_ENTER)) {
					mixer->r_buff[mixer->r_buff_pos] = '\0';

					// TODO not sure this is correct

					free(mixer->bus_list[mixer->selected_bus]->label);
					mixer->bus_list[mixer->selected_bus]->label = mixer->r_buff;

					mixer->r_buff = NULL;
					mixer->update_mode = NORMAL;
				}
				// check for escape
				else if (is_key_pressed(input, KEY_ESCAPE)) {
					free(mixer->r_buff);
					mixer->update_mode = NORMAL;
				}

			} break;

		case NORMAL:
		default:
			{
				// scroll down through file list
				if (input->num_key_press[KEY_DOWN]) {
					if (mixer->selected_bus < mixer->num_bus - 1) mixer->selected_bus++;
				}

				// scroll up through file list
				if (input->num_key_press[KEY_UP]) {
					if (mixer->selected_bus > 0) mixer->selected_bus--;
				}

				// atten (level)
				if (input->num_key_press[KEY_L]) {
					if (!alt) {
						if (mixer->bus_list[mixer->selected_bus]->atten > 0.02)
							mixer->bus_list[mixer->selected_bus]->atten -= 0.02f;
						else 
							mixer->bus_list[mixer->selected_bus]->atten = 0.0f;
					} else {
						if (mixer->bus_list[mixer->selected_bus]->atten < 0.98f)
							mixer->bus_list[mixer->selected_bus]->atten += 0.02f;
						else 
							mixer->bus_list[mixer->selected_bus]->atten = 1.0f;
					}
				}

				// pan
				if (input->num_key_press[KEY_P]) {
					if (alt) {
						if (mixer->bus_list[mixer->selected_bus]->pan > -0.98)
							mixer->bus_list[mixer->selected_bus]->pan -= 0.02f;
						else 
							mixer->bus_list[mixer->selected_bus]->pan = -1.0f;
					} else {
						if (mixer->bus_list[mixer->selected_bus]->pan < 0.98f)
							mixer->bus_list[mixer->selected_bus]->pan += 0.02f;
						else 
							mixer->bus_list[mixer->selected_bus]->pan = 1.0f;
					}
				}

				// create new bus
				if (is_key_pressed(input, KEY_N)) {
					struct bus *b = init_bus(sp_state);
					attach_bus(b, &mixer->master, sp_state);
				}

				// delete selected bus
				if (is_key_pressed(input, KEY_D)) {
					// dont delete master or sample busses
					struct bus *b = mixer->bus_list[mixer->selected_bus];
					if (b->type != MASTER && b->type != SAMPLE)
						mixer->update_mode = DELETE;
				}

				// rename
				if (is_key_pressed(input, KEY_R)) {
					// TODO will want to abstract user input maybe
					mixer->r_buff_pos = 0;
					if (!(mixer->r_buff = malloc(R_BUFF_MAX))) {
						fprintf(stderr, "Error allocating input buffer\n");
						mixer->update_mode = NORMAL;
					} else {
						mixer->update_mode = RENAME;
					}
				}
			} 
	}
}

////////////////////////////////////////////////////////////////////////////////
/// SHELL

// outputs a string to the shell
static void shell_print(const char *t, struct sp_state *sp_state)
{
	struct shell *shell = &sp_state->shell;

	shell->output_size = strlen(t);
	shell->output_buff = realloc(shell->output_buff, shell->output_size);
	strncpy(shell->output_buff, t, shell->output_size);
}

static void update_shell(struct sp_state *sp_state, struct key_input *input)
{
	struct shell *shell = &sp_state->shell;
	ASSERT(shell);
	ASSERT(shell->input_buff);

	// print output in draw function

	// poll input
	const char alt = is_key_down(input, KEY_SHIFT_L) || is_key_down(input, KEY_SHIFT_R); 

	// get letter input 
	for (int i = KEY_A; i <= KEY_Z; i++) {
		for (int c = input->num_key_press[i]; c > 0; c--) {
			// resize input if needed
			// TODO shrink later if needed
			if (shell->input_pos >= shell->input_size) 
				shell->input_buff = realloc(shell->input_buff, shell->input_size *= 2);

			if (alt) {
				shell->input_buff[shell->input_pos++] = 'A' + i - KEY_A;
			} else {
				shell->input_buff[shell->input_pos++] = 'a' + i - KEY_A;
			}
		}
	}

	// get number input
	for (int i = KEY_0; i <= KEY_9; i++) {
		for (int c = input->num_key_press[i]; c > 0; c--) {
			// resize input if needed
			// TODO shrink later if needed
			if (shell->input_pos >= shell->input_size) 
				shell->input_buff = realloc(shell->input_buff, shell->input_size *= 2);

			shell->input_buff[shell->input_pos++] = '0' + i - KEY_0;
		}
	}

	for (int c = input->num_key_press[KEY_SPACE]; c > 0; c--) {
		if (shell->input_pos >= shell->input_size) 
			shell->input_buff = realloc(shell->input_buff, shell->input_size *= 2);

		shell->input_buff[shell->input_pos++] = ' ';
	}

	for (int c = input->num_key_press[KEY_BACKSPACE]; c > 0; c--) {
		if (shell->input_pos > 0) shell->input_pos--;
	}

	// on enter decide what to do with switch statement
	if (is_key_pressed(input, KEY_ENTER)) {
		// clear input
		shell->input_pos = 0;
	}
}
