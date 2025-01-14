#include "gui.h"
#include "defs.h"

#include <stdlib.h>
#include <stdint.h>



static void draw_waveform(const struct sample_view_params* p)
{
	const struct sample* s = p->sample;
	int num_points;
	float frame_freq;
	// calculate num points and frame_freq
	if (s->num_frames > p->max_points) { 
		num_points = p->max_points;
		frame_freq = (float) s->num_frames / (float) p->max_points;
	} else {
		num_points = s->num_frames;
		frame_freq = 1.0f;
	}


	Vector2 points[num_points];
	const float sample_width = p->wave_width / (float) (num_points);
	for (int i = 0; i < num_points; i++) {
		// caluclate waveform points
		const int32_t frame = i * (int) frame_freq;
		const double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		const float y = 
			(float) sum * (p->wave_height / 2.0f) + p->wave_origin.y;
		const float x = (float) i * sample_width + p->wave_origin.x;
		const Vector2 v = {x, y};
		points[i] = v;
	}
	// draw waveform
	DrawSplineLinear(points, num_points, p->wave_thickness, p->color);

	/* Draw Markers */
	
	// find marker frames
	const int32_t start_frame = (s->start_frame - s->data) / NUM_CHANNELS;
	const int32_t end_frame = (s->end_frame - s->data) / NUM_CHANNELS;
	const int32_t play_frame = (s->next_frame - s->data) / NUM_CHANNELS;
	// find marker x coordinates
	const float play_x = 
		((int) play_frame / frame_freq) * sample_width + p->wave_origin.x;
	float start_x = 
		((int) start_frame / frame_freq) * sample_width + p->wave_origin.x;
	float end_x = 
		((int) end_frame / frame_freq) * sample_width + p->wave_origin.x;
	// draw playhead, start and end lines
	Vector2 startv = {play_x, p->wave_origin.y - p->wave_height / 2.0f};
	Vector2 endv = {play_x, p->wave_origin.y + p->wave_height / 2.0f};
	DrawLineV(startv, endv, RED);

	startv.x = start_x;
	startv.y = p->wave_origin.y - p->wave_height / 2.0f;
	endv.x = start_x;
	endv.y = p->wave_origin.y + p->wave_height / 2.0f;
	DrawLineV(startv, endv, BLUE);

	startv.x = end_x;
	startv.y = p->wave_origin.y - p->wave_height / 2.0f;
	endv.x = end_x;
	endv.y = p->wave_origin.y + p->wave_height / 2.0f;
	DrawLineV(startv, endv, BLUE);

}

void draw_sample_view(const struct sample_view_params* params)
{
	if (!params || !params->sample)
		return;
	draw_waveform(params);
}
