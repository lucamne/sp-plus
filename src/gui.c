#include "defs.h"
#include "system.h"

#include <stdlib.h>
#include <stdint.h>

static void draw_sample_view(const struct system* sys, const struct sampler* sampler)
{
	static const int MAX_POINTS = 4000;		// max points to render
	static const float WAVE_WIDTH = 580.0f;		// width of wave spline 
	static const float WAVE_HEIGHT = 280.0f;	// max height of wave spline
	static const Vector2 ORIGIN = {20.0f, 160.0f};	// origin of wave spline
	static const float THICKNESS = 2.0f;		// wave spline thickness
	static const Color WAVE_COLOR = WHITE;		// color of wave spline

	// sample box
	const Rectangle rec = { 10.0f, 10.0f, 600.0f, 300.0f };
	DrawRectangleLinesEx(rec, 3.0f, WHITE);

	// draw sample pads
	const Rectangle recq = { 10.0f, 320.0f, 40.0f, 40.0f};
	if (sys->banks[0][PAD_Q].playing) DrawRectangleRounded(recq, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(recq, 0.3f, 1, WHITE);
	DrawText("Q", 15.0f, 325.0f, 20, BLACK);

	const Rectangle recw = { 60.0f, 320.0f, 40.0f, 40.0f};
	if (sys->banks[0][PAD_W].playing) DrawRectangleRounded(recw, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(recw, 0.3f, 1, WHITE);
	DrawText("W", 65.0f, 325.0f, 20, BLACK);

	const Rectangle rece = { 110.0f, 320.0f, 40.0f, 40.0f};
	if (sys->banks[0][PAD_E].playing) DrawRectangleRounded(rece, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(rece, 0.3f, 1, WHITE);
	DrawText("E", 115.0f, 325.0f, 20, BLACK);

	const Rectangle recr = { 160.0f, 320.0f, 40.0f, 40.0f};
	if (sys->banks[0][PAD_R].playing) DrawRectangleRounded(recr, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(recr, 0.3f, 1, WHITE);
	DrawText("R", 165.0f, 325.0f, 20, BLACK);

	if (!sys || !sampler || !sampler->active_sample)
		return;

	const struct sample* s = sampler->active_sample;

	// calculate zoom parameters
	const int32_t frames_to_draw = s->num_frames / sampler->zoom;
	const int32_t focused_frame = sampler->zoom_focus == END ? 
		s->end_frame : s->start_frame;

	int32_t first_frame_to_draw;
	if (focused_frame < frames_to_draw / 2)
		first_frame_to_draw = 0;
	else if (	s->num_frames - focused_frame <= 
			frames_to_draw - (frames_to_draw / 2))
		first_frame_to_draw = s->num_frames - frames_to_draw;
	else
		first_frame_to_draw = focused_frame - frames_to_draw / 2;

	// not all frames will be rendered at once
	int num_vertices;
	float frame_freq;
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
		const int32_t frame = first_frame_to_draw + i * (int) frame_freq;
		const double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		const float y = 
			(float) sum * (WAVE_HEIGHT / 2.0f) + ORIGIN.y;
		const float x = (float) i * vertex_spacing + ORIGIN.x;
		const Vector2 v = {x, y};
		vertices[i] = v;
	}
	DrawSplineLinear(vertices, num_vertices, THICKNESS, WAVE_COLOR);

	/* Draw Markers */
	if (	s->next_frame >= first_frame_to_draw && 
		s->next_frame < first_frame_to_draw + frames_to_draw) {

		const float play_x = 
			((int) (s->next_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + ORIGIN.x;
		const Vector2 startv = {play_x, ORIGIN.y - WAVE_HEIGHT / 2.0f};
		const Vector2 endv = {play_x, ORIGIN.y + WAVE_HEIGHT / 2.0f};
		DrawLineEx(startv, endv, 2.0f, RED);
	}
	if (	s->start_frame >= first_frame_to_draw && 
		s->start_frame < first_frame_to_draw + frames_to_draw) {

		const float start_x = 
			((int) (s->start_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + ORIGIN.x;
		const Vector2 startv = {start_x, ORIGIN.y - WAVE_HEIGHT / 2.0f};
		const Vector2 endv = {start_x, ORIGIN.y + WAVE_HEIGHT / 2.0f};
		DrawLineEx(startv, endv, 2.0f, BLUE);
	}
	if (	s->end_frame >= first_frame_to_draw && 
		s->end_frame <= first_frame_to_draw + frames_to_draw) {

		const float end_x = 
			((int) (s->end_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + ORIGIN.x;
		const Vector2 startv = {end_x, ORIGIN.y - WAVE_HEIGHT / 2.0f};
		const Vector2 endv = {end_x, ORIGIN.y + WAVE_HEIGHT / 2.0f};
		DrawLineEx(startv, endv, 2.0f, BLUE);
	}
}

void draw(const struct system* sys, const struct sampler* sampler)
{
	BeginDrawing();
	ClearBackground(BLACK);
	draw_sample_view(sys, sampler);
	EndDrawing();
}

