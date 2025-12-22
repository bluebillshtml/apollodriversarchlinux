// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Control Library Header
 */

#ifndef _APOLLO_CONTROL_H
#define _APOLLO_CONTROL_H

#include <stdint.h>

#define APOLLO_MAX_CHANNELS 8
#define APOLLO_CONFIG_FILE "/etc/apollo.conf"

/* Channel types */
enum apollo_channel_type {
	APOLLO_CHAN_ANALOG,
	APOLLO_CHAN_DIGITAL,
	APOLLO_CHAN_SPDIF,
	APOLLO_CHAN_ADAT,
};

/* Input source selection */
enum apollo_input_source {
	APOLLO_INPUT_ANALOG1,
	APOLLO_INPUT_ANALOG2,
	APOLLO_INPUT_ANALOG3,
	APOLLO_INPUT_ANALOG4,
	APOLLO_INPUT_DIGITAL1,
	APOLLO_INPUT_DIGITAL2,
};

/* Monitor output selection */
enum apollo_monitor_source {
	APOLLO_MONITOR_MAIN,
	APOLLO_MONITOR_ALT,
	APOLLO_MONITOR_CUE,
};

/* Device configuration */
struct apollo_config {
	/* Gain settings (dB) */
	float analog_gain[4];  /* Analog inputs 1-4 */
	float output_gain[2];  /* Main outputs L/R */

	/* Input configuration */
	enum apollo_input_source input_source[APOLLO_MAX_CHANNELS];

	/* Phantom power */
	uint8_t phantom_power[4];  /* Per analog input */

	/* High-pass filter */
	uint8_t hpf_enabled[APOLLO_MAX_CHANNELS];
	float hpf_freq[APOLLO_MAX_CHANNELS];  /* Hz */

	/* Pad settings (-20dB) */
	uint8_t pad_enabled[4];  /* Per analog input */

	/* Monitor settings */
	enum apollo_monitor_source monitor_source;
	float monitor_gain;
};

/* Control interface handle */
struct apollo_control;

/* API Functions */
struct apollo_control *apollo_control_init(void);
void apollo_control_cleanup(struct apollo_control *control);

int apollo_control_load_config(struct apollo_control *control, struct apollo_config *config);
int apollo_control_save_config(struct apollo_control *control, const struct apollo_config *config);
void apollo_control_default_config(struct apollo_config *config);

int apollo_control_set_analog_gain(struct apollo_control *control, int channel, float gain_db);
int apollo_control_get_analog_gain(struct apollo_control *control, int channel, float *gain_db);

int apollo_control_set_phantom_power(struct apollo_control *control, int channel, int enabled);
int apollo_control_get_phantom_power(struct apollo_control *control, int channel, int *enabled);

int apollo_control_set_input_source(struct apollo_control *control, int channel,
				   enum apollo_input_source source);
int apollo_control_get_input_source(struct apollo_control *control, int channel,
				   enum apollo_input_source *source);

int apollo_control_process_events(struct apollo_control *control);

#endif /* _APOLLO_CONTROL_H */

