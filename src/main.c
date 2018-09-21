#define _POSIX_SOURCE

#include "audio.h"
#include "config.h"
#include "event-loop.h"
#include "wav.h"
#include "wayland.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

// static const char *usage =

static struct wav_state state = {0};

static void handle_signal(int signum) {
	state.running = false;
}

int main(int argc, char **argv) {
	init_default_config(&state.config);
	/*switch (parse_config(&state.config, argc, argv)) {
		case 1:
			printf(usage);
			return EXIT_SUCCESS;
		case -1: return EXIT_FAILURE;
	} // ignore 0
*/

	if (!init_wayland(&state)) return EXIT_FAILURE;
	if (!init_audio(&state)) return EXIT_FAILURE;

	struct sigaction sa = { .sa_handler = handle_signal };
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	run_event_loop(&state);

	finish_audio(&state);
	finish_wayland(&state);
//	finish_config(&state.config);

	return EXIT_SUCCESS;
}
