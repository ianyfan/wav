#include "audio.h"
#include "buffer.h"
// #include "config.h"
#include "render.h"
#include "output.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <wayland-client.h>

// TODO: what if output dimensions are odd
bool create_bars(struct wav_output *output) {
	struct wav_config config = output->state->config;
	int total_bar_width = config.bar_width + 2*config.bar_margin;
	int bar_count = (output->height + output->width)/total_bar_width;
	output->spectrum_size = bar_count;

	output->bars = calloc(output->spectrum_size, sizeof(*output->bars));
	if (output->bars == NULL) {
		return false;
	}

	int bar_height = config.bar_height;
	int roundness = config.roundness*bar_height;

	int horizontal_bar_count = (output->height - 2*roundness)/total_bar_width;
	if (horizontal_bar_count % 2 != bar_count % 2) ++horizontal_bar_count;

	int vertical_bar_count = (output->width/2 - roundness)/total_bar_width;
	int corner_bar_count = (bar_count - horizontal_bar_count)/2 - vertical_bar_count;

	int x = output->width/2 - config.bar_margin;
	for (int i = 0; i < vertical_bar_count; ++i) {
		struct wav_bar *bar = &output->bars[i];
		bar->type = BOTTOM;
		bar->start = x - config.bar_width;
		bar->end = x;

		struct wav_bar *mirror_bar = &output->bars[bar_count - 1 - i];
		*mirror_bar = *bar;
		mirror_bar->type = TOP;

		x -= total_bar_width;
	}

	int y = (output->height - total_bar_width*horizontal_bar_count)/2 - config.bar_margin;

	// x + y = corner_bar_count*corner_bar_width + (corner_bar_count - 1)*2*corner_bar_margin
	//  where: corner_bar_width/corner_bar_margin = bar_width/bar_margin
	float corner_bar_width = (x + y)/(corner_bar_count + (float)(corner_bar_count-1)*2*config.bar_margin/config.bar_width);
	float corner_bar_margin = (x + y - corner_bar_count*corner_bar_width)/(corner_bar_count - 1)/2;
	float corner_bar_total_width = corner_bar_width + 2*corner_bar_margin;

	y = output->height - y;

	float corner_x = x;
	float corner_y = output->height;
	enum bar_type current_type = SKEWED_BOTTOM;
	int center_x = roundness;
	int center_y = output->height - roundness;
	for (int i = vertical_bar_count; i < vertical_bar_count + corner_bar_count; ++i) {
		struct wav_bar *bar = &output->bars[i];
		bar->type = current_type;

		if (current_type == SKEWED_BOTTOM) {
			bar->end = corner_x;
			bar->top_end = bar->end + (center_x - bar->end)*bar_height/roundness;
			corner_x -= corner_bar_width;
			if (corner_x < 0) {
				bar->type = CORNER_BOTTOM_LEFT;
				corner_y -= -corner_x;
				bar->start = corner_y;
				bar->top_start = bar->start + (center_y - bar->start)*bar_height/roundness;

				current_type = SKEWED_LEFT;
				corner_x = 0;
				corner_y -= 2*corner_bar_margin;
			} else {
				bar->start = corner_x;
				bar->top_start = bar->start + (center_x - bar->start)*bar_height/roundness;
				corner_x -= 2*corner_bar_margin;
				if (corner_x <= 0) {
					current_type = SKEWED_LEFT;
					corner_y -= -corner_x;
					corner_x = 0;
				}
			}
		} else { // SKEWED_LEFT
			bar->end = corner_y;
			bar->start = corner_y - corner_bar_width;
			bar->top_end = bar->end + (center_y - bar->end)*bar_height/roundness;
			bar->top_start = bar->start + (center_y - bar->start)*bar_height/roundness;

			corner_y -= corner_bar_total_width;
		}

		struct wav_bar *mirror_bar = &output->bars[bar_count - 1 - i];
		*mirror_bar = *bar;
		if (bar->type == SKEWED_BOTTOM) {
			mirror_bar->type = SKEWED_TOP;
		} else {
			if (bar->type == CORNER_BOTTOM_LEFT) {
				mirror_bar->type = CORNER_TOP_LEFT;
				mirror_bar->start = output->height - bar->start;
				mirror_bar->top_start = output->height - bar->top_start;
			} else {
				mirror_bar->start = output->height - bar->end;
				mirror_bar->end = output->height - bar->start;
				mirror_bar->top_start = output->height - bar->top_end;
				mirror_bar->top_end = output->height - bar->top_start;
			}
		}
	}

	// straighten end corner bars
	if (corner_bar_count > 0) {
		output->bars[bar_count - 1 - vertical_bar_count].top_end =
			output->bars[vertical_bar_count].top_end = output->bars[vertical_bar_count].end;
		output->bars[vertical_bar_count + corner_bar_count - 1].top_start =
			output->bars[vertical_bar_count + corner_bar_count - 1].start;
		output->bars[bar_count - vertical_bar_count - corner_bar_count].top_end =
			output->bars[bar_count - vertical_bar_count - corner_bar_count].end;
	}

	y -= 2*config.bar_margin;

	for (int i = vertical_bar_count + corner_bar_count;
			i < vertical_bar_count + corner_bar_count + horizontal_bar_count; ++i) {
		struct wav_bar *bar = &output->bars[i];
		bar->type = LEFT;
		bar->start = y - config.bar_width;
		bar->end = y;

		y -= total_bar_width;
	}

	// int i = vertical_bar_count - 1;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);
	// i = vertical_bar_count;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);
	// i = vertical_bar_count + corner_bar_count - 1;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);
	// i = vertical_bar_count + corner_bar_count;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);
	// i = vertical_bar_count + corner_bar_count + horizontal_bar_count - 1;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);
	// i = vertical_bar_count + corner_bar_count + horizontal_bar_count;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);
	// i = vertical_bar_count + 2*corner_bar_count + horizontal_bar_count - 1;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);
	// i = vertical_bar_count + 2*corner_bar_count + horizontal_bar_count;
	// printf("%d: %f %f\n", i, output->bars[i].end, output->bars[i].start);

	return true;
}

