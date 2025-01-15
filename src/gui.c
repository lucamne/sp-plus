#include "gui.h"


void draw(const struct sample_view_params* sp)
{
		BeginDrawing();
		ClearBackground(BLACK);
		draw_sample_view(sp);
		EndDrawing();
}

