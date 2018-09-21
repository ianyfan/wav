#ifndef _AUDIO_H
#define _AUDIO_H

#include "wav.h"

#include <stdbool.h>

bool init_audio(struct wav_state *state);
void finish_audio(struct wav_state *state);

void diminish_bars(struct wav_state *state);

#endif
