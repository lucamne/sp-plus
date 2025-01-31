
static void print_wav(const struct wav_file* w)
{
	printf(	
			"***********************\n"
			"Wav Info:\n"
			"-----------------------\n"
			"path: %s\n"
			"channels: %d\n"
			"sample rate: %dHz\n"
			"bytes per sec: %d\n"
			"bitdepth: %d\n"
			"num samples: %d\n"
			"size: %.1fkB\n"
			"***********************\n",
			w->path,
			w->num_channels,
			w->sample_rate,
			w->bytes_per_sec,
			w->bit_depth,
			w->num_samples,
			((float) w->data_size) / 1000.0f);
}

// log read error and free state ptrs
static void wav_read_error(FILE* f) 
{
	fprintf(stderr, "Error reading wav\n");
	fclose(f);
}

// check endianess of system at runtime
static bool is_little_endian(void)
{
	int n = 1;
	if(*(char *)&n == 1) 
		return true;
	return false;
}

// load a wav file from disk
int load_wav(struct wav_file* wav, const char* path)
{

	wav->path = path;
	FILE* f = fopen(path, "r");
	if (!f) {
		fprintf(stderr, "Could not open file: %s\n", path);
		return 1;
	}

	int32_t buffer[12];

	// read RIFF, WAVE, and fmt headers assuming they exist
	if (fread(buffer, sizeof(*buffer), 5, f) != 5) {
		wav_read_error(f);
		return 1;
	}
	if (buffer[0] ^ 0x46464952) {		// check for 'RIFF' tag
		wav_read_error(f);
		return 1;
	}
	/* Spec does not require WAVE tag here
	 * For now I am checking to reduce scope */
	if (buffer[2] ^ 0x45564157) {		// check for 'WAVE' tag
		wav_read_error(f);
		return 1;
	}
	if (buffer[3] ^ 0x20746D66) {		// check for 'fmt ' tag
		wav_read_error(f);
		return 1;
	}
	wav->fmt_ck_size = buffer[4];
	// support  only non-extended PCM for now
	if (wav->fmt_ck_size != 16) {
		wav_read_error(f);
		return 1;
	}

	// read fmt chunk
	if (fread(buffer, 1, wav->fmt_ck_size, f) != (size_t) wav->fmt_ck_size) {
		wav_read_error(f);
		return 1;
	}
	wav->format = buffer[0] & 0xFFFF;		
	if (wav->format != 1) {			// support only PCM for now
		wav_read_error(f);
		return 1;
	}
	wav->num_channels = buffer[0] >> 16;	
	wav->sample_rate = buffer[1];
	wav->bytes_per_sec = buffer[2];
	wav->frame_size = buffer[3] & 0xFFFF;
	wav->bit_depth = buffer[3] >> 16;
	if (wav->bit_depth != 16)
		fprintf(stderr, "Warning bitdepth is %d\n", wav->bit_depth);

	// read data chunk assuming no other chunks come next (ie. fact)
	if (fread(buffer, sizeof(*buffer), 2, f) != 2) {
		wav_read_error(f);
		return 1;
	}
	// seek to data chunk
	while (buffer[0] ^ 0x61746164) { 		
		if (fseek(f, buffer[1], SEEK_CUR)) {
			wav_read_error(f);
			return 1;
		}
		if (fread(buffer, sizeof(*buffer), 2, f) != 2) {
			wav_read_error(f);
			return 1;
		}
	}
	wav->data_size = buffer[1];
	wav->num_samples = wav->data_size / wav->frame_size;
	wav->data = malloc(wav->data_size);
	if (!wav->data) {
		wav_read_error(f);
		return 1;
	}
	if (fread(wav->data, 1, wav->data_size, f) != (size_t) wav->data_size) {
		wav_read_error(f);
		return 1;
	}
	fclose(f);

	print_wav(wav);
	return 0;
}





// read wav file into provided sample
int load_wav_into_sample(struct sample* s, const char* path)
{
	// zero out value which will not be reset
	// watch for memory leak with data
	s->data = NULL;
	s->playing = false;
	s->speed = 1.0f;
	s->loop_mode = LOOP_OFF;
	s->gate_closed = false;

	struct wav_file w = {0};
	if (load_wav(&w, path)) return 1;

	// only deal with mono and stereo files for now
	if (w.num_channels != 2 && w.num_channels != 1) {
		printf("%d channel(s) not supported\n", w.num_channels);
		return 1;
	}
	// convert to stereo if necessary
	if (w.num_channels == 1) {
		int16_t* new_data = calloc(2, w.data_size);
		for (int i = 0; i < w.num_samples; i++) {
			new_data[2 * i] = w.data[i];
			new_data[2 * i + 1] = w.data[i];
		}
		free(w.data);
		w.data = new_data;
		w.data_size *= 2;
		w.frame_size *= 2;
	}

	s->num_frames = w.num_samples;
	s->frame_size = w.frame_size;
	s->rate = w.sample_rate;

	// convert data from float to double
	assert(sizeof(double) >= 2);
	s->data = realloc(s->data, sizeof(double) * s->num_frames * NUM_CHANNELS);
	for (int i = 0; i < s->num_frames; i++) {
		// convert left channel
		double f = ((double) w.data[NUM_CHANNELS * i]) / 32768.0;
		if (f > 1.0) f = 1.0;
		else if (f < -1.0) f = -1.0;
		s->data[NUM_CHANNELS * i] = f;
		// convert right channel
		f = ((double) w.data[NUM_CHANNELS * i + 1]) / 32768.0;
		if (f > 1.0) f = 1.0;
		else if (f < -1.0) f = -1.0;
		s->data[NUM_CHANNELS * i + 1] = f;
	}
	// resample if necessary
	if (s->rate != SAMPLE_RATE) {
		const int r = resample(s, s->rate, SAMPLE_RATE);
		if (r == -1) {
			fprintf(stderr, "Resampling Error\n");
			return 1;
		}
		s->num_frames = r;
		s->rate = SAMPLE_RATE;
	}
	// initiliaze here because data may have been reallocated
	s->next_frame = 0;
	s->start_frame = 0;
	s->end_frame = s->num_frames;
	s->path = malloc(sizeof(char) * (strlen(path) + 1));
	strcpy(s->path, path);

	free(w.data);

	print_sample(s);
	return 0;
}
