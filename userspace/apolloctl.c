// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Control CLI Tool
 *
 * Command-line interface for controlling Apollo Twin device parameters
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "apollo_control.h"

#define VERSION "0.1.0"
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static void print_usage(const char *program_name)
{
	printf("Apollo Twin Control Tool v%s\n\n", VERSION);
	printf("Usage: %s <command> [options]\n\n", program_name);
	printf("Commands:\n");
	printf("  gain <channel> <value>        Set analog input gain (dB)\n");
	printf("  gain <channel>                Get analog input gain\n");
	printf("  phantom <channel> <on|off>    Set phantom power\n");
	printf("  phantom <channel>             Get phantom power status\n");
	printf("  input <channel> <source>      Set input source\n");
	printf("  input <channel>               Get input source\n");
	printf("  monitor <source>              Set monitor source\n");
	printf("  monitor                       Get monitor source\n");
	printf("  save <preset>                 Save current settings\n");
	printf("  load <preset>                 Load settings from preset\n");
	printf("  status                        Show device status\n");
	printf("  help                          Show this help\n\n");
	printf("Channels: 1-4 (analog inputs)\n");
	printf("Sources: analog1, analog2, analog3, analog4, digital1, digital2\n");
	printf("Monitor: main, alt, cue\n");
}

static int cmd_gain(struct apollo_control *control, int argc, char *argv[])
{
	int channel;
	float gain;

	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	channel = atoi(argv[1]);
	if (channel < 1 || channel > 4) {
		fprintf(stderr, "Invalid channel: %d\n", channel);
		return EXIT_FAILURE;
	}

	if (argc == 3) {
		/* Set gain */
		gain = atof(argv[2]);
		if (apollo_control_set_analog_gain(control, channel, gain) < 0) {
			fprintf(stderr, "Failed to set gain\n");
			return EXIT_FAILURE;
		}
		printf("Set analog input %d gain to %.1f dB\n", channel, gain);
	} else {
		/* Get gain */
		if (apollo_control_get_analog_gain(control, channel, &gain) < 0) {
			fprintf(stderr, "Failed to get gain\n");
			return EXIT_FAILURE;
		}
		printf("Analog input %d gain: %.1f dB\n", channel, gain);
	}

	return EXIT_SUCCESS;
}

static int cmd_phantom(struct apollo_control *control, int argc, char *argv[])
{
	int channel, enabled;

	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	channel = atoi(argv[1]);
	if (channel < 1 || channel > 4) {
		fprintf(stderr, "Invalid channel: %d\n", channel);
		return EXIT_FAILURE;
	}

	if (argc == 3) {
		/* Set phantom power */
		if (strcmp(argv[2], "on") == 0) {
			enabled = 1;
		} else if (strcmp(argv[2], "off") == 0) {
			enabled = 0;
		} else {
			fprintf(stderr, "Invalid value: %s (use 'on' or 'off')\n", argv[2]);
			return EXIT_FAILURE;
		}

		if (apollo_control_set_phantom_power(control, channel, enabled) < 0) {
			fprintf(stderr, "Failed to set phantom power\n");
			return EXIT_FAILURE;
		}
		printf("Set phantom power for channel %d to %s\n", channel, enabled ? "on" : "off");
	} else {
		/* Get phantom power */
		if (apollo_control_get_phantom_power(control, channel, &enabled) < 0) {
			fprintf(stderr, "Failed to get phantom power status\n");
			return EXIT_FAILURE;
		}
		printf("Phantom power for channel %d: %s\n", channel, enabled ? "on" : "off");
	}

	return EXIT_SUCCESS;
}

static int cmd_input(struct apollo_control *control, int argc, char *argv[])
{
	int channel;
	enum apollo_input_source source;
	const char *source_names[] = {
		"analog1", "analog2", "analog3", "analog4",
		"digital1", "digital2"
	};

	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	channel = atoi(argv[1]);
	if (channel < 1 || channel > APOLLO_MAX_CHANNELS) {
		fprintf(stderr, "Invalid channel: %d\n", channel);
		return EXIT_FAILURE;
	}

	if (argc == 3) {
		/* Set input source */
		int i;
		for (i = 0; i < ARRAY_SIZE(source_names); i++) {
			if (strcmp(argv[2], source_names[i]) == 0) {
				source = i;
				break;
			}
		}
		if (i == ARRAY_SIZE(source_names)) {
			fprintf(stderr, "Invalid source: %s\n", argv[2]);
			return EXIT_FAILURE;
		}

		if (apollo_control_set_input_source(control, channel, source) < 0) {
			fprintf(stderr, "Failed to set input source\n");
			return EXIT_FAILURE;
		}
		printf("Set channel %d input to %s\n", channel, source_names[source]);
	} else {
		/* Get input source */
		if (apollo_control_get_input_source(control, channel, &source) < 0) {
			fprintf(stderr, "Failed to get input source\n");
			return EXIT_FAILURE;
		}
		if (source < ARRAY_SIZE(source_names)) {
			printf("Channel %d input: %s\n", channel, source_names[source]);
		}
	}

	return EXIT_SUCCESS;
}

