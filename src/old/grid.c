// Grid Drawing code

// sample struct requires tempo field

/*sampler struct fields*/
int subdiv_top;			// grid subdivision e.g. 2/1, 1/1, 1/2
int subdiv_bot;		
GridMode grid_mode;

// calculate gridline spacing
// min/beat * sec/min * beat/measure * measure/gridline * frames/sec
if (sampler->grid_mode) {
	const int div_top = sampler->subdiv_top;
	const int div_bot = sampler->subdiv_bot;
	const int frames_per_gridline = 
		1.0 / s->tempo * 60 * 4 * div_top / div_bot * SAMPLE_RATE;
	// draw gridlines
	for (int32_t i = 0; i < s->num_frames; i += frames_per_gridline)
	{
		if (i < first_frame_to_draw) 
			continue;
		if (i >= first_frame_to_draw + num_vertices * (int) frame_freq)
			break;
		const float x = 
			((i - first_frame_to_draw) / frame_freq) * 
			vertex_spacing + wave_origin.x;
		const Vector2 startv = {x, wave_origin.y - WAVE_HEIGHT / 2.0f};
		const Vector2 endv = {x, wave_origin.y + WAVE_HEIGHT / 2.0f};
		DrawLineEx(startv, endv, 1.0f, LIGHTGRAY);
	}
}

