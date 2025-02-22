#include "raster.h"

static char pad_to_char(int pad)
{
	switch (pad) {
		case PAD_Q:
			return 'Q';
		case PAD_W:
			return 'W';
		case PAD_E:
			return 'E';
		case PAD_R:
			return 'R';
		case PAD_A:
			return 'A';
		case PAD_S:
			return 'S';
		case PAD_D:
			return 'D';
		case PAD_F:
			return 'F';
		default:
			return '?';
	}
}


static void draw_waveform(
		const struct sp_state *sp_state, 
		const struct pixel_buffer *buffer, 
		const vec2i wave_origin, const int width, 
		const int max_height)
{
	const struct sampler sampler = sp_state->sampler;
	const struct sample *s = sampler.active_sample;
	if (!s) return;

	// calculate zoom parameters
	const int32_t frames_to_draw = s->num_frames / sampler.zoom;
	const int32_t focused_frame = sampler.zoom_focus == END ? 
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
	if (frames_to_draw > sampler.max_vert) { 
		num_vertices = sampler.max_vert;
		frame_freq = (float) frames_to_draw / (float) sampler.max_vert;
	} else {
		num_vertices = frames_to_draw;
		frame_freq = 1.0f;
	}
	if (num_vertices < 2) return;
	const float vertex_spacing = width / (float) (num_vertices);

	// draw wave lines
	{
		int32_t frame = first_frame_to_draw; 
		double sum = 
			(s->data[frame * NUM_CHANNELS] + 
			 s->data[frame * NUM_CHANNELS + 1]) / 2.0;
		int y = roundf((float) sum * (max_height / 2.0f) + wave_origin.y);
		int x = wave_origin.x;
		vec2i last_vertex = {x, y};

		for (int i = 1; i < num_vertices; i++) {
			frame = first_frame_to_draw + i * (int) frame_freq;
			sum = (s->data[frame * NUM_CHANNELS] + s->data[frame * NUM_CHANNELS + 1]) / 2.0;

			y = roundf((float) sum * (max_height / 2.0f) + wave_origin.y);
			x = roundf((float) i * vertex_spacing + wave_origin.x);

			const vec2i curr_vertex = {x, y};
			draw_line(buffer, last_vertex, curr_vertex, WHITE);
			last_vertex = curr_vertex;
		}
	}


	// draw markers
	if (	s->next_frame >= first_frame_to_draw && 
			s->next_frame < first_frame_to_draw + frames_to_draw) {

		const int play_x = 
			roundf(((int) (s->next_frame - first_frame_to_draw) / frame_freq) * 
					vertex_spacing + wave_origin.x);
		const vec2i startv = {play_x, roundf(wave_origin.y - max_height / 2.0f)};
		const vec2i endv = {play_x, roundf(wave_origin.y + max_height / 2.0f)};
		draw_line(buffer, startv, endv, RED);
	}
	if (	s->start_frame >= first_frame_to_draw && 
			s->start_frame < first_frame_to_draw + frames_to_draw) {

		const int start_x = 
			roundf(((int) (s->start_frame - first_frame_to_draw) / frame_freq) * 
					vertex_spacing + wave_origin.x);
		const vec2i startv = {start_x, roundf(wave_origin.y - max_height / 2.0f)};
		const vec2i endv = {start_x, roundf(wave_origin.y + max_height / 2.0f)};
		draw_line(buffer, startv, endv, GREEN);
	}
	if (	s->end_frame >= first_frame_to_draw && 
			s->end_frame <= first_frame_to_draw + frames_to_draw) {

		const int end_x = 
			roundf(((int) (s->end_frame - first_frame_to_draw) / frame_freq) * 
					vertex_spacing + wave_origin.x);
		const vec2i startv = {end_x, roundf(wave_origin.y - max_height / 2.0f)};
		const vec2i endv = {end_x, roundf(wave_origin.y + max_height / 2.0f)};
		draw_line(buffer, startv, endv, GREEN);
	}
}

// TODO Kinda gross?
// Used when active sample is a null sample to prevent garbage ui output
static const struct sample DUMMY_SAMPLE = {0};

