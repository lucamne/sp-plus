#include "system.h"
#include "defs.h"

static void trigger_sample(struct sample* s)
{
	if (!s->reverse) s->next_frame = s->start_frame;
	else s->next_frame = s->end_frame - 1.0;

	if (s->loop_mode && s->playing) s->playing = false;
	else s->playing = true;
}

static void close_gate(struct sample* s)
{
	if (!s) return;
	// check if gate should release
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

static void squeeze_envelope(struct sample* s )
{
	if (s->end_frame - s->start_frame > s->attack + s->release) return;
	float r = (float) s->attack / (s->release + s->attack);
	s->attack = (s->end_frame - s->start_frame) * r;
	s->release = s->end_frame - s->start_frame - s->attack;
}


void update_sampler(struct sampler* sampler)
{
	struct sample** banks = sampler->banks;
	struct sample** active_sample = &sampler->active_sample;
	int cur_bank = sampler->cur_bank;

	const bool alt = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

	/* Update general sampler parameters */
	// trigger samples
	if (IsKeyPressed(KEY_Q)) {
		if (!alt) trigger_sample(&banks[cur_bank][PAD_Q]);
		*active_sample = &banks[cur_bank][PAD_Q];
	} else if (IsKeyUp(KEY_Q)){
		close_gate(&banks[cur_bank][PAD_Q]);
	}
	if (IsKeyPressed(KEY_W)) {
		if (!alt) trigger_sample(&banks[cur_bank][PAD_W]);
		*active_sample = &banks[cur_bank][PAD_W];
	} else if (IsKeyUp(KEY_W)){
		close_gate(&banks[cur_bank][PAD_W]);
	}
	if (IsKeyPressed(KEY_E)) {
		if (!alt) trigger_sample(&banks[cur_bank][PAD_E]);
		*active_sample = &banks[cur_bank][PAD_E];
	} else if (IsKeyUp(KEY_E)){
		close_gate(&banks[cur_bank][PAD_E]);
	}
	if (IsKeyPressed(KEY_R)) {
		if (!alt) trigger_sample(&banks[cur_bank][PAD_R]);
		*active_sample = &banks[cur_bank][PAD_R];
	} else if (IsKeyUp(KEY_R)){
		close_gate(&banks[cur_bank][PAD_R]);
	}
	// stop all samples
	if (IsKeyPressed(KEY_X)) {
		for (int b = 0; b < sampler->num_banks; b++) {
			for (int p = 0; p < NUM_PADS; p++) {
				kill_sample(&banks[b][p]);
			}
		}
	}
	// waveform viewer zoom
	if (IsKeyPressed(KEY_EQUAL)) {
		if (sampler->zoom <= 256) sampler->zoom *= 2;
	}
	if (IsKeyPressed(KEY_MINUS)) {
		sampler->zoom /= 2;
		if (sampler->zoom < 1) sampler->zoom = 1;
	}
	// switch bank
	if (IsKeyPressed(KEY_B)) {
		if (!alt && sampler->cur_bank + 1 < sampler->num_banks)
			sampler->cur_bank += 1;
		else if (sampler->cur_bank > 0 ) 
			sampler->cur_bank -= 1;
	}

	/* Update active sample */
	struct sample *s = *active_sample;
	if (!s) return;
	// playback speed / pitch
	if (IsKeyPressed(KEY_O)) {
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
	// move start/end
	if (IsKeyDown(KEY_U)) {
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
	if (IsKeyDown(KEY_I)) {
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
	if (IsKeyDown(KEY_J)) {
		float ms = frames_to_ms(s->attack);
		if (alt) ms -= 1;
		else ms += 1;
		
		if (ms >= 0) { 
			const int32_t frames = ms_to_frames(ms);
			if (s->end_frame - s->start_frame - s->release >= frames) {
				s->attack = frames;
			}
		}
	}
	if (IsKeyDown(KEY_K)) {
		float ms = frames_to_ms(s->release);
		if (alt) ms -= 1; 
		else ms += 1;

		if (ms >= 0) {
			const int32_t frames = ms_to_frames(ms);
			if (s->end_frame - s->start_frame - s->attack >= frames) {
				s->release = frames;
			}
		}
	}
	// set playback modes
	if (IsKeyPressed(KEY_G)) {
		s->gate = !s->gate;
	}
	if (IsKeyPressed(KEY_V)) {
		s->reverse = !s->reverse;
		s->speed *= -1.0;
	}
	if (IsKeyPressed(KEY_L)) {
		if (s->loop_mode == PING_PONG) s->loop_mode = LOOP_OFF;
		else s->loop_mode += 1;
	}
}
