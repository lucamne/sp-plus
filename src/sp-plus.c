#include "defs.h"
#include "file_io.h"
#include "system.h"
#include "raylib.h"

#define WAV1 "../test/GREEN.wav"
#define WAV2 "../test/drums/R-8can1.wav"
#define WAV3 "../test/drums/R-8solid k.wav"
#define WAV4 "../test/drums/R-8dry clap.wav"

int main(int argc, char** argv)
{
	// init master bus
	struct bus master = {0};
	// init sampler
	struct sampler sampler = { NULL, 0, NULL, 1, 0, 3000 };

	// init audio_playback
	struct alsa_dev a_dev = {0};
	if (open_alsa_dev(&a_dev, SAMPLE_RATE, NUM_CHANNELS)) {
		printf("Error opening audio device\n");
		return 0;
	}
	if (start_alsa_dev(&a_dev, &master)) {
		printf("Error starting audio\n");
		return 0;
	}

	// init gui
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME); 
	SetTargetFPS(30);

	// load some data for testing
	sampler.banks = malloc(sizeof(struct sample*));
	sampler.banks[0] = calloc(NUM_PADS, sizeof(struct sample));
	sampler.num_banks = 1;
	// load samples
	if (load_wav_into_sample(&sampler.banks[0][0], WAV3)) {
		fprintf(stderr, "Error loading %s\n", WAV3);
		return 0;
	}
	if (load_wav_into_sample(&sampler.banks[0][1], WAV4)) {
		fprintf(stderr, "Error loading %s\n", WAV4);
		return 0;
	}
	if (load_wav_into_sample(&sampler.banks[0][2], WAV1)) {
		fprintf(stderr, "Error loading %s\n", WAV1);
		return 0;
	}
	// create some aux busses
	struct bus sb1 = {0};
	struct bus sb2 = {0};
	struct bus sb3 = {0};
	// attach samples to input
	sb1.sample_in = &sampler.banks[0][0];
	sb2.sample_in = &sampler.banks[0][1];
	sb3.sample_in = &sampler.banks[0][2];
	// attach aux busses to master bus
	add_bus_in(&master, &sb1);
	add_bus_in(&master, &sb2);
	add_bus_in(&master, &sb3);
	sampler.active_sample = &sampler.banks[0][0];

	while(!WindowShouldClose()) {
		// update
		update_sampler(&sampler);
		// draw
		draw(&sampler);
	}
	CloseWindow();

	return 0;
}
