#include "io.h"
#include "sample.h"
#include "ring_buffer.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#define TEST_WAV "test/StarWars.wav"

int main(int argc, char** argv)
{
	struct sample* samp = init_sample();
	load_wav_into_sample(TEST_WAV, samp);

	struct ring_buf* rb = init_ring_buf(129837); 
	if (!rb)
		printf("Error initializing ring buffer");

	char* tmp = calloc(1, samp->data_size);
	const int transfer_size = 11;
	int bytes_sent = 0;
	int bytes_received = 0;
	int _t;
	printf("Frame size: %d\n", transfer_size);

	while (bytes_sent + transfer_size <= samp->data_size) {
		while (bytes_sent + transfer_size <= samp->data_size) {
			_t = write_to_buf(rb, samp->data + bytes_sent, transfer_size); 
			bytes_sent += _t;
			if (!_t)
				break;
		}
		while ((_t = read_to_dest(rb, tmp + bytes_received, transfer_size)) != 0)
			bytes_received += _t; 

		//assert(tmp[bytes_received - 1] == samp->data[bytes_received - 1]);

	}
	for (int i = 0; i < bytes_sent; i++)
	{
		assert(tmp[i] == samp->data[i]);
	}
	printf("Original size: %d\n", samp->data_size);
	printf("Bytes transferred: %d\n", bytes_sent);
	play_clip(tmp, bytes_sent);

	return 0;
}
