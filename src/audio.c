#include "audio.h"
#include "output.h"
#include "wav.h"

#include <complex.h>
#include <fftw3.h>
#include <pulse/pulseaudio.h>

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

void diminish_bars(struct wav_state *state) {
	static struct timespec last_timestamp;
	struct timespec timestamp;
	clock_gettime(CLOCK_MONOTONIC, &timestamp);
	long diff = 1000000000*(timestamp.tv_sec - last_timestamp.tv_sec) + timestamp.tv_nsec - last_timestamp.tv_nsec;

	float max_amplitude = 0;
	for (int i = 0; i < state->spectrum_size; ++i) {
		float amplitude = state->frequency_spectrum[i] - diff*state->config.diminish_rate/1000000000.0;
		state->frequency_spectrum[i] = amplitude > 0 ? amplitude : 0;
		if (amplitude > max_amplitude) max_amplitude = amplitude;
	}
	state->max_amplitude = max_amplitude;

	last_timestamp = timestamp;
}

void read_stream(pa_stream *stream, size_t nbytes, void *data) {
	const void *stream_ptr;
	pa_stream_peek(stream, &stream_ptr, &nbytes);
	if (stream_ptr == NULL) {
		if (nbytes > 0) pa_stream_drop(stream);
		return;
	}

	// append new audio to buffer
	struct wav_state *state = data;
	size_t new_size = nbytes/sizeof(*state->audio_raw);
	int old_size = state->buf_size - new_size;
	if (old_size > 0) {
		memmove(state->audio_raw, state->audio_raw + new_size, old_size*sizeof(*state->audio_raw));
		memcpy(state->audio_raw + old_size, stream_ptr, nbytes);
	} else {
		memcpy(state->audio_raw, stream_ptr - old_size, state->buf_size*sizeof(*state->audio_raw));
	}
	pa_stream_drop(stream);

	// check for silence
	bool silent = true;
	for (int i = 0; i < state->buf_size; ++i) {
		if (state->audio_raw[i] != 0) {
			silent = false;
			break;
		}
	}
	if (state->silent && !silent) {
		static const uint64_t signal = 1;
		write(state->audiofd, &signal, sizeof(signal));
	}
	state->silent = silent;
	if (silent) return;

	// perform fft
	fftwf_execute(state->fft_plan);

	diminish_bars(state);
	for (int i = 0; i < state->spectrum_size; ++i) {
		float amplitude = cbrtf(cabs(state->audio_fft[i + 1]))*state->loudness_weighting[i];
		if (amplitude > state->frequency_spectrum[i]) state->frequency_spectrum[i] = amplitude;
	}
}

bool init_audio(struct wav_state *state) {
	state->silent = true;
	state->loop = pa_threaded_mainloop_new();
	pa_mainloop_api *loop_api = pa_threaded_mainloop_get_api(state->loop);

	pa_context *context = pa_context_new(loop_api, NULL);
	pa_context_connect(context, NULL, PA_CONTEXT_NOFLAGS, NULL);

	pa_threaded_mainloop_start(state->loop);

	static const pa_sample_spec sample_spec = {
		.channels = 1,
		.format = PA_SAMPLE_FLOAT32,
		.rate = 44100
	};

	state->buf_size = sample_spec.rate/state->config.frequency_step;
	state->audio_raw = calloc(state->buf_size, sizeof(float));
	state->audio_fft = calloc(state->buf_size/2 + 1, sizeof(fftw_complex));
	state->fft_plan = fftwf_plan_dft_r2c_1d(state->buf_size, state->audio_raw, state->audio_fft, FFTW_PATIENT);
	int max_spectrum_size = 0;
	struct wav_output *output;
	wl_list_for_each(output, &state->outputs, link) {
		if (output->spectrum_size > max_spectrum_size) max_spectrum_size = output->spectrum_size;
	}
	state->spectrum_size = max_spectrum_size;
	state->frequency_spectrum = calloc(max_spectrum_size, sizeof(float));
	state->loudness_weighting = calloc(max_spectrum_size, sizeof(float));
	if (state->audio_raw == NULL || state->audio_fft == NULL ||
			state->fft_plan == NULL || state->frequency_spectrum == NULL) {
		fputs("Failed to initialised audio\n", stderr);
		return false;
	}

	float signal_normalisation = 1/cbrtf(state->buf_size);
	for (int i = 0; i < max_spectrum_size; ++i) {

		float f = (i+1)*state->config.frequency_step;
		float logf = log10f(f);
		float equal_loudness_value =
			f <= 800 ? (-14.424*logf + 91.472)*logf - 143.88 :
			f < 2000 ? ((530*logf - 4875.5)*logf + 14926.688)*logf - 15210.564 :
			(-101.5*logf + 708.3)*logf - 1232.1;
		state->loudness_weighting[i] = powf(2, equal_loudness_value/18.06)*signal_normalisation;
	}

	state->audiofd = eventfd(0, 0);

	while (pa_context_get_state(context) != PA_CONTEXT_READY) {}

	state->stream = pa_stream_new(context, "Frequency spectrum", &sample_spec, NULL);
	pa_stream_set_read_callback(state->stream, read_stream, state);
	pa_stream_connect_record(state->stream, NULL, NULL, PA_STREAM_NOFLAGS);

	return true;
}

void finish_audio(struct wav_state *state) {
	pa_stream_disconnect(state->stream);
	pa_stream_unref(state->stream);

	pa_context *context = pa_stream_get_context(state->stream);
	pa_context_disconnect(context);
	pa_context_unref(context);

	pa_threaded_mainloop_stop(state->loop);
	pa_threaded_mainloop_free(state->loop);

	free(state->frequency_spectrum);
	fftwf_destroy_plan(state->fft_plan);
	free(state->audio_fft);
	free(state->audio_raw);
}
