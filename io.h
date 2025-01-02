#include <stdint.h>

struct wav_file {
	// fmt chunk
	int32_t fmt_ck_size;
	int16_t format;
	int16_t num_channels;
	int32_t samples_per_sec;
	int32_t bytes_per_sec;
	int16_t block_size;
	int16_t bits_per_sample;
	int16_t cb_size;
	int16_t valid_bits_per_sample;
	int32_t channel_mask;
	int32_t sub_format[4];

	// data chunk
	int32_t data_ck_size;
	int16_t* data;
	int num_samples;
};

struct wav_file* load_wav(const char* path);
