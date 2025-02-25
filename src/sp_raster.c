#include "sp_raster.h"
#include "sp_plus_assert.h"

#include "stb_truetype.h"

#include <stdio.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Core */

// returns true if vector is on screen
static inline int vec2i_on_screen(const struct pixel_buffer *pb, vec2i v)
{ return v.x >= 0 && v.x < pb->width && v.y >= 0 && v.y < pb->height; }

// TODO maybe a bit too clever
// TODO consolidate into the set_pixel function
static Color blend_pixel(Color bot, Color top)
{
	Color a, r, g, b;
	
	a = (top & A_MASK) >> 24;

	r = (bot & R_MASK) >> 16;
	r = r * (255  - a) + ((top & R_MASK) >> 16) * a;
	r /= 255;

	g = (bot & G_MASK) >> 8;
	g = g * (255  - a) + ((top & G_MASK) >> 8) * a;
	g /= 255;

	b = bot & B_MASK;
	b = b * (255  - a) + (top & B_MASK) * a;
	b /= 255;

	return A_MASK | r << 16 | g << 8 | b;
}

static inline Color get_pixel(const struct pixel_buffer *buffer, vec2i v)
{
	ASSERT(vec2i_on_screen(buffer, v));
	int i = v.y * buffer->width + v.x;
	return ((Color *) buffer->buffer)[i];

}


// expects pixels in range
static inline void set_pixel(const struct pixel_buffer *buffer, vec2i v, Color c)
{
	ASSERT(vec2i_on_screen(buffer, v));
	int i = v.y * buffer->width + v.x;
	((Color *) (buffer->buffer)) [i] = c;
}

static void 
octant0(const struct pixel_buffer *buffer, vec2i v, int dx, int dy, int xdir, Color c)
{
	// init error values
	const int dyX2 = dy * 2;
	const int dyX2_minus_dxX2 = dyX2 - dx * 2;
	int error_term = dyX2 - dx;

	set_pixel(buffer, v, c);
	while (dx--) {
		// increment y if error_term >=0 and adjust error_term back down
		if (error_term >= 0) {
			v.y++;
			error_term += dyX2_minus_dxX2;
		} else {
			error_term += dyX2;
		}
		v.x += xdir;
		set_pixel(buffer, v, c);
	}
}

static void 
octant1(const struct pixel_buffer *buffer, vec2i v, int dx, int dy, int xdir, Color c)
{
	// init error values
	const int dxX2 = dx * 2;
	const int dxX2_minus_dyX2 = dxX2 - dy * 2;
	int error_term = dxX2 - dy;

	set_pixel(buffer, v, c);
	while (dy--) {
		// increment y if error_term >=0 and adjust error_term back down
		if (error_term >= 0) {
			v.x += xdir;
			error_term += dxX2_minus_dyX2;
		} else {
			error_term += dxX2;
		}
		v.y++;
		set_pixel(buffer, v, c);
	}
}

/* API */
// clear pixel buffer to black
void clear_pixel_buffer(const struct pixel_buffer *buffer)
{
	memset(buffer->buffer, 0, buffer->width * buffer->height * buffer->pixel_size);
}

void fill_pixel_buffer(const struct pixel_buffer *buffer, Color c)
{
	uint64_t p = ((uint64_t) c) << 32 | c;
	int num_p = buffer->width * buffer->height;
	int i;
	for (i = 0; i < num_p - 1; i += 2)
		*((uint64_t *) ((uint32_t *) buffer->buffer + i)) = p;

	if (i = num_p + 1)
		((uint32_t *) (buffer->buffer))[i - 2] = c;
}

// TODO Currently this should not be used. I do not want to scale based on screen size
// just cut off the drawings that hit the edge of the screen
//
// transforms vec2i from internal coordinates to pixel_buffer coordinates
static inline vec2i transform_vec2i(const struct pixel_buffer *buffer, vec2i v)
{
	ASSERT(v.x >= 0 && v.x < SCRN_W && v.y >= 0 && v.y < SCRN_H);
	return (vec2i) { roundf((float) v.x / SCRN_W * buffer->width), roundf((float) v.y / SCRN_H * buffer->height)};
}

// TODO special case for horizontal line
// TODO no bounds processing on vectors as screen is locked at 1080
void draw_line(const struct pixel_buffer *buffer, vec2i start, vec2i end, Color c)
{
	/*
	   start = transform_vec2i(buffer, start);
	   end = transform_vec2i(buffer, end);
	   */


	// Skip half the cases by ensuring y1 > y0
	if (start.y > end.y) {
		const vec2i temp = start;
		start = end;
		end = temp;
	}

	int dx = end.x - start.x;
	const int dy = end.y - start.y;

	// octant 0 and 1 
	if (dx > 0) {
		if (dx > dy) {
			octant0(buffer, start, dx, dy, 1, c);
		} else {
			octant1(buffer, start, dx, dy, 1, c);
		}
	} else {
		// octant 2 and 3
		// take abs of dx and reverse line draw direction
		dx = -dx;
		if (dx > dy) {
			octant0(buffer, start, dx, dy, -1, c);
		} else {
			octant1(buffer, start, dx, dy, -1, c);
		}
	}

}



