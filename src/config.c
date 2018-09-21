#include "config.h"

#include <errno.h>
#include <getopt.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void init_default_config(struct wav_config *config) {
	config->frequency_step = 10;
	config->bar_height = 16;
	config->bar_margin = 1;
	config->bar_width = 8;
	config->roundness = 2;
	config->interpolated = false;
	config->diminish_rate = 0.5;
	config->noise_threshold = 0.5;
}

static bool parse_int(const char *string, int *out) {
	errno = 0;
	char *end;
	*out = (int) strtoul(string, &end, 10);
	return errno == 0 && *end == '\0';
}

static bool parse_color(const char *string, uint32_t *out) {
	if (*string++ != '#') return false;

	size_t len = strlen(string);
	if (len != 6 && len != 8) {
		return false;
	}

	errno = 0;
	char *end;
	*out = (uint32_t) strtoul(string, &end, 16);
	if (errno != 0 || *end != '\0') return false;

	if (len == 6) *out |= 0xff000000;

	return true;
}

static bool parse_option(const char c, const char *value, struct wav_config *config) {
	switch (c) {
		case 'f': return parse_int(optarg, &config->frequency_step);
		case 'H': return parse_int(optarg, &config->bar_height);
		case 'm': return parse_int(optarg, &config->bar_margin);
		case 'w': return parse_int(optarg, &config->bar_width);
		case 'r': return parse_int(optarg, &config->roundness);
		case 'i': return false;
		case 'c': return parse_color(optarg, &config->color);
		// case 'd': return parse_float(optarg, &config->diminish_rate);
		// case 'n': return parse_float(optarg, &config->noise_threshold);
		default: return false;
	}
}

int parse_config(struct wav_config *config, int argc, char **argv) {
	static const struct option long_options[] = {
		{"help", no_argument, NULL, 'h'},
		{"frequency-step", required_argument, NULL, 'f'},
		{"height", required_argument, NULL, 'H'},
		{"margin", required_argument, NULL, 'm'},
		{"width", required_argument, NULL, 'w'},
		{"roundness", required_argument, NULL, 'r'},
		{"interpolated", no_argument, NULL, 'i'},
		{"color", no_argument, NULL, 'c'},
		{"diminish-rate", required_argument, NULL, 'd'},
		{"noise-threshold", required_argument, NULL, 'n'},
		{"output", required_argument, NULL, 'o'},
		{0}
	};

	int number_of_outputs = 0;
	while (true) {
		int c = getopt_long(argc, argv, "hf:H:m:w:r:id:n:o:", long_options, NULL);
		if (c == -1) break;
		if (c == ':' || c == '?') return -1;
		if (c == 'h') return 1;

		if (c == 'o') ++number_of_outputs;
	}

	// if (number_of_outputs > 0) config->outputs = string_list_init(number_of_outputs);
	optind = 1;
	while (true) {
		int option_index = -1;
		int c = getopt_long(argc, argv, "hf:H:m:w:r:id:n:o:", long_options, &option_index);
		if (c == -1) return 0;

		if (!parse_option(c, optarg, config)) {
			if (option_index != -1) {
				fprintf(stderr, "Invalid argument for option '--%s': '%s'\n", long_options[option_index].name, optarg);
			} else {
				fprintf(stderr, "Invalid argument for option '-%c': '%s'\n", c, optarg);
			}
			return -1;
		}
	}
}
