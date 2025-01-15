#ifndef DEFS_H
#define DEFS_H

// Native format for this application
#define SAMPLE_RATE 48000	// standard sample rate
#define BITDEPTH 16		// bits per sample
#define NUM_CHANNELS 2		// number of channels (L + R)
#define FRAME_SIZE 4		// frame size in bytes
#define NUM_PADS 8

static const int WINDOW_HEIGHT = 450;
static const int WINDOW_WIDTH = 800;
static const char* WINDOW_NAME = "SP-PLUS";

// uncomment to turn off assertions
// #define NDEBUG


#endif
