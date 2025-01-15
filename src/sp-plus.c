#include "defs.h"
#include "io.h"
#include "audio_backend.h"
#include "gui.h"
#include "raylib.h"

#define WAV1 "../test/drums/R-8ride1.wav"
#define WAV2 "../test/drums/R-8can1.wav"
#define WAV3 "../test/drums/R-8solid k.wav"
#define WAV4 "../test/drums/R-8dry clap.wav"

// holds core data structures of system
struct system {
	struct bus master;		// master bus, passed to audio callback
	// struct bus* aux_busses;		// non master busses
	// int num_aux_busses;
	struct sample** banks;	// all loaded samples [BANK][PAD]
	int num_banks;
	struct alsa_dev playback_dev;	// alsa playback device
};

void run(struct system* sys, struct sample_view_params* sp);

int main(int argc, char** argv)
{
	// init system
	struct system sys = {0};
	struct bus master = {0};
	sys.master = master;
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
	
	// init audio_playback
	if (open_alsa_dev(&sys.playback_dev, SAMPLE_RATE, NUM_CHANNELS)) {
		printf("Error opening audio device\n");
		return 0;
	}
	if (start_alsa_dev(&sys.playback_dev, &sys.master)) {
		printf("Error starting audio\n");
		return 0;
	}

	// init gui
	InitWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_NAME); 
	SetTargetFPS(30);
	struct sample_view_params sp = {
		NULL,
		WHITE,
		{15.0f, 225.0f},
		2.0f,
		400.0f,
		770.0f,
		4000,
		1.0f,
		START };

	run(&sys, &sp);

	return 0;
}


void run(struct system* sys, struct sample_view_params* sp)
{	
	struct sample** banks = sys->banks;
	
	while(!WindowShouldClose()) {
		// update
		if (IsKeyPressed(KEY_Q)) {
			sp->sample = &banks[0][0];
			trigger_sample(&banks[0][0]);
		}
		if (IsKeyPressed(KEY_W)) {
			sp->sample = &banks[0][1];
			trigger_sample(&banks[0][1]);
		}
		if (IsKeyDown(KEY_U)) {
			const int32_t f = 
				(sp->sample->start_frame - 
				 sp->sample->data) 
				/ 2;
			set_start(sp->sample, f - 1000);
		}
		if (IsKeyDown(KEY_I)) {
			const int32_t f = 
				(sp->sample->start_frame - 
				 sp->sample->data) 
				/ 2;
			set_start(sp->sample, f + 1000);
		}
		if (IsKeyDown(KEY_J)) {
			const int32_t f = 
				(sp->sample->end_frame - 
				 sp->sample->data) 
				/ 2;
			set_end(sp->sample, f - 1000);
		}
		if (IsKeyDown(KEY_K)) {
			const int32_t f = 
				(sp->sample->end_frame - 
				 sp->sample->data) 
				/ 2;
			set_end(sp->sample, f + 1000);
		}
		// draw
		draw(sp);
	}
	CloseWindow();
}
