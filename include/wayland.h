#ifndef _WAYLAND_H
#define _WAYLAND_H

#include "wav.h"

#include <stdbool.h>

bool init_wayland(struct wav_state *state);
void finish_wayland(struct wav_state *state);

#endif
