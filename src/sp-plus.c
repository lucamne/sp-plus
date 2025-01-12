#include "io.h"
#include "defs.h"
#include "playback.h"
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
	if (load_wav_into_sample(&banks[0][2], WAV3)) {
		fprintf(stderr, "Error loading %s\n", WAV3);
		return 0;
	}
	if (load_wav_into_sample(&banks[0][3], WAV4)) {
		fprintf(stderr, "Error loading %s\n", WAV4);
		return 0;
	}
	// create busses
	struct bus master = {0};
	struct bus sb1 = {0};
	struct bus sb2 = {0};
	struct bus sb3 = {0};
	struct bus sb4 = {0};
	// attach samples to input
	sb1.sample_in = &banks[0][0];
	sb2.sample_in = &banks[0][1];
	sb3.sample_in = &banks[0][2];
	sb4.sample_in = &banks[0][3];
	// attach aux busses to master bus
	add_bus_in(&master, &sb1);
	add_bus_in(&master, &sb2);
	add_bus_in(&master, &sb3);
	add_bus_in(&master, &sb4);
	
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

	// draw window
	const int screen_width = 800;
	const int screen_height = 450;

	InitWindow(screen_width, screen_height, "My first window!!!");
	SetTargetFPS(60);

	while(!WindowShouldClose()) {
		BeginDrawing();
		ClearBackground(RAYWHITE);
		DrawText("Press q, w, e, r to play sample", 50, 200, 40, RED);

		if (IsKeyPressed(KEY_Q))
			trigger_sample(&banks[0][0]);
		if (IsKeyPressed(KEY_W))
			trigger_sample(&banks[0][1]);
		if (IsKeyPressed(KEY_E))
			trigger_sample(&banks[0][2]);
		if (IsKeyPressed(KEY_R))
			trigger_sample(&banks[0][3]);

		EndDrawing();
	}
	CloseWindow();
	return 0;
}
