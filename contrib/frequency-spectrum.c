#include <complex.h>        // allows fftw to use native complex numbers
#include <fftw3.h>          // fft library
#include <locale.h>         // for printing wide characters
#include <math.h>
#include <pthread.h>        // for creating background thread
#include <pulse/simple.h>   // pulseaudio API
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NANOSECONDS_IN_A_SECOND 1000000000

// arguments struct for background thread
struct args_struct {
    int width;
    float *bar_heights;
    float *bar_peaks;
    long *last_update;
    bool *silent_p;
    int refresh_rate;
};

// helper functions
void sleep(long nanoseconds) {
    static struct timespec sleep_spec;
    if (nanoseconds > 0) {
        sleep_spec.tv_nsec = nanoseconds;
        nanosleep(&sleep_spec, NULL);
    }
}

long get_time(void) {
    static struct timespec timestamp;
    clock_gettime(CLOCK_MONOTONIC, &timestamp);
    return timestamp.tv_sec*NANOSECONDS_IN_A_SECOND + timestamp.tv_nsec;
}

float equal_loudness_curve_low(float f) {
    float logf = log10f(f);
    return (-14.424*logf + 91.472)*logf - 143.88;
}

float equal_loudness_curve_middle(float f) {
    float logf = log10f(f);
    return ((530*logf - 4875.5)*logf + 14926.688)*logf - 15210.564;
}

float equal_loudness_curve_high(float f) {
    float logf = log10f(f);
    return (-101.5*logf + 708.3)*logf - 1232.1;
}

void *run(void *arguments) {
    // unpack arguments
    struct args_struct *args = (struct args_struct *) arguments;
    int width = args->width;
    float *bar_heights = args->bar_heights;
    float *bar_peaks = args->bar_peaks;
    long *last_update = args->last_update;
    bool *silent_p = args->silent_p;
    int refresh_rate = args->refresh_rate;

    // initialise variables
    float scale = 0;
    float inertia = 10; // how fast the scale moves

    pa_sample_spec sample_spec = { .channels = 1,
                                   .format = PA_SAMPLE_FLOAT32,
                                   .rate = 44100 };

    int freq_step_size = 10;
    int buf_size = sample_spec.rate/freq_step_size;
    int refill_size = sample_spec.rate/refresh_rate; // since the refresh rate is faster than the entire buffer can fill
                                                     // audio is read in quicker by partially filling in the buffer
                                                     // and shifting along the old values

    // initialise arrays involved in the audio processing
    float audio_raw[buf_size];
    fftwf_complex audio_fft[buf_size/2 + 1];
    fftwf_plan fft = fftwf_plan_dft_r2c_1d(buf_size, audio_raw, audio_fft, FFTW_PATIENT);
    float frequency_spectrum[width];

    float signal_normalisation = 1/cbrtf(buf_size);
    float loudness_weighting[width]; // ISO 226 @ 60 phons, estimated by some fitted curves
    for (int i = 0; i < width; i++) {
        float f = (i+1)*freq_step_size; // ignore 0 Hz
        float equal_loudness_value = f <= 800 ? equal_loudness_curve_low(f) :
                                     f < 2000 ? equal_loudness_curve_middle(f) :
                                                equal_loudness_curve_high(f);
        loudness_weighting[i] = powf(2, equal_loudness_value/18.06)*signal_normalisation; // scaled into a percentage
    }

    long last_timestamp = get_time();
    long timestamp;

    // open pulseaudio connection
    int error;
    pa_simple *pulseaudio_connection = pa_simple_new(NULL,                  // server = default
                                                     "Frequency Spectrum",  // name
                                                     PA_STREAM_RECORD,      // dir(ection) of stream
                                                     NULL,                  // dev (source name) = default
                                                     "Audio recorder",      // stream_name
                                                     &sample_spec,          // sample type
                                                     NULL,                  // (channel) map = default
                                                     NULL,                  // (buffering) attr(ibutes) = default
                                                     &error);               // error
    if (!pulseaudio_connection) {
        perror("frequency-spectrum: pulseaudio error");
        exit(2);
    }

    pa_simple_read(pulseaudio_connection, audio_raw, (buf_size - refill_size)*sizeof(float), &error);
    while (true) {
        pa_simple_read(pulseaudio_connection, audio_raw + buf_size - refill_size, refill_size*sizeof(float), &error);

        *silent_p = true;
        for (int i = 0; i < buf_size; i++) {
            if (audio_raw[i] != 0) {
                *silent_p = false;
                break;
            }
        }
        if (*silent_p) continue; // skip if silent

        fftwf_execute(fft);

        // first shift the output along by one to discard 0 Hz
        // take the absolute value of the complex outputs to get the amplitude
        // then normalise the values
        // then equalise the amplitudes according to equal-loudness contours (mainly quieten lower frequencies)
        // for some reason, amplitude seems to follow a cubic conversion from decibels to percentage
        // at the same time, determine the maximum amplitude, eliminating the need for an extra loop
        float max_amplitude = 0;
        for (int i = 0; i < width; i++) {
            frequency_spectrum[i] = cbrtf(cabs(audio_fft[i + 1]))*loudness_weighting[i];
            if (frequency_spectrum[i] > max_amplitude) max_amplitude = frequency_spectrum[i];
        }

        // determine the new scale
        // which is shifted by the difference between the current scale and the current max
        // multiplied by the inertia and the time elapsed since last iteration
        timestamp = get_time();
        float change = inertia*(timestamp - last_timestamp)/NANOSECONDS_IN_A_SECOND;
        if (change > 1) change = 1; // cap large changes that occur after a long pause
        scale = change*max_amplitude + (1 - change)*scale;

        last_timestamp = timestamp;

        // discard a "bed" of noise (bottom quarter)
        // then normalise to fit within a 0-9 range
        // since the unicode blocks are in eigths
        for (int i = 0; i < width; i++) {
            float height = 12*frequency_spectrum[i]/scale - 3;
            if (height >= bar_heights[i]) {
                if (height >= 9) height = 8.99;
                bar_peaks[i] = height;
                last_update[i] = timestamp;
            }
        }

        memmove(audio_raw, audio_raw + refill_size, (buf_size - refill_size)*sizeof(float));
    }
}

