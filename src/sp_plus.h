#ifndef SP_PLUS_H
#define SP_PLUS_H

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



#endif
