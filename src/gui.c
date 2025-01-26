#include "defs.h"
#include "system.h"

#include <stdlib.h>
#include <stdint.h>


static void draw_waveform(
		const struct sampler* sampler, 
		const float origin_x,
		const float origin_y,
		const float width,
		const float max_height)
{
	const Vector2 wave_origin = {origin_x, origin_y};
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
	const float vertex_spacing = width / (float) (num_vertices);

	// generate vertex list from sample data
	Vector2 vertices[num_vertices];
	for (int i = 0; i < num_vertices; i++) {
		const int32_t frame = first_frame_to_draw + i * (int) frame_freq;
		const double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		const float y = 
			(float) sum * (max_height / 2.0f) + wave_origin.y;
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
		const Vector2 startv = {play_x, wave_origin.y - max_height / 2.0f};
		const Vector2 endv = {play_x, wave_origin.y + max_height / 2.0f};
		DrawLineEx(startv, endv, 2.0f, RED);
	}
	if (	s->start_frame >= first_frame_to_draw && 
			s->start_frame < first_frame_to_draw + frames_to_draw) {

		const float start_x = 
			((int) (s->start_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x;
		const Vector2 startv = {start_x, wave_origin.y - max_height / 2.0f};
		const Vector2 endv = {start_x, wave_origin.y + max_height / 2.0f};
		DrawLineEx(startv, endv, 2.0f, BLUE);
	}
	if (	s->end_frame >= first_frame_to_draw && 
			s->end_frame <= first_frame_to_draw + frames_to_draw) {

		const float end_x = 
			((int) (s->end_frame - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x;
		const Vector2 startv = {end_x, wave_origin.y - max_height / 2.0f};
		const Vector2 endv = {end_x, wave_origin.y + max_height / 2.0f};
		DrawLineEx(startv, endv, 2.0f, BLUE);
	}
}

static void draw_sampler(const struct sampler* sampler, const Vector2* origin)
{
	if (!sampler || !origin) return;

	const float BORDER_WIDTH = WINDOW_WIDTH / 2;
	const float BORDER_HEIGHT = 325;
	const float VIEWER_WIDTH = 600;
	const float VIEWER_HEIGHT = 300;
	const float FONT_SIZE = 19;
	const float PAD_WIDTH = 30;
	const float PAD_HEIGHT = 30;

	float X = origin->x;
	float Y = origin->y;
	const struct sample *s = sampler->active_sample;

	const Rectangle border = { X, Y, BORDER_WIDTH, BORDER_HEIGHT };
	DrawRectangleLinesEx(border, 2.0f, WHITE);

	const Rectangle viewer = { X, Y + 25, VIEWER_WIDTH, VIEWER_HEIGHT };
	DrawRectangleLinesEx(viewer, 2.0f, WHITE);

	const Rectangle namebar = { X, Y, BORDER_WIDTH, 27 };
	DrawRectangleLinesEx(namebar, 2.0f, WHITE);

	const Rectangle infopane = { X + VIEWER_WIDTH - 2, Y + 25, 
		BORDER_WIDTH - VIEWER_WIDTH + 2, 105 };
	DrawRectangleLinesEx(infopane, 2.0f, WHITE);

	const Rectangle controlpane = { X + VIEWER_WIDTH - 2, Y + 128,
		(BORDER_WIDTH - VIEWER_WIDTH) / 2 + 1, 197 };
	DrawRectangleLinesEx(controlpane, 2.0f, WHITE);

	// info
	DrawText(TextFormat("filename: %s", s->path), X + 5, Y + 5, FONT_SIZE, WHITE);
	DrawText(TextFormat("zoom: %dx", sampler->zoom), 
			X + VIEWER_WIDTH + 10, Y + 30, FONT_SIZE, WHITE);

	// playback time
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
	DrawText(TextFormat("total length: %01d:%02d", times[0], times[1]), 
			X + VIEWER_WIDTH + 10, Y + 55, FONT_SIZE, WHITE);
	DrawText(TextFormat("active length: %01d:%02d", times[2], times[3]),
			X + VIEWER_WIDTH + 10, Y + 80, FONT_SIZE, WHITE);
	DrawText(TextFormat("playback: %01d:%02d", times[4], times[5]),
			X + VIEWER_WIDTH + 10, Y + 105, FONT_SIZE, WHITE);

	// sample controls
	// gate
	DrawText("gate", X + VIEWER_WIDTH + 10, Y + 140, FONT_SIZE, WHITE);
	if (s->gate) 
		DrawRectangle(X + VIEWER_WIDTH + 125, Y + 140, 20.0f, 20.0f, WHITE);
	else 
		DrawRectangleLines(X + VIEWER_WIDTH + 125, Y + 140, 20.0f, 20.0f, WHITE);

	// reverse
	DrawText("reverse", X + VIEWER_WIDTH + 10, Y + 165, FONT_SIZE, WHITE);
	if (s->reverse) 
		DrawRectangle(X + VIEWER_WIDTH + 125, Y + 165, 20.0f, 20.0f, WHITE);
	else 
		DrawRectangleLines(X + VIEWER_WIDTH + 125, Y + 165, 20.0f, 20.0f, WHITE);

	// loop
	DrawText("loop:", X + VIEWER_WIDTH + 10, Y + 190, FONT_SIZE, WHITE);
	if (s->loop_mode == LOOP_OFF) 
		DrawText("off", X + VIEWER_WIDTH + 65, Y + 190, FONT_SIZE, WHITE);
	else if (s->loop_mode == LOOP) 
		DrawText("loop", X + VIEWER_WIDTH + 65, Y + 190, FONT_SIZE, WHITE);
	else 
		DrawText("ping-pong", X + VIEWER_WIDTH + 65, Y + 190, FONT_SIZE, WHITE);

	// attack / release
	DrawText(TextFormat("attack: %.0fms", frames_to_ms(s->attack)), 
			X + VIEWER_WIDTH + 10, Y + 215, FONT_SIZE, WHITE);
	DrawText(TextFormat("release: %.0fms", frames_to_ms(s->release)), 
			X + VIEWER_WIDTH + 10, Y + 240, FONT_SIZE, WHITE);

	// pitch / speed
	DrawText(TextFormat("pitch: %+.0fst", roundf(speed_to_st(fabs(s->speed)))), 
			X + VIEWER_WIDTH + 10, Y + 265, FONT_SIZE, WHITE);
	DrawText(TextFormat("speed: %.2fx", fabs(s->speed)), 
			X + VIEWER_WIDTH + 10, Y + 290, FONT_SIZE, WHITE);

	// waveform
	draw_waveform(sampler, X + 10, Y + 175, VIEWER_WIDTH - 20, VIEWER_HEIGHT - 20);

	// bank
	const int BANK = sampler->cur_bank;
	DrawText(TextFormat("bank: %d/%d", BANK + 1, sampler->num_banks),
			X + VIEWER_WIDTH + 12 + (BORDER_WIDTH - VIEWER_WIDTH) / 2 + 1,
			Y + 140, FONT_SIZE, WHITE);

	// sample pads
	struct sample **banks = sampler->banks;
	float pad_y = Y + 175;
	float pad_x = X + VIEWER_WIDTH + 12 + (BORDER_WIDTH - VIEWER_WIDTH) / 2 + 1;
	// row 1
	const Rectangle padq = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_Q].num_frames && banks[BANK][PAD_Q].playing) 
		DrawRectangleRec(padq, ORANGE);
	else 
		DrawRectangleRec(padq, WHITE);
	DrawText("q", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);

	pad_x += PAD_WIDTH + 10;
	const Rectangle padw = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_W].num_frames && banks[BANK][PAD_W].playing)
		DrawRectangleRec(padw, ORANGE);
	else 
		DrawRectangleRec(padw, WHITE);
	DrawText("w", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);

	pad_x += PAD_WIDTH + 10;
	const Rectangle pade = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_E].num_frames && banks[BANK][PAD_E].playing)
		DrawRectangleRec(pade, ORANGE);
	else
		DrawRectangleRec(pade, WHITE);
	DrawText("e", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);

	pad_x += PAD_WIDTH + 10;
	const Rectangle padr = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_R].num_frames && banks[BANK][PAD_R].playing)
		DrawRectangleRec(padr, ORANGE);
	else
		DrawRectangleRec(padr, WHITE);
	DrawText("r", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);

	// row 2
	pad_x = X + VIEWER_WIDTH + 12 + (BORDER_WIDTH - VIEWER_WIDTH) / 2 + 1;
	pad_y += PAD_HEIGHT + 10;
	const Rectangle pada = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_A].num_frames && banks[BANK][PAD_A].playing) 
		DrawRectangleRec(pada, ORANGE);
	else 
		DrawRectangleRec(pada, WHITE);
	DrawText("a", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);

	pad_x += PAD_WIDTH + 10;
	const Rectangle pads = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_S].num_frames && banks[BANK][PAD_S].playing)
		DrawRectangleRec(pads, ORANGE);
	else 
		DrawRectangleRec(pads, WHITE);
	DrawText("s", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);

	pad_x += PAD_WIDTH + 10;
	const Rectangle padd = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_D].num_frames && banks[BANK][PAD_D].playing)
		DrawRectangleRec(padd, ORANGE);
	else
		DrawRectangleRec(padd, WHITE);
	DrawText("d", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);

	pad_x += PAD_WIDTH + 10;
	const Rectangle padf = { pad_x, pad_y, PAD_WIDTH, PAD_HEIGHT};
	if (banks[BANK][PAD_F].num_frames && banks[BANK][PAD_F].playing)
		DrawRectangleRec(padf, ORANGE);
	else
		DrawRectangleRec(padf, WHITE);
	DrawText("f", pad_x + 5, pad_y + 5, FONT_SIZE, BLACK);
}

void draw(const struct sampler* sampler)
{
	static const Vector2 sampler_origin = { 10, 10 };
	BeginDrawing();
	ClearBackground(BLACK);
	draw_sampler(sampler, &sampler_origin);
	EndDrawing();
}
