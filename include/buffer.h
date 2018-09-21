#ifndef _BUFFER_H
#define _BUFFER_H

#include "output.h"

#include <stdbool.h>
#include <wayland-client.h>

struct wav_buffer {
	struct wl_buffer *wl_buffer;
	void *data;
	bool busy;
};

struct wav_buffer *create_buffer(struct wav_output *output);
void destroy_buffer(struct wav_buffer *buffer);

#endif
