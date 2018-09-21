#!/usr/bin/env python

import atexit                       # to register cleanup functions
import ctypes                       # for working with the pulseaudio API
import numpy as np                  # for fft
import sys                          # for command-line arguments
import time                         # for measuring elapsed time


class pa_sample_spec(ctypes.Structure):
    _fields_ = [('format', ctypes.c_int),
                ('rate', ctypes.c_uint32),
                ('channels', ctypes.c_uint8)]


class PulseAudioError(Exception):
    pass


def visualiser_gen(width):
    # constants
    PA_STREAM_RECORD = 2
    PA_SAMPLE_U8 = 0

    # user defined
    freq_step_size = 30
    inertia = 0.9

    # calculated
    freq_step_number = width

    # setup pulseaudio API
    pa = ctypes.cdll.LoadLibrary('libpulse-simple.so.0')

    # mono channel makes processing easier (and sligtly less CPU intensive)
    ss = pa_sample_spec(format=PA_SAMPLE_U8, channels=1, rate=44100)
    error = ctypes.c_int(0)
    s = pa.pa_simple_new(None,                  # server = default
                         'Audio visualiser',    # name
                         PA_STREAM_RECORD,      # dir(ection) of stream
                         None,                  # dev (source name) = default
                         'Audio recorder',      # stream_name
                         ctypes.byref(ss),      # ss (sample type)
                         None,                  # (channel) map = default
                         None,                  # (buffering) attr(ibutes) = default
                         ctypes.byref(error))   # error = null (don't store)
    if not s:
        raise PulseAudioError('Error opening connection to pulseaudio')
    atexit.register(pa.pa_simple_free, s)

    buffer = ctypes.create_string_buffer(ss.channels*ss.rate//freq_step_size)
    bar_heights = np.zeros(freq_step_number, int)

    scale = 2
    waiting_for_audio = False
    while True:
        if pa.pa_simple_read(s, buffer, len(buffer), error) < 0:
            raise PulseAudioError('Error reading audio stream')

        if set(buffer.raw) == {128}: # no audio
            if any(bar_heights):
                bar_heights = (bar_heights - 1).clip(min=0)
            elif waiting_for_audio:
                continue
        else:
            waiting_for_audio = False

            # remove bias due to the data being unsigned
            audio = np.frombuffer(buffer, dtype=np.int8) - 128

            # perform fft, ignoring high frequencies
            audio_fft = np.abs(np.fft.fft(audio)[:freq_step_number])

            # crude rolling mean
            audio_mean = np.concatenate(([(audio_fft[0] + audio_fft[1])/2],
                                         (audio_fft[:-2] + audio_fft[1:-1] + audio_fft[2:])/3,
                                         [(audio_fft[-2] + audio_fft[-1])/2]))

            # move the scale towards the mean of the current sample
            # depending on how much inertia it has
            mean = audio_mean.mean()*2
            scale = inertia*scale + (1-inertia)*mean + 1

            # scale the bar heights so that the mean is always about halfway
            audio_scaled = 8*audio_mean/scale
            audio_rounded = audio_scaled.astype(int).clip(max=8)

            # the bars 'fall' by one unit every time
            # unless the current audio amplitude is higher
            bar_heights = np.maximum(audio_rounded, bar_heights-1)

        # convert the numbers to Unicode block elements
        bars = [chr(i+9600) if i else ' ' for i in bar_heights]

        waiting_for_audio = bool((yield bars))

def main(sway, width):
    try:
        visualiser = visualiser_gen(width)
    except Exception as e:
        raise e

    if sway:
        print('{"version": 1}')
        print('[')

        text = next(visualiser)
        start = time.time()

            # print in accordance to the i3bar input protocol
            print(f'[{{"full_text": "{text}", "align": "center", "min_width": 1920}}],', flush=True)

            # tell the visualiser that if there is no audio
            # it can keep waiting for new audio
            # instead of constantly sending back empty bar_heights
            text = visualiser.send(set(text) == {' '})
    else:
        # simple output
        while True:
            print(f'\r{"".join(next(visualiser))}', end='')


if __name__ == '__main__':
    main(sway='--sway' in sys.argv, width=190 if '--sway' in sys.argv else 180)
