#include <stdint.h>

// Handling .wav files
struct wav_file {
	// fmt chunk
	int32_t fmt_ck_size;		// fmt chunk size in bytes
	int16_t format;			// audio format tag
	int16_t num_channels;
	int32_t sample_rate;
	int32_t bytes_per_sec;
	int16_t block_size;		// num_channels*bits_per_sample/8
	int16_t bit_depth;
	int16_t cb_size;		// for extended formats
	int16_t valid_bits_per_sample;
	int32_t channel_mask;
	int32_t sub_format[4];

	// data chunk
	int32_t data_ck_size;		// data size in bytes
	int64_t* data;			// raw data (little endian)
	int num_samples;		// data_ck_size / block_size
};

struct wav_file* load_wav(const char* path);

void print_wav(const struct wav_file* w);

// assumes 22050hz, 16bit, 2 channel
void play_clip (void* clip, int clip_size);
