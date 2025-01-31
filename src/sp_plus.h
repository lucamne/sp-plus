#ifndef SP_PLUS_H
#define SP_PLUS_H

#include <stdio.h>

// audio constants
#define NUM_CHANNELS 2
#define SAMPLE_RATE 48000

// allocates and initializes program state
void *allocate_sp_state(void);

// service to fill audio buffer with requested number of frames
// called asynchronously
int fill_audio_buffer(void *sp_state, void* dest, int frames);

// service to update program state and then fill pixel_buf with image
void update_and_render_sp_plus(
		void *sp_state, 
		char *pixel_buf, 
		int pixel_width,
		int pixel_height,
		int pixel_bytes);


/* file IO defined in platform code and called by sp_plus */
// Returns bytes in buffer or 0 on failure.
// load_file() will initialize buffer and free buffer on failure
// on success buffer can be freed later with free_file_buffer()
long load_file(void **buffer, const char *path);
// frees buffer passed to load file
void free_file_buffer(void **buffer);

void file_close(FILE* f);

#endif
