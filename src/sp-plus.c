#include "defs.h"
#include "file_io.h"
#include "system.h"
#include "audio_backend.h"
#include "gui.h"
#include "raylib.h"

#define WAV1 "../test/drums/R-8ride1.wav"
#define WAV2 "../test/drums/R-8can1.wav"
#define WAV3 "../test/drums/R-8solid k.wav"
#define WAV4 "../test/drums/R-8dry clap.wav"


void run(struct system* sys, struct sampler* sampler);

int main(int argc, char** argv)
{
	// init system
	struct system sys = {0};
	struct bus master = {0};
	sys.master = master;
	// init sampler
	struct sampler sampler = {0};
	
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
	if (load_wav_into_sample(&sys.banks[0][0], WAV1)) {
		fprintf(stderr, "Error loading %s\n", WAV1);
		return 0;
	}
	if (load_wav_into_sample(&sys.banks[0][1], WAV2)) {
		fprintf(stderr, "Error loading %s\n", WAV2);
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

	run(&sys, &sampler);

	return 0;
}


void run(struct system* sys, struct sampler* sampler)
{	
	struct sample** banks = sys->banks;
	struct sample** active_sample = &sampler->active_sample;
	
	while(!WindowShouldClose()) {
		// update
		if (IsKeyPressed(KEY_Q)) {
			*active_sample = &banks[0][0];
			trigger_sample(&banks[0][0]);
		}
		if (IsKeyPressed(KEY_W)) {
			*active_sample = &banks[0][1];
			trigger_sample(&banks[0][1]);
		}
		if (IsKeyDown(KEY_U)) {
			set_start(*active_sample, get_start(*active_sample) - 1000);
		}
		if (IsKeyDown(KEY_I)) {
			set_start(*active_sample, get_start(*active_sample) + 1000);
		}
		if (IsKeyDown(KEY_J)) {
			set_end(*active_sample, get_end(*active_sample) - 1000);
		}
		if (IsKeyDown(KEY_K)) {
			set_end(*active_sample, get_end(*active_sample) + 1000);
		}
		// loop active sample
		if (IsKeyPressed(KEY_L)) {
			(*active_sample)->loop = !(*active_sample)->loop;
		}
		// stop all samples
		if (IsKeyPressed(KEY_X)) {
			for (int b = 0; b < sys->num_banks; b++) {
				for (int p = 0; p < NUM_PADS; p++) {
					banks[b][p].playing = false;
					banks[b][p].next_frame = 
						banks[b][p].start_frame;
				}
			}
		}
		// draw
		draw(sys, sampler);
	}
	CloseWindow();
}
