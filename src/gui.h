#ifndef GUI_H
#define GUI_H

#include "audio_backend.h"
#include "raylib.h"


typedef enum { PLAY, START, END } marker;

struct sample_view_params {
	// sample should be made const, need to fix input logic first
	struct sample* sample;	// sample to be drawn
	const Color color;
	const Vector2 wave_origin;	// location to start waveform
	float wave_thickness;		// waveform thickness
	float wave_height;		// max height of waveform
	float wave_width;		// max width of waveform
	int max_points;			// max points to be rendered
	float zoom;			// magnification amount
	marker zoom_focus;		// marker to focus on when zooming
};

void draw(const struct sample_view_params* sp);

void draw_sample_view(const struct sample_view_params* params);
//void draw_sample_view(struct sample* s, const Vector2* origin, float width, float height);


#endif
