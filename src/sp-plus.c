#include "io.h"
#include "defs.h"
#include "audio_backend.h"
#include "gui.h"
#include "raylib.h"

#define WAV1 "../test/drums/R-8ride1.wav"
#define WAV2 "../test/drums/R-8can1.wav"
#define WAV3 "../test/drums/R-8solid k.wav"
#define WAV4 "../test/drums/R-8dry clap.wav"
#define NUM_PADS 8


int main(int argc, char** argv)
{
	// create banks
	struct sample** banks = malloc(sizeof(struct sample*));
	banks[0] = calloc(NUM_PADS, sizeof(struct sample));
	// load samples
	if (load_wav_into_sample(&banks[0][0], WAV1)) {
		fprintf(stderr, "Error loading %s\n", WAV1);
		return 0;
	}
	if (load_wav_into_sample(&banks[0][1], WAV2)) {
		fprintf(stderr, "Error loading %s\n", WAV2);
		return 0;
	}
	// create busses
	struct bus master = {0};
	struct bus sb1 = {0};
	struct bus sb2 = {0};
	// attach samples to input
	sb1.sample_in = &banks[0][0];
	sb2.sample_in = &banks[0][1];
	// attach aux busses to master bus
	add_bus_in(&master, &sb1);
	add_bus_in(&master, &sb2);
	
	// create audio playback handle
	struct alsa_dev a_dev = {0};
	if (open_alsa_dev(&a_dev, SAMPLE_RATE, NUM_CHANNELS)) {
		printf("Error opening audio device\n");
		return 0;
	}
	// start audio listening
	if (start_alsa_dev(&a_dev, &master)) {
		printf("Error starting audio\n");
		return 0;
	}

	// init window
	const int screen_width = 800;
	const int screen_height = 450;

	InitWindow(screen_width, screen_height, "My first window!!!");
	SetTargetFPS(30);
	// init view data
	struct sample_view_params sample_params = {
		NULL,
		WHITE,
		{15.0f, 225.0f},
		2.0f,
		400.0f,
		770.0f,
		4000,
		1.0f,
		START };

	const Vector2 sample_origin = {15.0f, 225.0f};

	while(!WindowShouldClose()) {
		// update
		if (IsKeyPressed(KEY_Q)) {
			sample_params.sample = &banks[0][0];
			trigger_sample(&banks[0][0]);
		}
		if (IsKeyPressed(KEY_W)) {
			sample_params.sample = &banks[0][1];
			trigger_sample(&banks[0][1]);
		}
		if (IsKeyDown(KEY_U)) {
			const int32_t f = 
				(sample_params.sample->start_frame - 
				 sample_params.sample->data) 
				/ 2;
			set_start(sample_params.sample, f - 1000);
		}
		if (IsKeyDown(KEY_I)) {
			const int32_t f = 
				(sample_params.sample->start_frame - 
				 sample_params.sample->data) 
				/ 2;
			set_start(sample_params.sample, f + 1000);
		}
		if (IsKeyDown(KEY_J)) {
			const int32_t f = 
				(sample_params.sample->end_frame - 
				 sample_params.sample->data) 
				/ 2;
			set_end(sample_params.sample, f - 1000);
		}
		if (IsKeyDown(KEY_K)) {
			const int32_t f = 
				(sample_params.sample->end_frame - 
				 sample_params.sample->data) 
				/ 2;
			set_end(sample_params.sample, f + 1000);
		}
		// draw
		BeginDrawing();
		ClearBackground(BLACK);
		draw_sample_view(&sample_params);
		EndDrawing();
	}
	CloseWindow();
	return 0;
}
