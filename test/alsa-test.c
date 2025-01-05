#include <alsa/asoundlib.h>
#include <assert.h>
#include <stdio.h>


void select_device(char* buf, int buf_size)
{
	int dev_list_size = 3;
	int dev_count = 0;
	char** dev_list = malloc(sizeof(char*) * dev_list_size);

	int playback = 1;
	int icard = -1;
	while (1) {
		// Get next sound card
		assert(0 == snd_card_next(&icard));
		if (icard == -1)
			break;

		// Open sound card handler
		char scard[32];
		snprintf(scard, sizeof(scard), "hw:%u", icard);
		snd_ctl_t *sctl = NULL;
		if (0 != snd_ctl_open(&sctl, scard, 0))
			continue;

		// Get sound card info
		snd_ctl_card_info_t *scinfo = NULL;
		assert(0 == snd_ctl_card_info_malloc(&scinfo));
		assert(0 == snd_ctl_card_info(sctl, scinfo));
		const char *soundcard_name = snd_ctl_card_info_get_name(scinfo);
		snd_ctl_card_info_free(scinfo);

		int idev = -1;
		while (1) {

			// Get next device
			if (0 != snd_ctl_pcm_next_device(sctl, &idev)
				|| idev == -1)
				break;

			// We can use this 'device_id' to assign audio buffer to this specific device
			char* dev_id = malloc(sizeof(char) * 64);

			snprintf(dev_id, sizeof(char) * 64, "plughw:%u,%u", icard, idev);
			// double size of device list if max is reached
			dev_list[dev_count++] = dev_id;
			if (dev_count >= dev_list_size) {
				dev_list_size *= 2;
				dev_list = realloc(dev_list, sizeof(char*) * dev_list_size);
			}

			// Get device info
			snd_pcm_info_t *pcminfo;
			snd_pcm_info_alloca(&pcminfo);
			snd_pcm_info_set_device(pcminfo, idev);

			// Get device properties
			if (0 != snd_ctl_pcm_info(sctl, pcminfo))
				break;


			const char *device_name = snd_pcm_info_get_name(pcminfo);
			printf("[%d] Device: %s: %s: %s\n", dev_count, soundcard_name, device_name, dev_id);
		}
		snd_ctl_close(sctl);
	}

	int selection = -1;
	printf("Select device: ");
	while (1) {
		char buf[5];
		fgets(buf, sizeof(buf), stdin);
		// extract device selection
		const int o = sscanf(buf, " %d", &selection);
		if (o == 0 || o == EOF || selection >= dev_count || selection < 1)
			printf("Enter valid device: ");
		else 
			break;
	}
	strncpy(buf, dev_list[selection - 1], buf_size);

	for (int i = 0; i < dev_count; i++)
		free(dev_list[i]);
	free(dev_list);
}


int main(void)
{
	char dev_id[64] = "";
	select_device(dev_id, 64);
	
	printf("Selected device: %s\n", dev_id);

	return 0;
}
