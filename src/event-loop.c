#define _POSIX_C_SOURCE 199309L

#include "config.h"
#include "event-loop.h"
#include "render.h"
#include "wav.h"

#include <poll.h>
#include <stdbool.h>
#include <stdio.h>
#include <sys/timerfd.h>
#include <time.h>
#include <unistd.h>

enum wav_events {
	WAV_WAYLAND_EVENT,
	WAV_AUDIO_EVENT,
	WAV_EVENT_COUNT
};

void run_event_loop(struct wav_state *state) {
	struct pollfd events[] = {
		[WAV_WAYLAND_EVENT] = (struct pollfd) {
			.fd = wl_display_get_fd(state->display),
			.events = POLLIN
		},
		[WAV_AUDIO_EVENT] = (struct pollfd) {
			.fd = state->audiofd,
			.events = POLLIN
		}
	};

	int polled = 0;
	state->running = true;
	while (state->running) {
		while (wl_display_prepare_read(state->display) != 0) {
			wl_display_dispatch_pending(state->display);
		}
		wl_display_flush(state->display);

		polled =  poll(events, WAV_EVENT_COUNT, -1);
		if (polled < 0) {
			wl_display_cancel_read(state->display);
			break;
		}

		// read wayland events
		if (events[WAV_WAYLAND_EVENT].revents & POLLIN) {
			if (wl_display_read_events(state->display) != 0) {
				fputs("Failed to process wayland event\n", stderr);
				break;
			}
		} else {
			wl_display_cancel_read(state->display);
		}

		// read audio events
		if (events[WAV_AUDIO_EVENT].revents & POLLIN) {
			uint64_t signal;
			ssize_t n = read(state->audiofd, &signal, sizeof(signal));
			if (n < 0) {
				fputs("Failed to process audio event\n", stderr);
				break;
			}
			if (!state->frame_scheduled) render_frame(state);
		}
	}
}
