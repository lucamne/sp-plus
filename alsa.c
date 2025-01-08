#include "io.h"
#include "defs.h"

#include <stdlib.h>

static unsigned int period_time = 100000;	// period time in usec
static unsigned int buffer_time = 500000;	// buffer time in usec

static int set_hwparams(struct alsa_dev* a_dev, snd_pcm_hw_params_t* params)
{
	snd_pcm_t* pcm = a_dev->pcm;
	int err, dir;

	err = snd_pcm_hw_params_any(pcm, params);
	if (err < 0) {
		printf("Broken configuration for playback:"
				"no configurations available: %s\n",
				snd_strerror(err));
		return err;
	}
	/* set hardware resampling */
	err = snd_pcm_hw_params_set_rate_resample(pcm, params, 1);
	if (err < 0) {
		printf("Resampling setup failed for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	/* set the interleaved read/write format */
	err = snd_pcm_hw_params_set_access(pcm, params, 
			SND_PCM_ACCESS_MMAP_INTERLEAVED);
	if (err < 0) {
		printf("Access type not available for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	/* set the sample format */
	err = snd_pcm_hw_params_set_format(pcm, params, SND_PCM_FORMAT_S16_LE);
	if (err < 0) {
		printf("Sample format not available for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	/* set the count of channels */
	err = snd_pcm_hw_params_set_channels(pcm, params, a_dev->num_channels);
	if (err < 0) {
		printf("Channels count (%u) not available for playbacks: %s\n",
				a_dev->num_channels, snd_strerror(err));
		return err;
	}
	/* set the stream rate */
	int rrate = a_dev->rate;
	err = snd_pcm_hw_params_set_rate_near(pcm, params, &rrate, 0);
	if (err < 0) {
		printf("Rate %uHz not available for playback: %s\n",
				a_dev->rate, snd_strerror(err));
		return err;
	}
	if (rrate != a_dev->rate) {
		printf("Rate doesn't match (requested %uHz, get %iHz)\n", 
				a_dev->rate, err);
		return -EINVAL;
	}
	/* set the buffer time */
	err = snd_pcm_hw_params_set_buffer_time_near(
			pcm, params, 
			&buffer_time, &dir);
	if (err < 0) {
		printf("Unable to set buffer time %u for playback: %s\n",
				buffer_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_buffer_size(params, &(a_dev->buffer_size));
	if (err < 0) {
		printf("Unable to get buffer size for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	/* set the period time */
	err = snd_pcm_hw_params_set_period_time_near(
			pcm, params, &period_time, &dir);
	if (err < 0) {
		printf("Unable to set period time %u for playback: %s\n",
				period_time, snd_strerror(err));
		return err;
	}
	err = snd_pcm_hw_params_get_period_size(
			params, &(a_dev->period_size), &dir);
	if (err < 0) {
		printf("Unable to get period size for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	/* write the parameters to device */
	err = snd_pcm_hw_params(pcm, params);
	if (err < 0) {
		printf("Unable to set hw params for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	return 0;
}

static int set_swparams(struct alsa_dev* a_dev, snd_pcm_sw_params_t* params)
{
	int err;
	snd_pcm_t* pcm = a_dev->pcm;
	/* get the current swparams */
	err = snd_pcm_sw_params_current(pcm, params);
	if (err < 0) {
		printf("Unable to determine current swparams for playback:%s\n",
				snd_strerror(err));
		return err;
	}
	/* start the transfer when the buffer is almost full: */
	/* (buffer_size / avail_min) * avail_min */
	err = snd_pcm_sw_params_set_start_threshold(
			pcm, params,
			(a_dev->buffer_size / a_dev->period_size) * 
			a_dev->period_size);
	if (err < 0) {
		printf("Unable to set start threshold mode for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	/* allow the transfer when at least period_size samples can be processed */
	/* or disable this mechanism when period event is enabled (aka interrupt like style processing) */
	err = snd_pcm_sw_params_set_avail_min(
			pcm, params, a_dev->period_size);
	if (err < 0) {
		printf("Unable to set avail min for playback: %s\n", 
				snd_strerror(err));
		return err;
	}
	/* enable period events when requested */
	if (0) {
		err = snd_pcm_sw_params_set_period_event(pcm, params, 1);
		if (err < 0) {
			printf("Unable to set period event: %s\n", 
					snd_strerror(err));
			return err;
		}
	}
	/* write the parameters to the playback device */
	err = snd_pcm_sw_params(pcm, params);
	if (err < 0) {
		printf("Unable to set sw params for playback: %s\n",
				snd_strerror(err));
		return err;
	}
	return 0;
}

static int xrun_recovery(snd_pcm_t* pcm, int err)
{
	/* under-run */
	if (err == -EPIPE) {    
		err = snd_pcm_prepare(pcm);
		if (err < 0)
			printf("Can't recovery from underrun,"
					"prepare failed: %s\n",
					snd_strerror(err));
		return 0;
	} else if (err == -ESTRPIPE) {
		while ((err = snd_pcm_resume(pcm)) == -EAGAIN)
			sleep(1);   /* wait until the suspend flag is released */
		if (err < 0) {
			err = snd_pcm_prepare(pcm);
			if (err < 0)
				printf("Can't recovery from suspend,"
						"prepare failed: %s\n",
						snd_strerror(err));
		}
		return 0;
	}
	return err;
}

struct async_private_data {
	int frame_size;
	snd_pcm_uframes_t period_size;
	struct bus* master;
};

static void async_callback(snd_async_handler_t *ahandler)
{
	snd_pcm_t *handle = snd_async_handler_get_pcm(ahandler);
	struct async_private_data *data = 
		snd_async_handler_get_callback_private(ahandler);
	const snd_pcm_channel_area_t *my_areas;
	snd_pcm_uframes_t offset, frames, size;
	snd_pcm_sframes_t avail, commitres;
	snd_pcm_state_t state;
	int first = 0;
	int err;

	while (1) {
		state = snd_pcm_state(handle);
		if (state == SND_PCM_STATE_XRUN) {
			err = xrun_recovery(handle, -EPIPE);
			if (err < 0) {
				printf("XRUN recovery failed: %s\n",
						snd_strerror(err));
				exit(EXIT_FAILURE);
			}
			first = 1;
		} else if (state == SND_PCM_STATE_SUSPENDED) {
			err = xrun_recovery(handle, -ESTRPIPE);
			if (err < 0) {
				printf("SUSPEND recovery failed: %s\n",
						snd_strerror(err));
				exit(EXIT_FAILURE);
			}
		}
		avail = snd_pcm_avail_update(handle);
		if (avail < 0) {
			err = xrun_recovery(handle, avail);
			if (err < 0) {
				printf("avail update failed: %s\n",
						snd_strerror(err));
				exit(EXIT_FAILURE);
			}
			first = 1;
			continue;
		}
		if (avail < (long int) data->period_size) {
			if (first) {
				first = 0;
				err = snd_pcm_start(handle);
				if (err < 0) {
					printf("Start error: %s\n", 
							snd_strerror(err));
					exit(EXIT_FAILURE);
				}
			} else {
				break;
			}
			continue;
		}

		size = data->period_size;
		while (size > 0) {
			frames = size;

			err = snd_pcm_mmap_begin(
					handle, &my_areas, &offset, &frames);
			if (err < 0) {
				if ((err = xrun_recovery(handle, err)) < 0) {
					printf("MMAP begin avail error: %s\n",
							snd_strerror(err));
					exit(EXIT_FAILURE);
				}
				first = 1;
			}
			process_bus(	data->master,
					my_areas[0].addr + 
					offset * my_areas[0].step / 8,
					frames);

			commitres = snd_pcm_mmap_commit(handle, offset, frames);
			if (commitres < 0 || (snd_pcm_uframes_t)commitres != frames) {
				if ((err = xrun_recovery(handle, commitres >= 0 ? 
								-EPIPE : 
								commitres)) < 0) {
					printf("MMAP commit error: %s\n", 
							snd_strerror(err));
					exit(EXIT_FAILURE);
				}
				first = 1;
			}
			size -= frames;
		}
	}
}

// might need to add the unused params
static int async_loop(
		snd_pcm_t* handle, 
		struct bus* master,
		int frame_size, 
		snd_pcm_uframes_t period_size)
{
	// maybe need to free later
	struct async_private_data* data = malloc(sizeof(struct async_private_data));
	snd_async_handler_t *ahandler;
	const snd_pcm_channel_area_t *my_areas;
	snd_pcm_uframes_t offset, frames, size;
	snd_pcm_sframes_t commitres;
	int err, count;

	data->frame_size = frame_size;
	data->period_size = period_size;
	data->master = master;

	err = snd_async_add_pcm_handler(&ahandler, handle, async_callback, data);
	if (err < 0) {
		printf("Unable to register async handler\n");
		exit(EXIT_FAILURE);
	}
	for (count = 0; count < 2; count++) {
		size = period_size;
		while (size > 0) {
			frames = size;
			const int avail = snd_pcm_avail_update(handle);
			if (avail < 0) {
				err = xrun_recovery(handle, avail);
				if (err < 0) {
					printf("avail update failed: %s\n",
							snd_strerror(err));
					exit(EXIT_FAILURE);
				}
				continue;
			}

			err = snd_pcm_mmap_begin(
					handle, &my_areas, &offset, &frames);
			if (err < 0) {
				if ((err = xrun_recovery(handle, err)) < 0) {
					printf("MMAP begin avail error: %s\n",
							snd_strerror(err));
					exit(EXIT_FAILURE);
				}
			}
			// add data to bus
			process_bus(	data->master, 
					my_areas[0].addr + 
					offset * my_areas[0].step / 8, 
					frames);
			commitres = snd_pcm_mmap_commit(handle, offset, frames);
			if (commitres < 0 || 
					(snd_pcm_uframes_t)commitres != frames) {
				if ((err = xrun_recovery(handle, commitres >= 0 ? 
								-EPIPE : 
								commitres)) < 0) {

					printf("MMAP commit error: %s\n",
							snd_strerror(err));
					exit(EXIT_FAILURE);
				}
			}
			size -= frames;
		}
	}

	err = snd_pcm_start(handle);
	if (err < 0) {
		printf("Start error: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	return 0;
}

struct alsa_dev* open_alsa_dev(int r, int num_c)
{
	int err;
	struct alsa_dev* a_dev = malloc(sizeof(struct alsa_dev));

	snd_pcm_hw_params_t* hwparams;
	snd_pcm_sw_params_t* swparams;
	snd_pcm_hw_params_alloca(&hwparams);
	snd_pcm_sw_params_alloca(&swparams);

	a_dev->dev_id = "plughw:0,0";
	a_dev->num_channels = num_c;
	a_dev->rate = r;
	// init pcm handle
	err = snd_pcm_open(
			&(a_dev->pcm), a_dev->dev_id, SND_PCM_STREAM_PLAYBACK, 0);
	if (err < 0) {
		printf("Error opening PCM device: %s\n", snd_strerror(err));
		return 0;
	}
	// init hwparams
	err = set_hwparams(a_dev, hwparams);
	if (err < 0) {
		printf("Setting of hwparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	// init swparams
	err = set_swparams(a_dev, swparams);
	if (err < 0) {
		printf("Setting of swparams failed: %s\n", snd_strerror(err));
		exit(EXIT_FAILURE);
	}
	return a_dev;
}

int start_alsa_dev(struct alsa_dev* a_dev, struct bus* master)
{
	int err = snd_pcm_prepare(a_dev->pcm);
	if (err < 0) {
		printf("Failed to prepare pcm device\n");
		return 1;
	}
	err = async_loop(
			a_dev->pcm, 
			master,
			a_dev->num_channels * 2, 
			a_dev->period_size);

	if (err < 0) {
		printf("Playback failed: %s\n", snd_strerror(err));
		return 1;
	}
	return 0;
}
