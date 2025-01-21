#include "defs.h"
#include "system.h"

#include <stdlib.h>
#include <stdint.h>

static void draw_sample_window(const struct system* sys, const Vector2* origin)
{	
	static const float WIN_WIDTH = 600;
	static const float WIN_HEIGHT = 300;
	static const float PAD_WIDTH = 40;
	static const float PAD_HEIGHT = 40;

	struct sample** banks = sys->banks;

	// window
	const Rectangle win = { origin->x, origin->y, WIN_WIDTH,  WIN_HEIGHT };
	DrawRectangleLinesEx(win, 3.0f, WHITE);

	// sample pads
	const float PAD_Y = origin->y + WIN_HEIGHT + 10;
	float PAD_X = origin->x;

	const Rectangle padq = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_Q].playing) DrawRectangleRounded(padq, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(padq, 0.3f, 1, WHITE);
	DrawText("q", PAD_X + 5, PAD_Y + 5, 20, BLACK);

	PAD_X += PAD_WIDTH + 10;
	const Rectangle padw = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_W].playing) DrawRectangleRounded(padw, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(padw, 0.3f, 1, WHITE);
	DrawText("w", PAD_X + 5, PAD_Y + 5, 20, BLACK);

	PAD_X += PAD_WIDTH + 10;
	const Rectangle pade = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_E].playing) DrawRectangleRounded(pade, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(pade, 0.3f, 1, WHITE);
	DrawText("e", PAD_X + 5, PAD_Y + 5, 20, BLACK);

	PAD_X += PAD_WIDTH + 10;
	const Rectangle padr = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_R].playing) DrawRectangleRounded(padr, 0.3f, 1, ORANGE);
	else DrawRectangleRounded(padr, 0.3f, 1, WHITE);
	DrawText("r", PAD_X + 5, PAD_Y + 5, 20, BLACK);
}

static void draw_waveform(const struct sampler* sampler, const Vector2* origin)
{
	// waveform constants
	static const int MAX_POINTS = 4000;			// max points to render
	static const float WAVE_WIDTH = 580.0f;			// width of wave spline 
	static const float WAVE_HEIGHT = 280.0f;		// max height of wave spline

	if (!sampler->active_sample) return;
	const Vector2 wave_origin = {origin->x + 10.0f, origin->y + 150.0f};
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
	// generate vertex list from sample data
	Vector2 vertices[num_vertices];
	const float vertex_spacing = WAVE_WIDTH / (float) (num_vertices);
	for (int i = 0; i < num_vertices; i++) {
		const int32_t frame = first_frame_to_draw + i * (int) frame_freq;
		const double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		const float y = 
			(float) sum * (WAVE_HEIGHT / 2.0f) + wave_origin.y;
		const float x = (float) i * vertex_spacing + wave_origin.x;
		const Vector2 v = {x, y};
		vertices[i] = v;
	}
	DrawSplineLinear(vertices, num_vertices, 2, WHITE);

	// draw markers
	if (	s->next_frame >= first_frame_to_draw && 
		s->next_frame < first_frame_to_draw + frames_to_draw) {

		const float play_x = 
			((int) (s->next_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x;
		const Vector2 startv = {play_x, wave_origin.y - WAVE_HEIGHT / 2.0f};
		const Vector2 endv = {play_x, wave_origin.y + WAVE_HEIGHT / 2.0f};
		DrawLineEx(startv, endv, 2.0f, RED);
	}
	if (	s->start_frame >= first_frame_to_draw && 
		s->start_frame < first_frame_to_draw + frames_to_draw) {

		const float start_x = 
			((int) (s->start_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x;
		const Vector2 startv = {start_x, wave_origin.y - WAVE_HEIGHT / 2.0f};
		const Vector2 endv = {start_x, wave_origin.y + WAVE_HEIGHT / 2.0f};
		DrawLineEx(startv, endv, 2.0f, BLUE);
	}
	if (	s->end_frame >= first_frame_to_draw && 
		s->end_frame <= first_frame_to_draw + frames_to_draw) {

		const float end_x = 
			((int) (s->end_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x;
		const Vector2 startv = {end_x, wave_origin.y - WAVE_HEIGHT / 2.0f};
		const Vector2 endv = {end_x, wave_origin.y + WAVE_HEIGHT / 2.0f};
		DrawLineEx(startv, endv, 2.0f, BLUE);
	}
}

static void draw_sampler_controls(const struct sampler* sampler, const Vector2* origin)
{
	if (!sampler->active_sample) return;
	const struct sample* s = sampler->active_sample;
	const float X = origin->x + 200;
	const float Y = origin->y + 300;

	Vector2 s1 = { X, Y + 5 };
	Vector2 s2 = { X, Y + 55 };
	DrawLineEx( s1, s2, 2.0f, WHITE);
	// gate
	DrawText("(g)ate", X + 10, Y + 10, 20, WHITE);
	if (s->gate) DrawRectangle(X + 125, Y + 10, 20.0f, 20.0f, WHITE);
	else DrawRectangleLines(X + 125, Y + 10, 20.0f, 20.0f, WHITE);
	// reverse
	DrawText("re(v)erse", X + 10, Y + 35, 20, WHITE);
	if (s->reverse) DrawRectangle(X + 125, Y + 35, 20.0f, 20.0f, WHITE);
	else DrawRectangleLines(X + 125, Y + 35, 20.0f, 20.0f, WHITE);

	s1.x = X + 155;
	s2.x = X + 155;
	DrawLineEx( s1, s2, 2.0f, WHITE);
	// loop
	DrawText("(l)oop:", X + 165, Y + 10, 20, WHITE);
	if (s->loop_mode == OFF) 
		DrawText("off", X + 165, Y + 35, 20, WHITE);
	else if (s->loop_mode == LOOP) DrawText("loop", X + 165, Y + 35, 20, WHITE);
	else DrawText("ping-pong", X + 165, Y + 35, 20, WHITE);
}

static void draw_sampler(
	const struct system* sys, 
	const struct sampler* sampler,
	const Vector2 *origin)
{
	if (!sys || !sampler || !origin) return;

	draw_sample_window(sys, origin);
	draw_waveform(sampler, origin);
	draw_sampler_controls(sampler, origin);
}

void draw(const struct system* sys, const struct sampler* sampler)
{
	static const Vector2 sampler_origin = { 10, 10 };
	BeginDrawing();
	ClearBackground(BLACK);
	draw_sampler(sys, sampler, &sampler_origin);
	EndDrawing();
}

