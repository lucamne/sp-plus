#include "render.h"
#include "sp_plus_assert.h"

#include "stb_truetype.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Core */
static inline void set_pixel(const struct pixel_buffer *buffer, vec2i v, color c)
{
	const int i = v.y * buffer->width + v.x;
	((color *) (buffer->buffer)) [i] = c;
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
	memset(buffer->buffer, 0, buffer->width * buffer->height * buffer->pixel_size);
}

void fill_pixel_buffer(const struct pixel_buffer *buffer, color c)
{
	uint64_t p = ((uint64_t) c) << 32 | c;
	int num_p = buffer->width * buffer->height;
	int i;
	for (i = 0; i < num_p - 1; i += 2)
		*((uint64_t *) ((uint32_t *) buffer->buffer + i)) = p;

	if (i = num_p + 1)
		((uint32_t *) (buffer->buffer))[i - 2] = c;
}

// transforms vec2i from internal coordinates to pixel_buffer coordinates
static inline vec2i transform_vec2i(const struct pixel_buffer *buffer, vec2i v)
{
	ASSERT(v.x >= 0 && v.x < SCRN_W && v.y >= 0 && v.y < SCRN_H);
	return (vec2i) { roundf((float) v.x / SCRN_W * buffer->width), roundf((float) v.y / SCRN_H * buffer->height)};
}

// TODO special case for horizontal line
void draw_line(const struct pixel_buffer *buffer, vec2i start, vec2i end, color c)
{
	start = transform_vec2i(buffer, start);
	end = transform_vec2i(buffer, end);

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
