#include "defs.h"
#include "file_io.h"
#include "system.h"
#include "audio_backend.h"
#include "raylib.h"

#define WAV1 "../test/GREEN.wav"
#define WAV2 "../test/drums/R-8can1.wav"
#define WAV3 "../test/drums/R-8solid k.wav"
#define WAV4 "../test/drums/R-8dry clap.wav"

// returns true once after sample in gate mode is released
static bool is_gate_released(struct sample* s)
{
	return s->gate && s->playing && !s->gate_closed; 
}

void update_sampler(struct system* sys, struct sampler* sampler)
{
	struct sample** banks = sys->banks;
	struct sample** active_sample = &sampler->active_sample;

	const bool alt = IsKeyDown(KEY_LEFT_SHIFT) || IsKeyDown(KEY_RIGHT_SHIFT);

	// trigger samples
	if (IsKeyPressed(KEY_Q)) {
		if (!alt) trigger_sample(&banks[0][PAD_Q]);
		*active_sample = &banks[0][PAD_Q];
	} else if (is_gate_released(&banks[0][PAD_Q]) && IsKeyUp(KEY_Q)){
		close_gate(&banks[0][PAD_Q]);
	}
	if (IsKeyPressed(KEY_W)) {
		if (!alt) trigger_sample(&banks[0][PAD_W]);
		*active_sample = &banks[0][PAD_W];
	} else if (is_gate_released(&banks[0][PAD_W]) && IsKeyUp(KEY_W)){
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
		for (int b = 0; b < sys->num_banks; b++) {
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

int main(int argc, char** argv)
{
	// init system
	struct system sys = {0};
	struct bus master = {0};
	sys.master = master;
	// init sampler
	struct sampler sampler = { NULL, 1, 0, 3000, 2, 1, AUTO};

	// init audio_playback
	struct alsa_dev a_dev = {0};
	if (open_alsa_dev(&a_dev, SAMPLE_RATE, NUM_CHANNELS)) {
		printf("Error opening audio device\n");
		return 0;
	}
	if (start_alsa_dev(&a_dev, &sys.master)) {
		printf("Error starting audio\n");
		return 0;
	}

	// init gui
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME); 
	SetTargetFPS(30);

	// load some data for testing
	sys.banks = malloc(sizeof(struct sample*));
	sys.banks[0] = calloc(NUM_PADS, sizeof(struct sample));
	sys.num_banks = 1;
	// load samples
	if (load_wav_into_sample(&sys.banks[0][0], WAV3)) {
		fprintf(stderr, "Error loading %s\n", WAV3);
		return 0;
	}
	if (load_wav_into_sample(&sys.banks[0][1], WAV4)) {
		fprintf(stderr, "Error loading %s\n", WAV4);
		return 0;
	}
	if (load_wav_into_sample(&sys.banks[0][2], WAV1)) {
		fprintf(stderr, "Error loading %s\n", WAV1);
		return 0;
	}
	// create some aux busses
	struct bus sb1 = {0};
	struct bus sb2 = {0};
	struct bus sb3 = {0};
	// attach samples to input
	sb1.sample_in = &sys.banks[0][0];
	sb2.sample_in = &sys.banks[0][1];
	sb3.sample_in = &sys.banks[0][2];
	// attach aux busses to master bus
	add_bus_in(&sys.master, &sb1);
	add_bus_in(&sys.master, &sb2);
	add_bus_in(&sys.master, &sb3);
	sampler.active_sample = &sys.banks[0][0];

	while(!WindowShouldClose()) {
		// update
		update_sampler(&sys, &sampler);
		// draw
		draw(&sys, &sampler);
	}
	CloseWindow();

	return 0;
}
