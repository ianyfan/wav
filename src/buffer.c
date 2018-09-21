#define _XOPEN_SOURCE 500

#include "buffer.h"
#include "output.h"
#include "wav.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client.h>

static int create_pool_file(int size, char path[64]) {
	static const char *template = "wav-XXXXXX";
	const char *dir = getenv("XDG_RUNTIME_DIR");
	if (dir == NULL) {
		fputs("XDG_RUNTIME_DIR is not set\n", stderr);
		return -1;
	}

	snprintf(path, 64, "%s/%s", dir, template);

	int fd = mkstemp(path);
	if (fd == -1) {
		fputs("Failed to create pool file\n", stderr);
		return -1;
	}

	if (ftruncate(fd, size) == -1) {
		fputs("Failed to resize pool file\n", stderr);
		close(fd);
		return -1;
	}

	return fd;
}

static void release_buffer(void *data, struct wl_buffer *wl_buffer) {
	struct wav_buffer *buffer = data;
	buffer->busy = false;
}

struct wav_buffer *create_buffer(struct wav_output *output) {
	struct wav_buffer *buffer = malloc(sizeof(*buffer));
	if (buffer == NULL) {
		fputs("Failed to allocate memory for buffer object\n", stderr);
		return NULL;
	}
	buffer->busy = false;

	int stride = 4 * output->width;
	int size = stride * output->height;
	char path[64];
	int fd = create_pool_file(size, path);
	if (fd >= 0) {
		buffer->data = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
		if (buffer->data != MAP_FAILED) {
			struct wl_shm_pool *pool = wl_shm_create_pool(output->state->shm, fd, size);

			buffer->wl_buffer = wl_shm_pool_create_buffer(pool, 0,
					output->width, output->height, stride, WL_SHM_FORMAT_ARGB8888);
			static const struct wl_buffer_listener buffer_listener = {
				.release = release_buffer
			};
			wl_buffer_add_listener(buffer->wl_buffer, &buffer_listener, buffer);

			wl_shm_pool_destroy(pool);
		} else {
			fputs("Failed to map pool file to memory\n", stderr);
			free(buffer);
			buffer = NULL;
		}
	} else {
		free(buffer);
		buffer = NULL;
	}

	close(fd);
	unlink(path);

	return buffer;
}

void destroy_buffer(struct wav_buffer *buffer) {
	wl_buffer_destroy(buffer->wl_buffer);
	free(buffer);
}
