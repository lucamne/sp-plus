#include "render.h"

#include <math.h>
#include <string.h>
#include <stdlib.h>

/* Core */
// set pixel at v to white
static void set_pixel(const struct pixel_buffer *buffer, vec2i v, color c)
{
	if (v.x >= SCRN_H || v.x < 0 || v.y >= SCRN_H || v.y < 0) return;
	if (!buffer || !buffer->buffer) return;

	const int true_x = round((double) v.x / SCRN_W * buffer->width);
	const int true_y = round((double) v.y / SCRN_H * buffer->height);

	const int i = buffer->pixel_size * (true_y * buffer->width + true_x);

	memcpy(buffer->buffer + i, &c, buffer->pixel_size);
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


// TODO can make more effecient if straight line
void draw_line(const struct pixel_buffer *buffer, vec2i start, vec2i end, color c)
{
	const int dx = start.x - end.x;
	const int dy = start.y - end.y;

	const int steps = abs(dx) > abs(dy) ? abs(dx) : abs(dy); 

	const float x_inc = (float) dx / steps;
	const float y_inc = (float) dy / steps;

	for (int i = 0; i < steps; i++) {
		const vec2i p = {start.x - roundf(x_inc * i), start.y - roundf(y_inc * i)};
		set_pixel(buffer, p, c);
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
