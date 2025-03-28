#ifndef SP_PLUS_H
#define SP_PLUS_H

#include <stdio.h>
#include <stdint.h>

// audio constants
#define NUM_CHANNELS 2
#define SAMPLE_RATE 48000

//////////////////////////////////////////////////////////////////////
/// Input Handling Types

#define NUM_KEYS 49
enum Key {
	KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I, KEY_J, KEY_K, 
	KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R, KEY_S, KEY_T, KEY_U, KEY_V,
	KEY_W, KEY_X, KEY_Y, KEY_Z, KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6,
	KEY_7, KEY_8, KEY_9, KEY_SPACE, KEY_EQUAL, KEY_MINUS, KEY_SHIFT_L, KEY_SHIFT_R, 
	KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_TAB, KEY_ENTER, KEY_BACKSPACE, KEY_ESCAPE
};
struct key_input {
	// for bitmaps rightmost bit is 0 bit
	uint64_t key_pressed;			// bitmap, was unpressed key newly pressed
	uint64_t key_released;			// bitmap, was key released this frame
	uint64_t key_down;			// bitmap, is key currently held down

	int num_key_press[NUM_KEYS];		// how many times did keypress event occur
};						// during frame

//////////////////////////////////////////////////////////////////////////
/// Platform to service calls

void *sp_plus_allocate_state(void);
// allocates and initializes program state

int sp_plus_fill_audio_buffer(void *sp_state, void* dest, int frames);
// service to fill audio buffer with requested number of frames
// called asynchronously

void sp_plus_update_and_render(
		void *sp_state, 
		char *pixel_buf, 
		int pixel_width,
		int pixel_height,
		int pixel_bytes,
		struct key_input* input);
// service to update program state and then fill pixel_buf with image


//////////////////////////////////////////////////////////////////////////
/// Service to platform calls

/* file loading */

long platform_load_entire_file(void **buffer, const char *path);
// Returns bytes in buffer or 0 on failure.
// load_file() will initialize buffer and free buffer on failure
// on success buffer can be freed later with free_file_buffer()

// TODO test if this really needs to be a double pointer
void platform_free_file_buffer(void **buffer);
// frees buffer passed to load file

/* directory reading */

typedef void SP_DIR;
// directory handle type

SP_DIR *platform_opendir(const char *path);
// opens a directory at path
// returns NULL on error

int platform_closedir(SP_DIR *dir);
// closes dir opened by platform_opendir
// returns 0 on success on -1 on failure

int platform_num_valid_items_in_dir(SP_DIR *dir);
// SP_DIR* returned by platform_opendir
// return number of wavs or directories not including "./" in dir
// returns -1 on error

int platform_read_next_valid_item(SP_DIR *dir, char **path, int *is_dir);
// takes SP_DIR* returned by platform_opendir and a ptr to a string
// Reads the path of the next entry into path
// this function WILL allocate path appropriately
// The caller is responsible for freeing path when they are done with it
// Returns 0 on success, 1 on no more entries, and -1 on failure

char *platform_get_realpath(const char * dir);
// resolves absolute path to real path
// allocates a text buffer large enough to hold resolved path
// caller must free this text buffer when done with it
// returns NULL on error

char *platform_get_parent_dir(const char *dir);
// returns parent dir or NULL on error

/* threading */
void *platform_init_mutex(void);
// allocates an initialized mutex or returns NULL on failure

int platform_mutex_lock(void *mutex);
// locks mutex
// if mutex is already locked by another thread 
// suspend thread until unlocked then lock
// returns 0 on success and non 0 on failure

int platform_mutex_unlock(void *mutex);
// unlock mutex locked by calling thread
// returns 0 on success and non 0 on failure

#endif
