#ifndef _WAV_H
#define _WAV_H

#include "config.h"

#include "wlr-layer-shell-unstable-v1-client-protocol.h"
#include "xdg-output-unstable-v1-client-protocol.h"

#include <complex.h> // allows fftw to use native complex numbers
#include <fftw3.h>
#include <pulse/pulseaudio.h>
#include <wayland-client.h>

#include <stdbool.h>

struct wav_state {
	struct wav_config config;

	// wayland
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct wl_shm *shm;
	struct zwlr_layer_shell_v1 *layer_shell;
	struct zxdg_output_manager_v1 *output_manager;
	struct wl_list outputs; // wav_output::link
	bool frame_scheduled;

	// audio
	pa_threaded_mainloop *loop;
	pa_stream *stream;

	int audiofd;
	int buf_size;
	float *audio_raw;
	fftwf_complex *audio_fft;
	fftwf_plan fft_plan;
	int spectrum_size;
	float *frequency_spectrum;
	float *loudness_weighting;
	float max_amplitude;
	bool silent;

	// state
	bool running;
};

#endif