int main(int argc, char **argv) {
    // parse arguments
    if (argc != 2 && argc != 3) {
        printf("Usage: frequency-spectrum [-b|--bar] WIDTH\n");
        return 1;
    }
    bool bar = false;
    if (argc == 3) {
        if (strcmp(argv[1], "-b") && strcmp(argv[1], "--bar") && strcmp(argv[2], "-b") && strcmp(argv[2], "--bar")) {
            printf("frequency-spectrum: invalid option\n");
            return 2;
        }
        bar = true;
    }
    char *line_format = bar ? "[{\"name\": \"frequency_spectrum\", \"full_text\": \"%ls\", \"align\": \"center\", \"min_width\": 1920}],\n" : "\r%ls";

    char *arg_pointer;
    int width = (int) strtoul(argv[1], &arg_pointer, 10);
    if (*arg_pointer != '\0') {
        if (argc == 3) width = (int) strtoul(argv[2], &arg_pointer, 10);
        if (*arg_pointer != '\0') {
            printf("frequency-spectrum: invalid width\n");
            return 3;
        }
    }

    setlocale(LC_ALL, "en_GB.utf8");

    // initialise arguments for background thread
    float bar_heights[width];
    memset(bar_heights, 0, width*sizeof(float));
    float bar_peaks[width];
    memset(bar_peaks, 0, width*sizeof(float));
    long last_update[width];
    memset(last_update, 0, width*sizeof(long));
    bool silent = true;
    int refresh_rate = 24;

    // create background thread
    pthread_t *thread = malloc(sizeof(pthread_t));
    pthread_create(thread, NULL, run, & (struct args_struct) { .width = width,
                                                               .bar_heights = bar_heights,
                                                               .bar_peaks = bar_peaks,
                                                               .last_update = last_update,
                                                               .silent_p = &silent,
                                                               .refresh_rate = refresh_rate });

    bool no_bars;
    wchar_t blocks[] = { L' ', L'▁', L'▂', L'▃', L'▄', L'▅', L'▆', L'▇', L'█' };
    wchar_t line[width + 1];
    line[width] = '\0';

    int diminish_rate = NANOSECONDS_IN_A_SECOND/16;

    long last_timestamp = get_time();
    long timestamp;

    if (bar) printf("{\"version\": 1, \"click_events\": true}\n[\n");
    while (true) {
        timestamp = get_time();
        if (!(silent && no_bars)) {
            no_bars = true;
            for (int i = 0; i < width; i++) {
                if ( (bar_heights[i] = bar_peaks[i] - (timestamp - last_update[i])/diminish_rate) >= 1) no_bars = false;
                else if (bar_heights[i] < 0) bar_heights[i] = 0;

                line[i] = blocks[(int) bar_heights[i]];
            }
            printf(line_format, line);
            fflush(stdout);
        }

        sleep(NANOSECONDS_IN_A_SECOND/refresh_rate - (timestamp - last_timestamp));
        last_timestamp = timestamp;
    }
}