static void draw_mirrored_pixel(struct wav_output *output, int x, int y, uint32_t color) {
	uint32_t *canvas = output->free_buffer->data;
	canvas[output->width*y + x] = color;
	canvas[output->width*(y + 1) - 1 - x] = color;
}

static void render_straight_bar(struct wav_output *output, struct wav_bar *bar, int bar_height, uint32_t color) {
	int x_start, x_end, y_start, y_end;
	switch (bar->type) {
		case BOTTOM:
			x_start = bar->start;
			x_end = bar->end;
			y_start = output->height - bar_height;
			y_end = output->height;
			break;
		case LEFT:
			x_start = 0;
			x_end = bar_height;
			y_start = bar->start;
			y_end = bar->end;
			break;
		case RIGHT:
			x_start = output->width - bar_height;
			x_end = output->width;
			y_start = bar->start;
			y_end = bar->end;
			break;
		case TOP:
			x_start = bar->start;
			x_end = bar->end;
			y_start = 0;
			y_end = bar_height;
			break;
		default: // this should never happen
			return;
	}

	for (int y = y_start; y < y_end; ++y) {
		for (int x = x_start; x < x_end; ++x) draw_mirrored_pixel(output, x, y, color);
	}
}

static void render_skewed_bar(struct wav_output *output, struct wav_bar *bar, int bar_height, uint32_t color) {
	int max_bar_height = output->state->config.bar_height;
	for (int h = 0; h < bar_height; ++h) {
		int start = roundf(bar->start + (bar->top_start - bar->start)*h/max_bar_height);
		int end = roundf(bar->end + (bar->top_end - bar->end)*h/max_bar_height);

		if (bar->type == SKEWED_BOTTOM || bar->type == SKEWED_TOP) {
			int y = bar->type == SKEWED_BOTTOM ? output->height - 1 - h : h;
			for (int x = start; x < end; ++x) draw_mirrored_pixel(output, x, y, color);
		} else if (bar->type == SKEWED_LEFT || bar->type == SKEWED_RIGHT) {
			int x = bar->type == SKEWED_RIGHT ? output->width - 1 - h : h;
			for (int y = start; y < end; ++y) draw_mirrored_pixel(output, x, y, color);
		}
	}
}