static void draw_sampler(const struct sp_state *sp_state, const struct pixel_buffer *buffer)
{
	ASSERT(sp_state);

	// TODO maybe find a more elegant solution to this
	const struct sample *active_sample;
	if (sp_state->sampler.active_sample)
		active_sample = sp_state->sampler.active_sample;
	else 
		active_sample = &DUMMY_SAMPLE;

	// sets position of sampler on screen
	const vec2i origin = {350, 0};
	const int BORDER_W = 800;
	const int BORDER_H = 400;
	const int VIEWER_W = BORDER_W * 3 / 4;
	const int VIEWER_H = BORDER_H * 3 / 4;
	const vec2i WAVE_ORIGIN = {origin.x + 10, origin.y + VIEWER_H / 2};

	// border pane
	draw_rec_outline(buffer, origin, BORDER_W, BORDER_H, WHITE);
	// waveform viewer pane
	draw_rec_outline(buffer, origin, VIEWER_W, VIEWER_H, WHITE);
	// draw control pane
	const vec2i control_s = {origin.x + VIEWER_W - 1, origin.y + VIEWER_H - 1};
	const vec2i control_e = {origin.x + VIEWER_W - 1, origin.y + BORDER_H - 1};
	draw_line(buffer, control_s, control_e, WHITE);

	// draw waveform
	draw_waveform(sp_state, buffer, WAVE_ORIGIN, VIEWER_W - 20, VIEWER_H - 20);

	/////////////////////////////////////////////////////////
	/// info
	const struct font *curr_font = sp_state->fonts + MED;
	int font_h = curr_font->height;
	vec2i txt_pos = origin;
	char txt[64];
	
	// filename
	txt_pos.x = origin.x + 10;
	txt_pos.y = origin.y + VIEWER_H;
	snprintf(txt, 64, "filename: %s", active_sample->path);
	draw_text(buffer, txt, sp_state->fonts + MED, txt_pos, WHITE);

	// bank
	txt_pos.y += font_h + 2;
	snprintf(txt, 64, "bank: %d/%d", sp_state->sampler.curr_bank + 1, sp_state->sampler.num_banks);
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	// curr pad
	txt_pos.x += 100;
	snprintf(txt, 64, "pad: %c", pad_to_char(sp_state->sampler.curr_pad));
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	// playback time
	int times[3 * 2] = {0};	// holds mins and secs for each time field
	if (active_sample->num_frames) {
		// total
		const float speed = fabs(active_sample->speed);
		int sec = active_sample->num_frames / SAMPLE_RATE / speed;
		times[0] = sec / 60;
		times[1] = sec % 60;
		// active
		sec = (active_sample->end_frame - active_sample->start_frame) / SAMPLE_RATE / speed;
		times[2] = sec / 60;
		times[3] = sec % 60;
		// playback
		sec = active_sample->next_frame / SAMPLE_RATE / speed;
		times[4] = sec / 60;
		times[5] = sec % 60;
	} 
	txt_pos.x = origin.x + VIEWER_W + 7;
	txt_pos.y = origin.y;
	snprintf(txt, 64, "total length: %01d:%02d", times[0], times[1]);
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	txt_pos.y += font_h + 2;
	snprintf(txt, 64, "active length: %01d:%02d", times[2], times[3]);
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	txt_pos.y += font_h + 2;
	snprintf(txt, 64, "playback: %01d:%02d", times[4], times[5]);
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	//////////////////////////////////////////////////////////////////////////
	/// Sample Controls

	// gate
	txt_pos.y += 2 * font_h;
	vec2i rec_pos = {txt_pos.x + 100, txt_pos.y + 6};
	strcpy(txt, "gate:");

	draw_text(buffer, txt, curr_font, txt_pos, WHITE);
	if (active_sample->gate) 
		draw_rec(buffer, rec_pos, font_h - 2, font_h - 2, WHITE);
	else 
		draw_rec_outline(buffer, rec_pos, font_h - 2, font_h - 2, WHITE);
	
	// reverse
	txt_pos.y += font_h + 2;
	rec_pos.y += font_h + 2;
	strcpy(txt, "reverse:");

	draw_text(buffer, txt, sp_state->fonts + MED, txt_pos, WHITE);
	if (active_sample->reverse) 
		draw_rec(buffer, rec_pos, font_h - 2, font_h - 2, WHITE);
	else 
		draw_rec_outline(buffer, rec_pos, font_h - 2, font_h - 2, WHITE);

	// loop
	txt_pos.y += font_h + 2;
	char *loop_mode;
	switch (active_sample->loop_mode) {
		case LOOP:
			loop_mode = "loop";
			break;
		case PING_PONG:
			loop_mode = "ping-pong";
			break;
		case LOOP_OFF:
		default:
			loop_mode = "off";
			break;
	}

	snprintf(txt, 64, "loop: %s", loop_mode);
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	// attack / release
	txt_pos.y += font_h + 2;
	snprintf(txt, 64, "attack: %.0fms", frames_to_ms(active_sample->attack));
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	txt_pos.y += font_h + 2;
	snprintf(txt, 64, "release: %.0fms", frames_to_ms(active_sample->release));
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	// pitch / speed
	txt_pos.y += font_h + 2;
	snprintf(txt, 64, "pitch: %+.0fst", roundf(speed_to_st(fabs(active_sample->speed))));
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);

	txt_pos.y += font_h + 2;
	snprintf(txt, 64, "speed: %.2fx", fabs(active_sample->speed));
	draw_text(buffer, txt, curr_font, txt_pos, WHITE);


	////////////////////////////////////////////////////////////////////////////////
	/// Sample Pads

	struct sample ***banks = sp_state->sampler.banks;
	int curr_bank = sp_state->sampler.curr_bank;

	vec2i pad_pos = {origin.x + 10, origin.y + VIEWER_H + (int) (2.5f * font_h) };
	vec2i label_pos = {pad_pos.x + 2, pad_pos.y - 2};
	int PAD_WIDTH = 40;
	int PAD_HEIGHT = 40;

	// Q
	if (banks[curr_bank][PAD_Q] && banks[curr_bank][PAD_Q]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "Q", curr_font, label_pos, BLACK);

	// W
	pad_pos.x += PAD_WIDTH + 10;
	label_pos.x += PAD_WIDTH + 10; 
	if (banks[curr_bank][PAD_W] && banks[curr_bank][PAD_W]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "W", curr_font, label_pos, BLACK);

	// E
	pad_pos.x += PAD_WIDTH + 10;
	label_pos.x += PAD_WIDTH + 10; 
	if (banks[curr_bank][PAD_E] && banks[curr_bank][PAD_E]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "E", curr_font, label_pos, BLACK);

	// R
	pad_pos.x += PAD_WIDTH + 10;
	label_pos.x += PAD_WIDTH + 10; 
	if (banks[curr_bank][PAD_R] && banks[curr_bank][PAD_R]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "R", curr_font, label_pos, BLACK);

	// A
	pad_pos.x += PAD_WIDTH + 10;
	label_pos.x += PAD_WIDTH + 10; 
	if (banks[curr_bank][PAD_A] && banks[curr_bank][PAD_A]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "A", curr_font, label_pos, BLACK);

	// S
	pad_pos.x += PAD_WIDTH + 10;
	label_pos.x += PAD_WIDTH + 10; 
	if (banks[curr_bank][PAD_S] && banks[curr_bank][PAD_S]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "S", curr_font, label_pos, BLACK);

	// D
	pad_pos.x += PAD_WIDTH + 10;
	label_pos.x += PAD_WIDTH + 10; 
	if (banks[curr_bank][PAD_D] && banks[curr_bank][PAD_D]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "D", curr_font, label_pos, BLACK);

	// F
	pad_pos.x += PAD_WIDTH + 10;
	label_pos.x += PAD_WIDTH + 10; 
	if (banks[curr_bank][PAD_F] && banks[curr_bank][PAD_F]->playing)
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, RED);
	else
		draw_rec(buffer, pad_pos, PAD_WIDTH, PAD_HEIGHT, WHITE);
	draw_text(buffer, "F", curr_font, label_pos, BLACK);
}
