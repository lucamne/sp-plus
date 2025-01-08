#include "io.h"
#include "defs.h"
#include "sample.h"
#include "bus.h"

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
	struct sample* samp = init_sample();
	load_wav_into_sample(TEST_WAV, samp);

	struct bus* master = init_bus();
	master->sample_in = samp;

	struct alsa_dev* a_dev = 
		open_alsa_dev(22050, 2);
	if (!a_dev) {
		printf("Error opening audio device\n");
		return 0;
	}

	int err = start_alsa_dev(a_dev, master);
	if (err) {
		printf("Error starting audio\n");
		return 0;
	}


	while(1) {
		sleep(1);
	}
	return 0;
}
