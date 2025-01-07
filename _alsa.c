/** Audio API Quick Start Guide: ALSA: Play audio from stdin
Link with -lasound */
#include "io.h"
#include "defs.h"

#include <assert.h>
#include <unistd.h>
#include <stdio.h>
#include <stdint.h>

static int xrun_recovery(snd_pcm_t *handle, int err)
{
    	if (err == -EPIPE) {    /* under-run */
		err = snd_pcm_prepare(handle);
		if (err < 0)
		    printf("Can't recovery from underrun," 
				    "prepare failed: %s\n", 
				    snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(handle)) == -EAGAIN)
			sleep(1);   /* wait until the suspend flag is released */
		if (err < 0) {
		    	err = snd_pcm_prepare(handle);
		    	if (err < 0)
				printf("Can't recovery from suspend,"
					"prepare failed: %s\n", 
					snd_strerror(err));
		}
		return 0;
	}
	return err;
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

// globals for signal handlers
static int quit;
struct async_private_data {
	int buffer_frames;
	int frame_size;
	struct ring_buf* out_buf;
};
// signal handlers
void on_sigint()
{
	quit = 1;
}

static void async_callback(snd_async_handler_t *ahandler)
{
	snd_pcm_state_t state;
	int err;
	snd_pcm_t* pcm = snd_async_handler_get_pcm(ahandler);
	struct async_private_data* a_data = 
		snd_async_handler_get_callback_private(ahandler);
	
	state = snd_pcm_state(pcm);
	if (state == SND_PCM_STATE_XRUN) {
	    err = xrun_recovery(pcm, -EPIPE);
            if (err < 0) {
                printf("XRUN recovery failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
        } else if (state == SND_PCM_STATE_SUSPENDED) {
            err = xrun_recovery(pcm, -ESTRPIPE);
            if (err < 0) {
                printf("SUSPEND recovery failed: %s\n", snd_strerror(err));
                exit(EXIT_FAILURE);
            }
        }
	
	// check available frames
	snd_pcm_sframes_t avail = snd_pcm_avail_update(pcm);
	if (avail < 0) {
		err = xrun_recovery(pcm, avail);
		if (err < 0) {
			printf("avail update failed: %s\n", snd_strerror(err));
			exit(EXIT_FAILURE);
		}
		return;
	}
	
	
	// get buffer area
	const snd_pcm_channel_area_t* area;
	snd_pcm_uframes_t off;
	snd_pcm_uframes_t frames =
		(snd_pcm_uframes_t) a_data->buffer_frames;
	err = snd_pcm_mmap_begin(pcm, &area, &off, &frames);
	if (err < 0) {
        	if ((err = xrun_recovery(pcm, err)) < 0) {
                	printf("MMAP begin avail error: %s\n", snd_strerror(err));
                    	exit(EXIT_FAILURE);
                }
	}

	// buffer is full
	if (frames == 0) {
		// if stream is not running
		if (snd_pcm_state(pcm) != SND_PCM_STATE_RUNNING) {
			const int err = snd_pcm_start(pcm);
			assert(!err);
		}
		return;
	}
	char* data = (char *) area[0].addr + off * area[0].step / 8;

	// read bytes from out_buf to data
	// THERE COULD BE ISSUES IF BUFFER SIZE DOES NOT FIT IN INT
	// take min of bytes in out_buf and space in pcm buffer
	int copy_size = 
		a_data->out_buf->in_use <
		(int) frames * a_data->frame_size ?
		a_data->out_buf->in_use :
		(int) frames * a_data->frame_size;
	// ensure only full frames are transferred
	copy_size -= copy_size % a_data->frame_size;
	const int b_out = read_to_dest(a_data->out_buf, data, copy_size);
	assert(b_out == copy_size);
	// mark chunk as complete
	const snd_pcm_sframes_t sent_frames = 
		snd_pcm_mmap_commit(pcm, off, frames);
	if (sent_frames < 0) {
		abuf_handle_error(pcm, sent_frames);
		return; 
	}
	if (sent_frames != copy_size / a_data->frame_size) {
		abuf_handle_error(pcm, -EPIPE);
		fprintf(stderr, "Buffer Overun!\n");
		return;
	}
	return;
}


snd_pcm_t* abuf_create(int *buf_size, int *frame_size)
{
	// Attach audio buffer to device
	snd_pcm_t *pcm;
	const char *device_id = "plughw:1,0"; // Use default device
	int mode = SND_PCM_STREAM_PLAYBACK;
	// mode is nonblocking
	const int t = snd_pcm_open(&pcm, device_id, mode, 0); 
	printf("%s\n", snd_strerror(t));
	assert(0 == t);

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

static int pcm_open_err(struct alsa_dev* a_dev, int err)
{
	if (err) {
		if (a_dev->pcm) 
			free(a_dev->pcm);
		if (a_dev->params)
			free(a_dev->params);
		free(a_dev);
		fprintf(stderr, "%s\n", snd_strerror(err));
		return 1;
	}
	return 0;
}

struct alsa_dev* open_alsa_dev(int r, int num_c, int buf_frames, int f_size)
{
	struct alsa_dev* a_dev = malloc(sizeof(struct alsa_dev));
	if (!a_dev)
		return NULL;

	a_dev->rate = r;
	a_dev->num_channels = num_c;
	a_dev->buffer_frames = buf_frames;
	a_dev->frame_size = f_size;
	a_dev->dev_id = "plughw:1,0";
	// open pcm device in playback async
	const int stream = SND_PCM_STREAM_PLAYBACK;
	const int mode = SND_PCM_ASYNC;
	int err;
	err = snd_pcm_open(&(a_dev->pcm), a_dev->dev_id, stream, mode);
	if (pcm_open_err(a_dev, err))
		return NULL;

	// setup device
	snd_pcm_hw_params_t *params = a_dev->params;
	snd_pcm_hw_params_alloca(&params);
	snd_pcm_hw_params_any(a_dev->pcm, params);
	const int access = SND_PCM_ACCESS_MMAP_INTERLEAVED;
	const int format = SND_PCM_FORMAT_S16_LE;
	unsigned int channels = (unsigned int) a_dev->num_channels;
	const unsigned int sample_rate = (unsigned int) a_dev->rate;

	err = snd_pcm_hw_params_set_access(a_dev->pcm, params, access);
	if (pcm_open_err(a_dev, err))
		return NULL;

	err = snd_pcm_hw_params_set_format(a_dev->pcm, params, format);
	if (pcm_open_err(a_dev, err))
		return NULL;

	err = snd_pcm_hw_params_set_channels_near(a_dev->pcm, params, &channels);
	if (pcm_open_err(a_dev, err))
		return NULL;

	err = snd_pcm_hw_params_set_rate(a_dev->pcm, params, sample_rate, 0);
	if (pcm_open_err(a_dev, err))
		return NULL;

	fprintf(stderr, "Using format int16," 
			"sample rate %u, channels %u\n", 
			sample_rate, channels);

	const snd_pcm_uframes_t buffer_size = 
		(snd_pcm_uframes_t) a_dev->buffer_frames;
	err = snd_pcm_hw_params_set_buffer_size(a_dev->pcm, params, buffer_size);
	if (pcm_open_err(a_dev, err))
		return NULL;

	// apply config
	err = snd_pcm_hw_params(a_dev->pcm, params);
	if (pcm_open_err(a_dev, err))
		return NULL;
	// after applying the config pcm device should be in prepared state
	assert(snd_pcm_state(a_dev->pcm) == SND_PCM_STATE_PREPARED);

	return a_dev;
}

int start_alsa_dev(struct alsa_dev* a_dev, struct ring_buf* buf)
{
	int err;

	snd_async_handler_t* ahandler;
	struct async_private_data data;
	data.buffer_frames = a_dev->buffer_frames;
	data.frame_size = a_dev->frame_size;
	data.out_buf = buf;

	err = snd_async_add_pcm_handler(
			&ahandler, a_dev->pcm, async_callback, &data);
	if (err < 0) {
		printf("Unable to register async handler\n");
		exit(EXIT_FAILURE);
	}

	// prime buffer with empty data
	char* empty = calloc(1, a_dev->buffer_frames * a_dev->frame_size);
	const snd_pcm_channel_area_t* area;
	snd_pcm_uframes_t off;
	snd_pcm_uframes_t frames = (snd_pcm_uframes_t) a_dev->buffer_frames;

        err = snd_pcm_mmap_begin(a_dev->pcm, &area, &off, &frames);
	if (err < 0) {
		if ((err = xrun_recovery(a_dev->pcm, err)) < 0) {
			printf("MMAP begin avail error: %s\n", 
					snd_strerror(err));
			exit(EXIT_FAILURE);
		}
        }
	char* pcm_buffer = (char *) area[0].addr + off * area[0].step / 8;
	memcpy(pcm_buffer, empty, frames * a_dev->frame_size);
	snd_pcm_mmap_commit(a_dev->pcm, off, frames);

	err = snd_pcm_start(a_dev->pcm);
	if (err) {
		fprintf(stderr, "%s\n", snd_strerror(err));
	}

	free(empty);
	return 0;
}
