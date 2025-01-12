#include "gui.h"
#include "defs.h"

#include <stdlib.h>
#include <stdint.h>

static const int max_points = 4000;

void draw_sample_view(struct sample* s, const Vector2* origin, float width, float height)
{
	if (!s) return;

	int num_points;
	float freq;
	if (s->num_frames > max_points) { 
		num_points = max_points;
		freq = (float) s->num_frames / (float) max_points;
	} else {
		num_points = s->num_frames;
		freq = 1.0f;
	}


	Vector2 points[num_points];
	const float sample_width = width / (float) (num_points);
	for (int i = 0; i < num_points; i++) {
		// caluclate waveform points
		const int32_t frame = i * (int) freq;
		const double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		const float y = (float) sum * (height / 2.0f) + origin->y;
		const float x = (float) i * sample_width + origin->x;
		const Vector2 v = {x, y};
		points[i] = v;
	}
	// draw waveform
	DrawSplineLinear(points, num_points, 2.0f, WHITE);

	const int32_t start = (s->start_frame - s->data) / NUM_CHANNELS;
	const int32_t end = (s->end_frame - s->data) / NUM_CHANNELS;
	const int32_t playhead_frame = (s->next_frame - s->data) / NUM_CHANNELS;
	const float playhead_x = 
		((int) playhead_frame / freq) * sample_width + origin->x;
	float start_x = ((int) start / freq) * sample_width + origin->x;
	float end_x = ((int) end / freq) * sample_width + origin->x;
	// draw playhead, start and end lines
	Vector2 startv = {playhead_x, origin->y - height / 2.0f};
	Vector2 endv = {playhead_x, origin->y + height / 2.0f};
	DrawLineV(startv, endv, RED);

	startv.x = start_x;
	startv.y = origin->y - height / 2.0f;
	endv.x = start_x;
	endv.y = origin->y + height / 2.0f;
	DrawLineV(startv, endv, BLUE);

	startv.x = end_x;
	startv.y = origin->y - height / 2.0f;
	endv.x = end_x;
	endv.y = origin->y + height / 2.0f;
	DrawLineV(startv, endv, BLUE);
}