void draw_rec_outline(const struct pixel_buffer *buffer, vec2i start, int width, int height, Color c)
{
	const vec2i p2 = {start.x + width - 1, start.y};
	const vec2i p3 = {start.x + width - 1, start.y + height - 1};
	const vec2i p4 = {start.x , start.y + height - 1};

	draw_line(buffer, start, p2, c);
	draw_line(buffer, p2, p3, c);
	draw_line(buffer, p3, p4, c);
	draw_line(buffer, p4, start, c);
}

void draw_rec(const struct pixel_buffer *buffer, vec2i start, int width, int height, Color c)
{
	const vec2i p2 = {start.x + width - 1, start.y};
	for (int i = 0; i < height; i++) {
		const vec2i s = {start.x, start.y + i};
		const vec2i e = {p2.x, p2.y + i};
		draw_line(buffer, s, e, c);
	}
}


/////////////////////////////////////////////////////////////////////////////////////
///
/// Text stuff
///

// font_bitmap array extracts ASCII chars from SPACE to '~'
#define FIRST_ASCII_VAL 32
#define LAST_ASCII_VAL 126
#define NUM_GLYPHS 94
#define FONT_SIZE 100
void load_font(struct font *font, void *ttf_buffer, int pix_height)
{
	// TODO save handle to ttf file in case we want to load a a new font
	// load .ttf file
	ASSERT(ttf_buffer);

	stbtt_fontinfo font_info;
	stbtt_InitFont(&font_info, ttf_buffer, stbtt_GetFontOffsetForIndex(ttf_buffer,0));

	font->height = pix_height;
	font->glyphs = calloc(NUM_GLYPHS, sizeof(struct glyph));
	if (!font->glyphs) {
		fprintf(stderr, "Error allocating font\n");
		exit(1);
	}

	// populate font bitmap
	// skip SPACE codepoint and fill in manually last
	struct glyph *glyph = font->glyphs + 1;
	for (unsigned char c = FIRST_ASCII_VAL + 1; c <= LAST_ASCII_VAL; c++) {
		glyph->bitmap = stbtt_GetCodepointBitmap(
				&font_info, 0, 
				stbtt_ScaleForPixelHeight(&font_info, pix_height), 
				c, &glyph->w, &glyph->h, &glyph->x_off, &glyph->y_off);
		glyph++;
	}
	font->glyphs->x_off = (font->glyphs + '0' - FIRST_ASCII_VAL)->w;
}

void draw_text(
		const struct pixel_buffer *pix_buff, 
		const char *text, const struct font *font, 
		vec2i pos, Color color)
{
	// pos = transform_vec2i(pix_buff, pos);
	// real pixel per internal pixel
	//
	// TODO remove when scaled function is removed
	// float scale = (float) pixel_height / FONT_SIZE;

	pos.y += font->height;
	int origin_x = pos.x;
	const char *c = text;

	// iterate through text
	while (*c != '\0') {
		const struct glyph *g = font->glyphs + *c - FIRST_ASCII_VAL;
		int x_off = g->x_off;

		for (int bitmap_x = 0; bitmap_x < g->w; bitmap_x++) {
			int x_pos = origin_x + bitmap_x + g->x_off;
			x_off = g->x_off;

			if (x_pos < 0) continue; 
			if (x_pos >= pix_buff->width) break;

			for (int bitmap_y = 0; bitmap_y < g->h; bitmap_y++) {
				unsigned char curr_pix = g->bitmap[bitmap_y * g->w + bitmap_x];
				if(curr_pix) {
					int y_pos = pos.y + bitmap_y + g->y_off;

					if (y_pos < 0) continue;
					if (y_pos >= pix_buff->height) break;

					// align text to starting x position
					if (c == text) 
						x_off = 0;
					vec2i pixel_pos = {origin_x + bitmap_x + x_off, pos.y + bitmap_y + g->y_off};

					unsigned char alpha = (((color & A_MASK) >> 24) * curr_pix) /  255;

					Color new_pixel = (color & (~A_MASK)) | (alpha << 24);
					new_pixel = blend_pixel(get_pixel(pix_buff, pixel_pos), new_pixel); 
					set_pixel(pix_buff, pixel_pos, new_pixel);
				}
			}
		}
		origin_x += g->w + x_off;
		c++;
	}
}
