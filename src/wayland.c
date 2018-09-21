// #include "config.h"
#include "output.h"
// #include "string-list.h"
#include "wav.h"
#include "wayland.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wayland-client.h>

static void noop() {
	// intentionally left blank
}

static void handle_registry(void *data, struct wl_registry *registry,
		uint32_t name, const char *interface, uint32_t version) {
	struct wav_state *state = data;

	if (strcmp(interface, wl_compositor_interface.name) == 0) {
		state->compositor = wl_registry_bind(registry, name, &wl_compositor_interface, 4);
	} else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
		state->layer_shell = wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1);
	} else if (strcmp(interface, wl_shm_interface.name) == 0) {
		state->shm = wl_registry_bind(registry, name, &wl_shm_interface, 1);
	} else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
		state->output_manager = wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 2);
	} else if (strcmp(interface, wl_output_interface.name) == 0) {
		struct wl_output *output = wl_registry_bind(registry, name, &wl_output_interface, 3);
		create_output(state, output);
	}
}

bool init_wayland(struct wav_state *state) {
	state->display = wl_display_connect(NULL);
	if (state->display == NULL) {
		fputs("Failed to connect to wayland display\n", stderr);
		return false;
	}

	wl_list_init(&state->outputs);

	state->registry = wl_display_get_registry(state->display);
	static struct wl_registry_listener registry_listener = {
		.global = handle_registry,
		.global_remove = noop
	};
	wl_registry_add_listener(state->registry, &registry_listener, state);
	wl_display_roundtrip(state->display);
	if (!(state->compositor && state->layer_shell && state->shm)) {
		fputs("Failed to acquire required wayland resources\n", stderr);
		return false;
	}

	if (wl_list_length(&state->outputs) == 0) {
		// do not terminate in case outputs are later added
		fputs("Warning: no outputs detected\n", stderr);
		return true;
	}

	// second roundtrip to get output properties
	wl_display_roundtrip(state->display);

	if (wl_list_length(&state->outputs) == 0) {
		// do not terminate in case the correct output is later added
		fputs("Warning: no outputs found matching specified names\n", stderr);
	}

	return true;
}

void finish_wayland(struct wav_state *state) {
	struct wav_output *output, *output_tmp;
	wl_list_for_each_safe(output, output_tmp, &state->outputs, link) {
		destroy_output(output);
	}

	wl_shm_destroy(state->shm);
	if (state->output_manager != NULL) zxdg_output_manager_v1_destroy(state->output_manager);
	zwlr_layer_shell_v1_destroy(state->layer_shell);
	wl_compositor_destroy(state->compositor);
	wl_registry_destroy(state->registry);
	wl_display_disconnect(state->display);
}
