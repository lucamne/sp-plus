#include "defs.h"
#include "system.h"

#include <stdlib.h>
#include <stdint.h>

static void draw_sample_window(const struct sampler* sampler, const Vector2* origin)
{	
	static const float WIN_WIDTH = 600;
	static const float WIN_HEIGHT = 300;
	static const float PAD_WIDTH = 40;
	static const float PAD_HEIGHT = 40;

	const float X = origin->x;
	const float Y = origin->y + 25;

	struct sample** banks = sampler->banks;

	// waveform window
	const Rectangle win = { X, Y , WIN_WIDTH,  WIN_HEIGHT };
	DrawRectangleLinesEx(win, 3.0f, WHITE);

	// sample pads
	const float PAD_Y = Y + WIN_HEIGHT + 10;
	float PAD_X = X;

	const Rectangle padq = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_Q].num_frames && banks[0][PAD_Q].playing) 
		DrawRectangleRounded(padq, 0.3f, 1, ORANGE);
	else 
		DrawRectangleRounded(padq, 0.3f, 1, WHITE);
	DrawText("q", PAD_X + 5, PAD_Y + 5, 20, BLACK);

	PAD_X += PAD_WIDTH + 10;
	const Rectangle padw = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_W].num_frames && banks[0][PAD_W].playing)
		DrawRectangleRounded(padw, 0.3f, 1, ORANGE);
	else 
		DrawRectangleRounded(padw, 0.3f, 1, WHITE);
	DrawText("w", PAD_X + 5, PAD_Y + 5, 20, BLACK);

	PAD_X += PAD_WIDTH + 10;
	const Rectangle pade = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_E].num_frames && banks[0][PAD_E].playing)
		DrawRectangleRounded(pade, 0.3f, 1, ORANGE);
	else
		DrawRectangleRounded(pade, 0.3f, 1, WHITE);
	DrawText("e", PAD_X + 5, PAD_Y + 5, 20, BLACK);

	PAD_X += PAD_WIDTH + 10;
	const Rectangle padr = { PAD_X, PAD_Y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[0][PAD_R].num_frames && banks[0][PAD_R].playing)
		DrawRectangleRounded(padr, 0.3f, 1, ORANGE);
	else
		DrawRectangleRounded(padr, 0.3f, 1, WHITE);
	DrawText("r", PAD_X + 5, PAD_Y + 5, 20, BLACK);
}


static void draw_waveform(const struct sampler* sampler, const Vector2* origin)
{
	// waveform constants
	static const float WAVE_WIDTH = 580.0f;			// width of wave spline 
	static const float WAVE_HEIGHT = 280.0f;		// max height of wave spline

	if (!sampler->active_sample || !origin) return;
	const Vector2 wave_origin = {origin->x + 10, origin->y + 175};
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

	// calculate vertices to render and frames per vertex
	int num_vertices;
	float frame_freq;
	if (frames_to_draw > sampler->max_vert) { 
		num_vertices = sampler->max_vert;
		frame_freq = (float) frames_to_draw / (float) sampler->max_vert;
	} else {
		num_vertices = frames_to_draw;
		frame_freq = 1.0f;
	}
	const float vertex_spacing = WAVE_WIDTH / (float) (num_vertices);

	// generate vertex list from sample data
	Vector2 vertices[num_vertices];
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
	const float X = origin->x + 600;
	const float Y = origin->y + 25;

	// gate
	DrawText("gate", X + 10, Y + 10, 20, WHITE);
	if (s->gate) DrawRectangle(X + 125, Y + 10, 20.0f, 20.0f, WHITE);
	else DrawRectangleLines(X + 125, Y + 10, 20.0f, 20.0f, WHITE);
	// reverse
	DrawText("reverse", X + 10, Y + 35, 20, WHITE);
	if (s->reverse) DrawRectangle(X + 125, Y + 35, 20.0f, 20.0f, WHITE);
	else DrawRectangleLines(X + 125, Y + 35, 20.0f, 20.0f, WHITE);
	// loop
	DrawText("loop:", X + 10, Y + 60, 20, WHITE);
	if (s->loop_mode == LOOP_OFF) 
		DrawText("off", X + 65, Y + 60, 20, WHITE);
	else if (s->loop_mode == LOOP) DrawText("loop", X + 65, Y + 60, 20, WHITE);
	else DrawText("ping-pong", X + 65, Y + 60, 20, WHITE);
	// attack / release
	DrawText(TextFormat("attack: %.0fms", frames_to_ms(s->attack)), 
			X + 10, Y + 85, 20, WHITE);
	DrawText(TextFormat("release: %.0fms", frames_to_ms(s->release)), 
			X + 10, Y + 110, 20, WHITE);
	// pitch / speed
	DrawText(TextFormat("pitch: %+.0fst", roundf(speed_to_st(fabs(s->speed)))), 
			X + 10, Y + 135, 20, WHITE);
	DrawText(TextFormat("speed: %.2fx", fabs(s->speed)), 
			X + 10, Y + 160, 20, WHITE);
	// zoom
	DrawText(TextFormat("zoom: %dx", sampler->zoom), X + 10, Y + 185, 20, WHITE);
	// sample time
	int times[3 * 2] = {0};	// holds mins and secs for each time field
	if (s->num_frames) {
		// total
		const float speed = fabs(s->speed);
		int sec = s->num_frames / SAMPLE_RATE / speed;
		times[0] = sec / 60;
		times[1] = sec % 60;
		// active
		sec = (s->end_frame - s->start_frame) / SAMPLE_RATE / speed;
		times[2] = sec / 60;
		times[3] = sec % 60;
		// playback
		sec = s->next_frame / SAMPLE_RATE / speed;
		times[4] = sec / 60;
		times[5] = sec % 60;
	} 
	DrawText(TextFormat("Total Length: %01d:%02d", times[0], times[1]), 
			X + 10, Y + 210, 20, WHITE);
	DrawText(TextFormat("Active Length: %01d:%02d", times[2], times[3]),
			X + 10, Y + 235, 20, WHITE);
	DrawText(TextFormat("Playback: %01d:%02d", times[4], times[5]),
			X + 10, Y + 260, 20, WHITE);
}

static void draw_sample_info(const struct sampler *sampler, const Vector2 *origin)
{
	const float X = origin->x + 5;
	const float Y = origin->y + 5;

	const struct sample *s = sampler->active_sample;

	DrawText(TextFormat("Name: %s", s->path), X, Y, 20, WHITE);
}

static void draw_sampler(
		const struct sampler* sampler,
		const Vector2 *origin)
{
	if (!sampler || !origin) return;

	draw_sample_info(sampler, origin);
	draw_sample_window(sampler, origin);
	draw_waveform(sampler, origin);
	draw_sampler_controls(sampler, origin);
}

void draw(const struct sampler* sampler)
{
	static const Vector2 sampler_origin = { 10, 10 };
	BeginDrawing();
	ClearBackground(BLACK);
	draw_sampler(sampler, &sampler_origin);
	EndDrawing();
}

