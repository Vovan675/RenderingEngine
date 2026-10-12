#pragma once

int2 RAB_ClampSamplePositionIntoView(int2 pixel_position, bool prev_frame)
{
	int width = int(render_resolution.x);
	int height = int(render_resolution.y);

	// Reflect the position across the screen edges.
	if (pixel_position.x < 0) pixel_position.x = -pixel_position.x;
	if (pixel_position.y < 0) pixel_position.y = -pixel_position.y;
	if (pixel_position.x >= width) pixel_position.x = 2 * width - pixel_position.x - 1;
	if (pixel_position.y >= height) pixel_position.y = 2 * height - pixel_position.y - 1;

	return pixel_position;
}