static void render_corner_bar(struct wav_output *output, struct wav_bar *bar, int bar_height, uint32_t color) {
	int max_bar_height = output->state->config.bar_height;
	float bar_top = bar->start + (bar->top_start - bar->start)*bar_height/max_bar_height;
	float bar_edge = bar->end + (bar->top_end - bar->end)*bar_height/max_bar_height;
	float shape_height = bar_top;
	if (bar->type == CORNER_BOTTOM_LEFT) shape_height = output->height - shape_height;

	for (int h = 0; h < roundf(shape_height); ++h) {
		int y = bar->type == CORNER_BOTTOM_LEFT ? output->height - h :
			bar->type == CORNER_TOP_LEFT ? h : 0;
		float x_start = (y - bar->start)/(bar->top_start - bar->start)*max_bar_height;
		if (x_start < 0) x_start = 0;
		float x_end = h > max_bar_height ? max_bar_height :
			h > bar_height ?  bar_edge + (bar_height - bar_edge)*(h - bar_height)/(shape_height - bar_height):
			bar->end + (bar->top_end - bar->end)*h/max_bar_height;
		for (int x = roundf(x_start); x < roundf(x_end); ++x) draw_mirrored_pixel(output, x, y, color);
	}
}

static void render_bar(struct wav_output *output, struct wav_bar *bar, int bar_height, uint32_t color) {
	if (bar->type & STRAIGHT_BAR_TYPE) render_straight_bar(output, bar, bar_height, color);
	else if (bar->type & SKEWED_BAR_TYPE) render_skewed_bar(output, bar, bar_height, color);
	else if (bar->type & CORNER_BAR_TYPE) render_corner_bar(output, bar, bar_height, color);
}

static void draw_frame(struct wav_output *output);

static void handle_frame_done(void *data, struct wl_callback *callback, uint32_t time) {
	wl_callback_destroy(callback);
	struct wav_output *output = data;
	draw_frame(output);
}

static void draw_frame(struct wav_output *output) {
	if (output->free_buffer == NULL) {
		output->free_buffer = create_buffer(output);
		if (output->free_buffer == NULL) return;
	}
	if (output->free_buffer->busy) return;

	struct wav_state *state = output->state;
	uint32_t *canvas = output->free_buffer->data;
	int size = output->height * output->width;
	memset(canvas, 0, size*sizeof(*canvas));

	int max_bar_height = state->config.bar_height;
	diminish_bars(state);
	static float scale = 0.125;
	static const float inertia_up = 0.75;
	static const float inertia_down = 1.0/32;
	float inertia = state->max_amplitude > scale ? inertia_up : inertia_down;
	scale = inertia*state->max_amplitude + (1 - inertia)*scale;

	struct wav_bar *bars = output->bars;
	for (int i = 0; i < output->spectrum_size; ++i) {
		float bar_height = state->frequency_spectrum[i]/scale;
		if (bar_height < state->config.noise_threshold) continue;
		bar_height = bar_height < 1 ?
			(bar_height - state->config.noise_threshold)/(1 - state->config.noise_threshold) : 1;

		bar_height = roundf(bar_height*max_bar_height);
		uint32_t color = 0xc0000000 | (((uint32_t) i * 265443761) % (1<<24)); // TODO: change to actual color
		render_bar(output, &bars[i], bar_height, color);
	}

	wl_surface_attach(output->surface, output->free_buffer->wl_buffer, 0, 0);
	wl_surface_damage_buffer(output->surface, 0, 0, output->width, max_bar_height);
	wl_surface_damage_buffer(output->surface, 0, 0, max_bar_height, output->height);
	wl_surface_damage_buffer(output->surface, 0, output->height - max_bar_height, output->width, max_bar_height);
	wl_surface_damage_buffer(output->surface, output->width - max_bar_height, 0, max_bar_height, output->height);

	bool bars_visible = !state->silent;
	if (!bars_visible) {
		for (int i = 0; i < state->spectrum_size; ++i) {
			if (state->frequency_spectrum[i] > 0) {
				bars_visible = true;
				break;
			}
		}
	}

	if (bars_visible && state->running) {
		state->frame_scheduled = true;
		struct wl_callback *frame_callback = wl_surface_frame(output->surface);
		static const struct wl_callback_listener frame_listener = {
			.done = handle_frame_done
		};
		wl_callback_add_listener(frame_callback, &frame_listener, output);
	} else {
		state->frame_scheduled = false;
	}

	wl_surface_commit(output->surface);

	struct wav_buffer *tmp = output->free_buffer;
	output->free_buffer = output->busy_buffer;
	output->busy_buffer = tmp;
}

void render_frame(struct wav_state *state) {
	struct wav_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		draw_frame(output);
	}
}
