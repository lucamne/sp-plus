/** Audio API Quick Start Guide: ALSA: Play audio from stdin
Link with -lasound */
#include "io.h"

#include <alsa/asoundlib.h>
#include <assert.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <stdint.h>

int quit;

snd_pcm_t* abuf_create(int *buf_size, int *frame_size)
{
	// Attach audio buffer to device
	snd_pcm_t *pcm;
	const char *device_id = "plughw:1,0"; // Use default device
	int mode = SND_PCM_STREAM_PLAYBACK;
	// mode is nonblocking
	assert(0 == snd_pcm_open(&pcm, device_id, mode, 0));

	// Get device property-set
	snd_pcm_hw_params_t *params;
	snd_pcm_hw_params_alloca(&params);
	assert(0 <= snd_pcm_hw_params_any(pcm, params));

	// Specify how we want to access audio data
	int access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
	assert(0 == snd_pcm_hw_params_set_access(pcm, params, access));

	// Set sample format
	int format = SND_PCM_FORMAT_S16_LE;
	assert(0 == snd_pcm_hw_params_set_format(pcm, params, format));

	// Set channels
	unsigned int channels = 1;
	assert(0 == snd_pcm_hw_params_set_channels_near(pcm, params, &channels));

	// Set sample rate
	unsigned int sample_rate = 22050;
	snd_pcm_hw_params_set_rate(pcm, params, sample_rate, 0);
	fprintf(stderr, "Using format int16, sample rate %u, channels %u\n", sample_rate, channels);

	// Set audio buffer length
	unsigned int buffer_length_usec = 500 * 1000;
	assert(0 == snd_pcm_hw_params_set_buffer_time_near(pcm, params, &buffer_length_usec, NULL));

	// Apply configuration
	assert(0 == snd_pcm_hw_params(pcm, params));

	*frame_size = (16/8) * channels;
	// buf_size = frame_size * samples_per_sec * buffer length in sec
	*buf_size = sample_rate * (16/8) * channels * buffer_length_usec / 1000000;
	return pcm;
}

void on_sigint()
{
	quit = 1;
}

int abuf_handle_error(snd_pcm_t *pcm, int r)
{
	switch (r) {

	case -ESTRPIPE:
		// Sound device is temporarily unavailable.  Wait until it's online.
		while (-EAGAIN == (r = snd_pcm_resume(pcm))) {
			int period_ms = 100;
			usleep(period_ms*1000);
		}
		if (r == 0)
			return 0;
		// fallthrough

	case -EPIPE:
		// Overrun or underrun occurred.  Reset buffer.
		if (0 > (r = snd_pcm_prepare(pcm)))
			return r;
		return 0;
	}

	return r;
}

void play_clip (void* clip, int clip_size)
{
	int bytes_read = 0;

	unsigned int buf_size;
	unsigned int frame_size;
	snd_pcm_t *pcm = abuf_create(&buf_size, &frame_size);

	// Properly handle SIGINT from user
	struct sigaction sa = {};
	sa.sa_handler = on_sigint;
	sigaction(SIGINT, &sa, NULL);

	// Read audio samples from stdin and pass them to audio buffer
	int r = 0;
	while (!quit) {

		if (r < 0)
			assert(0 == abuf_handle_error(pcm, r));

		// Refresh audio buffer state
		if (0 > (r = snd_pcm_avail_update(pcm)))
			continue;

		// Get audio data region available for writing
		const snd_pcm_channel_area_t *areas;
		snd_pcm_uframes_t off;
		snd_pcm_uframes_t frames = buf_size / frame_size;
		if (0 != (r = snd_pcm_mmap_begin(pcm, &areas, &off, &frames)))
			continue;

		if (frames == 0) {
			// Buffer is full

			if (SND_PCM_STATE_RUNNING != snd_pcm_state(pcm)) {
				// Stream isn't running.  Start it.
				assert(0 == snd_pcm_start(pcm));
			}

			// Wait 100ms until some free space is available
			int period_ms = 100;
			usleep(period_ms*1000);
			continue;
		}

		// Read data from stdin
		// get address to write data to
		void *data = (char*)areas[0].addr + off * areas[0].step/8;
		// get size to read in bytes
		int n = frames * frame_size;
		if (n + bytes_read > clip_size) {
			n = clip_size - bytes_read;
			if (n < (int) frame_size) 
				break;
		}
		memcpy(data, (int8_t*) clip + bytes_read, n);
		bytes_read += n;
		assert(n%frame_size == 0);
		frames = n / frame_size;

		// Mark the data chunk as complete
		// weird that r is redefined, might have to fix
		snd_pcm_sframes_t r = snd_pcm_mmap_commit(pcm, off, frames);
		if (r >= 0 && (snd_pcm_uframes_t)r != frames) {
			// Not all frames are processed, overrun occured
			r = -EPIPE;
		}

		if (n == 0)
			break; // stdin data is complete
	}

	// Wait until all bufferred data is played by audio device
	while (!quit) {

		// Refresh audio buffer state
		if (0 >= (r = snd_pcm_avail_update(pcm)))
			break;

		if (SND_PCM_STATE_RUNNING != snd_pcm_state(pcm)) {
			// Stream isn't running.  Start it.
			assert(0 == snd_pcm_start(pcm));
		}

		// Wait 100ms until some free space is available
		int period_ms = 100;
		usleep(period_ms*1000);
	}

	snd_pcm_close(pcm);
}
