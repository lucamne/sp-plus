#ifndef RENDER_H
#define RENDER_H

#include "sp_types.h"

#include <stdint.h>

/* Constants */
static const int SCRN_W = 1920;
static const int SCRN_H = 1080;

/* Colors */
// TODO: ARGB format may be platform specific
#define WHITE 	0xFFFFFFFF
#define BLACK 	0xFF000000
#define RED 	0xFFB82626
#define GREEN 	0xFF247316

#define A_MASK 	0xFF000000
#define R_MASK 	0x00FF0000
#define G_MASK 	0x0000FF00
#define B_MASK 	0x000000FF


/* Types */
struct pixel_buffer {
	char *buffer;				// pixel buffer
	int pixel_size;				// pixel size in bytes

	int width;				// screen width in pixels
	int height;				// screen height in pixels
};

typedef struct vec2i {
	int x;
	int y;
} vec2i;

typedef uint32_t Color;

void clear_pixel_buffer(const struct pixel_buffer *buffer);
void fill_pixel_buffer(const struct pixel_buffer *buffer, Color c);
void draw_line(const struct pixel_buffer *buffer, vec2i start, vec2i end, Color c);

// TODO revisit rec drawing later
void draw_rec_outline(const struct pixel_buffer *buffer, vec2i start, int width, int height, Color c);
void draw_rec(const struct pixel_buffer *buffer, vec2i start, int width, int height, Color c);

/* text stuff */
void load_font(struct font *font, void *ttf_buffer, int pix_height);
// draws text from top left (not super exact)
void draw_text(const struct pixel_buffer *pix_buff, const char *text, const struct font *font, vec2i pos, Color c);

#endif