static int cmd_monitor(struct apollo_control *control, int argc, char *argv[])
{
	const char *monitor_names[] = { "main", "alt", "cue" };

	if (argc == 2) {
		/* Set monitor source */
		int i;
		for (i = 0; i < ARRAY_SIZE(monitor_names); i++) {
			if (strcmp(argv[1], monitor_names[i]) == 0) {
				/* Placeholder - implement monitor control */
				printf("Set monitor source to %s\n", monitor_names[i]);
				return EXIT_SUCCESS;
			}
		}
		fprintf(stderr, "Invalid monitor source: %s\n", argv[1]);
		return EXIT_FAILURE;
	} else {
		/* Get monitor source */
		printf("Monitor source: main\n");  /* Placeholder */
	}

	return EXIT_SUCCESS;
}

static int cmd_save(struct apollo_control *control, int argc, char *argv[])
{
	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* Placeholder - implement preset saving */
	printf("Saving settings to preset: %s\n", argv[1]);
	return EXIT_SUCCESS;
}

static int cmd_load(struct apollo_control *control, int argc, char *argv[])
{
	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* Placeholder - implement preset loading */
	printf("Loading settings from preset: %s\n", argv[1]);
	return EXIT_SUCCESS;
}

static int cmd_status(struct apollo_control *control, int argc, char *argv[])
{
	int i;
	float gain;

	printf("Apollo Twin Status\n");
	printf("==================\n\n");

	/* Show analog gains */
	printf("Analog Input Gains:\n");
	for (i = 1; i <= 4; i++) {
		if (apollo_control_get_analog_gain(control, i, &gain) == 0) {
			printf("  Channel %d: %.1f dB\n", i, gain);
		} else {
			printf("  Channel %d: Error\n", i);
		}
	}

	/* Show phantom power status */
	printf("\nPhantom Power:\n");
	for (i = 1; i <= 4; i++) {
		int enabled;
		if (apollo_control_get_phantom_power(control, i, &enabled) == 0) {
			printf("  Channel %d: %s\n", i, enabled ? "ON" : "OFF");
		} else {
			printf("  Channel %d: Unknown\n", i);
		}
	}

	return EXIT_SUCCESS;
}

int main(int argc, char *argv[])
{
	struct apollo_control *control;
	int ret = EXIT_FAILURE;

	if (argc < 2) {
		print_usage(argv[0]);
		return EXIT_FAILURE;
	}

	/* Initialize control interface */
	control = apollo_control_init();
	if (!control) {
		fprintf(stderr, "Failed to initialize Apollo control interface\n");
		return EXIT_FAILURE;
	}

	/* Process command */
	if (strcmp(argv[1], "gain") == 0) {
		ret = cmd_gain(control, argc - 1, argv + 1);
	} else if (strcmp(argv[1], "phantom") == 0) {
		ret = cmd_phantom(control, argc - 1, argv + 1);
	} else if (strcmp(argv[1], "input") == 0) {
		ret = cmd_input(control, argc - 1, argv + 1);
	} else if (strcmp(argv[1], "monitor") == 0) {
		ret = cmd_monitor(control, argc - 1, argv + 1);
	} else if (strcmp(argv[1], "save") == 0) {
		ret = cmd_save(control, argc - 1, argv + 1);
	} else if (strcmp(argv[1], "load") == 0) {
		ret = cmd_load(control, argc - 1, argv + 1);
	} else if (strcmp(argv[1], "status") == 0) {
		ret = cmd_status(control, argc - 1, argv + 1);
	} else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "-h") == 0) {
		print_usage(argv[0]);
		ret = EXIT_SUCCESS;
	} else {
		fprintf(stderr, "Unknown command: %s\n", argv[1]);
		print_usage(argv[0]);
	}

	apollo_control_cleanup(control);
	return ret;
}

