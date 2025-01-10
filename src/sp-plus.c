#include "io.h"
#include "defs.h"
#include "signal_chain.h"

#include <signal.h>
#include <sys/time.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_WAV "test/StarWars.wav"

void on_sigio(int sig)
{
	printf("Caught %d:\n", sig);
}

int main(int argc, char** argv)
{
	struct sample* samp1 = init_sample();
	load_wav_into_sample(TEST_WAV, samp1);
	/*
	struct sample* samp2 = init_sample();
	load_wav_into_sample(TEST_WAV, samp2);
	samp2->playing = false;
	*/

	struct bus* master = init_bus();
	struct bus* sb1 = init_bus();
	// struct bus* sb2 = init_bus();
	sb1->sample_in = samp1;
	// sb2->sample_in = samp2;
	set_pan(sb1, 1.0f);
	// set_pan(sb2, -1.0f);

	add_bus_in(master, sb1);
	// add_bus_in(master, sb2);

	struct alsa_dev* a_dev = 
		open_alsa_dev(SAMPLE_RATE, NUM_CHANNELS);
	if (!a_dev) {
		printf("Error opening audio device\n");
		return 0;
	}

	int err = start_alsa_dev(a_dev, master);
	if (err) {
		printf("Error starting audio\n");
		return 0;
	}

	int c = 0;
	while(1) {
		sleep(1);
		/*
		c++;
		if (!samp2->playing && c >= 50){
			samp2->playing = true;
			printf("turned on samp2\n");
		}*/
	}
	return 0;
}
