#ifndef _OUTPUT_H
#define _OUTPUT_H

#include "render.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"

#include <wayland-client.h>

struct wav_output {
	struct wav_state *state;

	struct wl_output *wl_output;
	struct wl_list link; // wav_state::outputs

	struct wl_surface *surface;
	struct zwlr_layer_surface_v1 *layer_surface;
	struct wav_buffer *busy_buffer;
	struct wav_buffer *free_buffer;

	int32_t scale;

	int height;
	int width;
	int spectrum_size;
	struct wav_bar *bars;
};

void create_output(struct wav_state *state, struct wl_output *wl_output);
void destroy_output(struct wav_output *output);

#endif
