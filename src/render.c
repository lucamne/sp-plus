#include "render.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Core */
static void set_pixel(const struct pixel_buffer *buffer, vec2i v, color c)
{
	if (v.x >= SCRN_H || v.x < 0 || v.y >= SCRN_H || v.y < 0) return;
	if (!buffer || !buffer->buffer) return;

	const int true_x = roundf(v.x * buffer->width_ratio);
	const int true_y = roundf(v.y * buffer->height_ratio);

	const int i = buffer->pixel_size * (true_y * buffer->width + true_x);

	memcpy(buffer->buffer + i, &c, buffer->pixel_size);
}

static void 
octant0(const struct pixel_buffer *buffer, vec2i v, int dx, int dy, int xdir, color c)
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
octant1(const struct pixel_buffer *buffer, vec2i v, int dx, int dy, int xdir, color c)
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
	if (!buffer || !buffer->buffer) return;
	memset(buffer->buffer, 0, buffer->width * buffer->height * buffer->pixel_size);
}

void fill_pixel_buffer(const struct pixel_buffer *buffer, color c)
{
	for (int i = 0; i < buffer->width * buffer->height; i++) {
		memcpy(buffer->buffer + i * buffer->pixel_size, &c, buffer->pixel_size);
	}
}

// TODO special case for horizontal line
void draw_line(const struct pixel_buffer *buffer, vec2i start, vec2i end, color c)
{
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

void draw_rec_outline(const struct pixel_buffer *buffer, vec2i start, int width, int height, color c)
{
	const vec2i p2 = {start.x + width - 1, start.y};
	const vec2i p3 = {start.x + width - 1, start.y + height - 1};
	const vec2i p4 = {start.x , start.y + height - 1};

	draw_line(buffer, start, p2, c);
	draw_line(buffer, p2, p3, c);
	draw_line(buffer, p3, p4, c);
	draw_line(buffer, p4, start, c);
}

void draw_rec(const struct pixel_buffer *buffer, vec2i start, int width, int height, color c)
{
	const vec2i p2 = {start.x + width - 1, start.y};
	for (int i = 0; i < height; i++) {
		const vec2i s = {start.x, start.y + i};
		const vec2i e = {p2.x, p2.y + i};
		draw_line(buffer, s, e, c);
	}
}

void draw_text(const struct pixel_buffer *buffer, const char *txt, int num_chars, vec2i origin, int font_size, color c)
{
}
