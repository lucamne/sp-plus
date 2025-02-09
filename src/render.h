#ifndef RENDER_H
#define RENDER_H

#include <stdint.h>

/* Constants */
// NOTE: rendering will be done at 1080p and mapped to actual window size
// coordinate ranges - x:[0, SCRN_W - 1], y:[0, SCRN_H - 1]
static const int SCRN_W = 1920;
static const int SCRN_H = 1080;

/* Colors */
#define WHITE 	0xFFFFFFFF
#define BLACK 	0xFF000000
#define RED 	0xFFB82626
#define GREEN 	0xFF247316
// Soft Sand palette


/* Types */
struct pixel_buffer {
	char *buffer;				// pixel buffer
	int pixel_size;				// pixel size in bytes

	int width;				// screen width in pixels
	int height;				// screen height in pixels
	
	float width_ratio;			// ratio of buffer width to SCRN_W
	float height_ratio;			// ratio of buffer width to SCRN_W
};

typedef struct vec2i {
	int x;
	int y;
} vec2i;

typedef int32_t color;

void clear_pixel_buffer(const struct pixel_buffer *buffer);
void fill_pixel_buffer(const struct pixel_buffer *buffer, color c);
void draw_line(const struct pixel_buffer *buffer, vec2i start, vec2i end, color c);
void draw_rec_outline(const struct pixel_buffer *buffer, vec2i start, int width, int height, color c);
void draw_rec(const struct pixel_buffer *buffer, vec2i start, int width, int height, color c);
void draw_text(const struct pixel_buffer *buffer, const char *txt, int num_chars, vec2i origin, int font_size, color c);

#endif
