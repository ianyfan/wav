#ifndef _RENDER_H
#define _RENDER_H

#include "output.h"
#include "wav.h"

enum bar_type {
	BOTTOM = 1 << 0,
	LEFT = 1 << 1,
	RIGHT = 1 << 2,
	TOP = 1 << 3,
	SKEWED_BOTTOM = 1 << 4,
	SKEWED_LEFT = 1 << 5,
	SKEWED_RIGHT = 1 << 6,
	SKEWED_TOP = 1 << 7,
	CORNER_BOTTOM_LEFT = 1 << 8,
	CORNER_BOTTOM_RIGHT = 1 << 9,
	CORNER_TOP_LEFT = 1 << 10,
	CORNER_TOP_RIGHT = 1 << 11
};

static const enum bar_type STRAIGHT_BAR_TYPE = BOTTOM | LEFT | RIGHT | TOP;
static const enum bar_type SKEWED_BAR_TYPE = SKEWED_BOTTOM | SKEWED_LEFT | SKEWED_RIGHT | SKEWED_TOP;
static const enum bar_type CORNER_BAR_TYPE = CORNER_BOTTOM_LEFT | CORNER_BOTTOM_RIGHT | CORNER_TOP_LEFT | CORNER_TOP_RIGHT;

struct wav_bar {
	enum bar_type type;

	// if corner, then start is x-coord and end is y-coord
	float start;
	float end;

	// only meaningful for skewed/corner bars
	float top_start;
	float top_end;
};

struct wav_output; // TODO: sort out circular dependency

bool create_bars(struct wav_output *output);
void render_frame(struct wav_state *state);

#endif
