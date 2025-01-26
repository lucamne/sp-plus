#include "system.h"
#include "defs.h"

// returns true once after sample in gate mode is released
static bool is_gate_released(struct sample* s)
{
}

static int trigger_sample(struct sample* s)
{
	if (!s->reverse) s->next_frame = s->start_frame;
	else s->next_frame = s->end_frame - 1.0;

	if (s->loop_mode && s->playing)
		s->playing = false;
	else
		s->playing = true;
}

static int close_gate(struct sample* s)
{
	if (!s) return 1;
	// check if gate should release
	if (!(s->gate && s->playing && !s->gate_closed)) return 1;

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
	return 0;
}

void update_sampler(struct sampler* sampler)
{
	struct sample** banks = sampler->banks;
	struct sample** active_sample = &sampler->active_sample;

	const bool alt = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

	// trigger samples
	if (IsKeyPressed(KEY_Q)) {
		if (!alt) trigger_sample(&banks[0][PAD_Q]);
		*active_sample = &banks[0][PAD_Q];
	} else if (IsKeyUp(KEY_Q)){
		close_gate(&banks[0][PAD_Q]);
	}
	if (IsKeyPressed(KEY_W)) {
		if (!alt) trigger_sample(&banks[0][PAD_W]);
		*active_sample = &banks[0][PAD_W];
	} else if (IsKeyUp(KEY_W)){
		close_gate(&banks[0][PAD_W]);
	}
	if (IsKeyPressed(KEY_E)) {
		if (!alt) trigger_sample(&banks[0][PAD_E]);
		*active_sample = &banks[0][PAD_E];
	} else if (is_gate_released(&banks[0][PAD_E]) && IsKeyUp(KEY_E)){
		close_gate(&banks[0][PAD_E]);
	}
	if (IsKeyPressed(KEY_R)) {
		if (!alt) trigger_sample(&banks[0][PAD_R]);
		*active_sample = &banks[0][PAD_R];
	} else if (is_gate_released(&banks[0][PAD_R]) && IsKeyUp(KEY_R)){
		close_gate(&banks[0][PAD_R]);
	}
	// playback speed / pitch
	if (IsKeyPressed(KEY_O)) {
		const float st = speed_to_st(fabs((*active_sample)->speed));
		if (alt) set_speed(*active_sample, st_to_speed(st - 1));
		else set_speed(*active_sample, st_to_speed(st + 1));
	}
	// move start/end
	if (IsKeyDown(KEY_U)) {
		int32_t f; 
		int32_t inc = (float) (*active_sample)->num_frames / sampler->zoom / 100;
		if (inc == 0) inc = 1;
		if (alt)
			f = (*active_sample)->start_frame - inc; 
		else
			f = (*active_sample)->start_frame + inc;
		set_start(*active_sample, f);
		sampler->zoom_focus = START;
	}
	if (IsKeyDown(KEY_I)) {
		int32_t f; 
		int32_t inc = (float) (*active_sample)->num_frames / sampler->zoom / 100;
		if (inc == 0) inc = 1;
		if (alt)
			f = (*active_sample)->end_frame - inc; 
		else
			f = (*active_sample)->end_frame + inc;
		set_end(*active_sample, f);
		sampler->zoom_focus = END;
	}
	// set envelope
	if (IsKeyDown(KEY_J)) {
		const float ms = frames_to_ms((*active_sample)->attack);
		if (alt) set_attack(*active_sample, ms - 1);
		else set_attack(*active_sample, ms + 1);
	}
	if (IsKeyDown(KEY_K)) {
		const float ms = frames_to_ms((*active_sample)->release);
		if (alt) set_release(*active_sample, ms - 1);
		else set_release(*active_sample, ms + 1);
	}
	// set playback modes
	if (IsKeyPressed(KEY_G)) {
		(*active_sample)->gate = !(*active_sample)->gate;
	}
	if (IsKeyPressed(KEY_V)) {
		(*active_sample)->reverse = !(*active_sample)->reverse;
		(*active_sample)->speed *= -1.0;
	}
	if (IsKeyPressed(KEY_L)) {
		if ((*active_sample)->loop_mode == PING_PONG)
			(*active_sample)->loop_mode = LOOP_OFF;
		else
			(*active_sample)->loop_mode += 1;
	}
	// stop all samples
	if (IsKeyPressed(KEY_X)) {
		for (int b = 0; b < sampler->num_banks; b++) {
			for (int p = 0; p < NUM_PADS; p++) {
				kill_sample(&banks[b][p]);
			}
		}
	}
	if (IsKeyPressed(KEY_EQUAL)) {
		if (sampler->zoom <= 256) sampler->zoom *= 2;
	}
	if (IsKeyPressed(KEY_MINUS)) {
		sampler->zoom /= 2;
		if (sampler->zoom < 1) sampler->zoom = 1;
	}
}
