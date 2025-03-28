#ifndef RASTER_H
#define RASTER_H

#include "sp_types.h"

#include <stdint.h>

//////////////////////////////////////////////////////////
/// Constants and Types
static const int SCRN_W = 1920;
static const int SCRN_H = 1080;

typedef struct vec2i {
	int x;
	int y;
} vec2i;

typedef uint32_t Color;

// Colors
// TODO: ARGB format may be platform specific
#define WHITE 	0xFFFFFFFF
#define BLACK 	0xFF000000
#define RED 	0xFFB82626
#define GREEN 	0xFF247316
#define ORANGE	0xFFFF4D00
#define BLUE	0xFF000080

#define A_MASK 	0xFF000000
#define R_MASK 	0x00FF0000
#define G_MASK 	0x0000FF00
#define B_MASK 	0x000000FF


//////////////////////////////////////////////////////////////////
/// Data
///
// render.c draws to this buffer which is passed by platform
struct pixel_buffer {
	char *buffer;				// pixel buffer
	int pixel_size;				// pixel size in bytes

	int width;				// screen width in pixels
	int height;				// screen height in pixels
};


/////////////////////////////////////////////////////////////////
/// Clear buffer

void clear_pixel_buffer(const struct pixel_buffer *buffer);
// sets all pixels in buffer to 0
void fill_pixel_buffer(const struct pixel_buffer *buffer, Color c);
// sets all pixels in buffer to Color c

/////////////////////////////////////////////////////////////////
/// Draw line and shapes

void draw_line(const struct pixel_buffer *buffer, vec2i start, vec2i end, Color c);
// draw line from start to end of Color c
// Vectors must be in range ([0, SCRN_W - 1], [0, SCRN_H - 1])

void draw_rec_outline(const struct pixel_buffer *buffer, vec2i start, int width, int height, Color c);
// draw rectangle outline with color c

void draw_rec(const struct pixel_buffer *buffer, vec2i start, int width, int height, Color c);
// draw filled in rectange of color c

/////////////////////////////////////////////////////////////////
/// Draw Text

void load_font(struct font *font, void *ttf_buffer, int pix_height);
// Given a buffer containing .ttf file binary data and desired font height in pixels
// Populates a font struct which can be passed to draw text

void draw_ntext(const struct pixel_buffer *pix_buff, const char *text, int n, const struct font *font, vec2i pos, Color c);
// draws n characters of text from top left (not super exact)
// length of text must be <= n

void draw_text(const struct pixel_buffer *pix_buff, const char *text, const struct font *font, vec2i pos, Color c);
// draws text from top left (not super exact)
// text must be null terminated

int get_ntext_width (const char *text, int n, const struct font *font);
// returns the width of text in pixels of length n

int get_text_width (const char *text, const struct font *font);
// returns the width of text in pixels
// text must be null terminated

char *truncate_text_to_width(const char *text, const struct font *font, int max_width, int trunc_start);
// truncates text to fit within pixel width 
// if trunc start is true then the beginning is truncated, otherwise the end is truncated
// allocates new char array that the user is responsible for freeing
// returns NULL on error or new string length of 0
#endif
