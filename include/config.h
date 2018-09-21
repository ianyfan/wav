#ifndef _CONFIG_H
#define _CONFIG_H

#include <stdbool.h>
#include <stdint.h>

struct wav_config {
	int frequency_step;

	int bar_height;
	int bar_margin;
	int bar_width;
	int roundness;
	bool interpolated;
	uint32_t color;

	float diminish_rate;
	float noise_threshold;
};

void init_default_config(struct wav_config *config);
int parse_config(struct wav_config *config, int argc, char **argv);
// void finish_config(struct wav_config *config);

#endif
