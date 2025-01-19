#include "defs.h"
#include "file_io.h"
#include "system.h"
#include "audio_backend.h"
#include "raylib.h"

#define WAV1 "../test/drums/R-8ride1.wav"
#define WAV2 "../test/drums/R-8can1.wav"
#define WAV3 "../test/drums/R-8solid k.wav"
#define WAV4 "../test/drums/R-8dry clap.wav"

void update_sampler(struct system* sys, struct sampler* sampler)
{
	struct sample** banks = sys->banks;
	struct sample** active_sample = &sampler->active_sample;

	// trigger samples
	if (IsKeyPressed(KEY_Q)) {
		*active_sample = &banks[0][PAD_Q];
		trigger_sample(&banks[0][PAD_Q]);
	// check for gate release
	} else if (	banks[0][PAD_Q].gate && 
			banks[0][PAD_Q].playing && 
			!banks[0][PAD_Q].gate_closed && 
			IsKeyUp(KEY_Q)) {
		close_gate(&banks[0][PAD_Q]);
	}
	if (IsKeyPressed(KEY_W)) {
		*active_sample = &banks[0][PAD_W];
		trigger_sample(&banks[0][PAD_W]);
	// check for gate release
	} else if (	banks[0][PAD_W].gate && 
			banks[0][PAD_W].playing && 
			!banks[0][PAD_W].gate_closed && 
			IsKeyUp(KEY_W)) {
		close_gate(&banks[0][PAD_W]);
	}
	// set stretch
	if (IsKeyPressed(KEY_S)) {
		const int f = abs((*active_sample)->frame_increment);
		set_frame_increment(*active_sample, f + 10);
	}
	if (IsKeyPressed(KEY_A)) {
		const int f = abs((*active_sample)->frame_increment);
		set_frame_increment(*active_sample, f - 10);
	}
	// move markers
	if (IsKeyDown(KEY_U)) {
		const int32_t f = (*active_sample)->start_frame - 1000 / sampler->zoom;
		set_start(*active_sample, f);
		sampler->zoom_focus = START;
	}
	if (IsKeyDown(KEY_I)) {
		const int32_t f = (*active_sample)->start_frame + 1000 / sampler->zoom;
		set_start(*active_sample, f);
		sampler->zoom_focus = START;
	}
	if (IsKeyDown(KEY_J)) {
		const int32_t f = (*active_sample)->end_frame - 1000 / sampler->zoom;
		set_end(*active_sample, f);
		sampler->zoom_focus = END;
	}
	if (IsKeyDown(KEY_K)) {
		const int32_t f = (*active_sample)->end_frame + 1000 / sampler->zoom;
		set_end(*active_sample, f);
		sampler->zoom_focus = END;
	}
	// set playback modes
	if (IsKeyPressed(KEY_G)) {
		(*active_sample)->gate = !(*active_sample)->gate;
	}
	if (IsKeyPressed(KEY_R)) {
		(*active_sample)->reverse = !(*active_sample)->reverse;
		(*active_sample)->frame_increment *= -1.0;
	}
	if (IsKeyPressed(KEY_O)) {
		(*active_sample)->loop_mode = OFF;
	}
	if (IsKeyPressed(KEY_P)) {
		(*active_sample)->loop_mode = PING_PONG;
	}
	if (IsKeyPressed(KEY_L)) {
		(*active_sample)->loop_mode = LOOP;
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
		sampler->zoom *= 2;
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
	struct sampler sampler = {0};
	sampler.zoom = 1;

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
	set_attack(&sys.banks[0][0], 10000);
	set_release(&sys.banks[0][0], 10000);
	if (load_wav_into_sample(&sys.banks[0][1], WAV4)) {
		fprintf(stderr, "Error loading %s\n", WAV4);
		return 0;
	}
	// create some aux busses
	struct bus sb1 = {0};
	struct bus sb2 = {0};
	// attach samples to input
	sb1.sample_in = &sys.banks[0][0];
	sb2.sample_in = &sys.banks[0][1];
	// attach aux busses to master bus
	add_bus_in(&sys.master, &sb1);
	add_bus_in(&sys.master, &sb2);

	while(!WindowShouldClose()) {
		// update
		update_sampler(&sys, &sampler);
		// draw
		draw(&sys, &sampler);
	}
	CloseWindow();

	return 0;
}
