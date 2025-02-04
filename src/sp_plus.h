#ifndef SP_PLUS_H
#define SP_PLUS_H

#include <stdio.h>
#include <stdint.h>

// audio constants
#define NUM_CHANNELS 2
#define SAMPLE_RATE 48000

// key board input types and constants
#define NUM_KEYS 26

enum Key {
	KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, 
	KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V,
	KEY_W, KEY_X, KEY_Y, KEY_Z
};

struct key_input {
	// for bitmaps rightmost bit is 0 bit
	uint32_t key_pressed;			// bitmap, was unpressed key newly pressed
	uint32_t key_released;			// bitmap, was key released this frame
	uint32_t key_down;			// bitmap, is key currently held down

	char shift_lock;			// is shift down or was capslock pressed
	int num_key_press[NUM_KEYS];		// how many times did keypress event occur
};						// during frame

/* Calls from platform to sp_plus service */

// allocates and initializes program state
void *sp_plus_allocate_state(void);

// service to fill audio buffer with requested number of frames
// called asynchronously
int sp_plus_fill_audio_buffer(void *sp_state, void* dest, int frames);

// service to update program state and then fill pixel_buf with image
void sp_plus_update_and_render(
		void *sp_state, 
		char *pixel_buf, 
		int pixel_width,
		int pixel_height,
		int pixel_bytes,
		struct key_input* input);


/* Calls from sp_plus service to platform */

// Returns bytes in buffer or 0 on failure.
// load_file() will initialize buffer and free buffer on failure
// on success buffer can be freed later with free_file_buffer()
long platform_load_entire_file(void **buffer, const char *path);
// frees buffer passed to load file
void platform_free_file_buffer(void **buffer);

#endif
