#include "defs.h"
#include "system.h"

#include <stdlib.h>
#include <stdint.h>

static void draw_sample_view(const struct system* sys, const struct sampler* sampler)
{
	static const int MAX_POINTS = 4000;		// max points to render
	static const float WAVE_WIDTH = 770.0f;		// width of wave spline 
	static const float WAVE_HEIGHT = 400.0f;	// max height of wave spline
	static const Vector2 ORIGIN = {15.0f, 225.0f};	// origin of wave spline
	static const float THICKNESS = 2.0f;		// wave spline thickness
	static const Color WAVE_COLOR = WHITE;		// color of wave spline

	if (!sys || !sampler || !sampler->active_sample)
		return;

	const struct sample* s = sampler->active_sample;

	const int frames_to_draw = (int) (s->num_frames / sampler->zoom);
	if (frames_to_draw < MAX_POINTS) frames_to_draw = MAX_POINTS;

	int first_frame_to_draw;
	if (sampler->zoom_focus == END) {
		if (get_end(s) < frames_to_draw / 2)
	}

	int num_vertices;
	float frame_freq;
	// calculate num points and frame_freq
	if (frames_to_draw > MAX_POINTS) { 
		num_vertices = MAX_POINTS;
		frame_freq = (float) frames_to_draw / (float) MAX_POINTS;
	} else {
		num_vertices = frames_to_draw;
		frame_freq = 1.0f;
	}


	Vector2 vertices[num_vertices];
	const float vertex_spacing = WAVE_WIDTH / (float) (num_vertices);
	for (int i = 0; i < num_vertices; i++) {
		// caluclate waveform points
		const int32_t frame = i * (int) frame_freq;
		const double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		const float y = 
			(float) sum * (WAVE_HEIGHT / 2.0f) + ORIGIN.y;
		const float x = (float) i * vertex_spacing + ORIGIN.x;
		const Vector2 v = {x, y};
		vertices[i] = v;
	}
	// draw waveform
	DrawSplineLinear(vertices, num_vertices, THICKNESS, WAVE_COLOR);

	/* Draw Markers */
	
	// find marker frames
	const int32_t start_frame = (s->start_frame - s->data) / NUM_CHANNELS;
	const int32_t end_frame = (s->end_frame - s->data) / NUM_CHANNELS;
	const int32_t play_frame = (s->next_frame - s->data) / NUM_CHANNELS;
	// find marker x coordinates
	const float play_x = 
		((int) play_frame / frame_freq) * sample_width + ORIGIN.x;
	float start_x = 
		((int) start_frame / frame_freq) * sample_width + ORIGIN.x;
	float end_x = 
		((int) end_frame / frame_freq) * sample_width + ORIGIN.x;
	// draw playhead, start and end lines
	Vector2 startv = {play_x, ORIGIN.y - WAVE_HEIGHT / 2.0f};
	Vector2 endv = {play_x, ORIGIN.y + WAVE_HEIGHT / 2.0f};
	DrawLineV(startv, endv, RED);

	startv.x = start_x;
	startv.y = ORIGIN.y - WAVE_HEIGHT / 2.0f;
	endv.x = start_x;
	endv.y = ORIGIN.y + WAVE_HEIGHT / 2.0f;
	DrawLineV(startv, endv, BLUE);

	startv.x = end_x;
	startv.y = ORIGIN.y - WAVE_HEIGHT / 2.0f;
	endv.x = end_x;
	endv.y = ORIGIN.y + WAVE_HEIGHT / 2.0f;
	DrawLineV(startv, endv, BLUE);
}

void draw(const struct system* sys, const struct sampler* sampler)
{
		BeginDrawing();
		ClearBackground(BLACK);
		draw_sample_view(sys, sampler);
		EndDrawing();
}

