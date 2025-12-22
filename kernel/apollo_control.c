// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Twin Control Interface
 */

#include <linux/module.h>
#include <sound/core.h>
#include <sound/control.h>
#include "apollo.h"

/* Forward declarations for ALSA control callbacks */
static int apollo_ctl_master_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo);
static int apollo_ctl_master_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *uvalue);
static int apollo_ctl_master_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *uvalue);
static int apollo_ctl_input_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo);
static int apollo_ctl_input_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *uvalue);
static int apollo_ctl_input_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *uvalue);

int apollo_control_init(struct apollo_device *apollo)
{
	struct snd_kcontrol_new controls[] = {
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "Master Playback Volume",
			.index = 0,
			.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
			.info = apollo_ctl_master_info,
			.get = apollo_ctl_master_get,
			.put = apollo_ctl_master_put,
		},
		{
			.iface = SNDRV_CTL_ELEM_IFACE_MIXER,
			.name = "Input Source",
			.index = 0,
			.access = SNDRV_CTL_ELEM_ACCESS_READWRITE,
			.info = apollo_ctl_input_info,
			.get = apollo_ctl_input_get,
			.put = apollo_ctl_input_put,
		},
	};

	int i, err;

	for (i = 0; i < ARRAY_SIZE(controls); i++) {
		err = snd_ctl_add(apollo->card, snd_ctl_new1(&controls[i], apollo));
		if (err < 0) {
			dev_err(&apollo->pci->dev, "Failed to add control %s\n",
				controls[i].name);
			return err;
		}
	}

	return 0;
}

void apollo_control_cleanup(struct apollo_device *apollo)
{
	/* Controls are automatically removed when card is freed */
}

int apollo_control_command(struct apollo_device *apollo, u32 cmd, u32 *data)
{
	int ret;

	mutex_lock(&apollo->control_lock);

	/* Send command to device */
	apollo_write_reg(apollo, APOLLO_REG_CONTROL, cmd);

	if (data) {
		/* Wait for response or timeout */
		ret = wait_event_timeout(apollo->control_wait,
					apollo_read_reg(apollo, APOLLO_REG_STATUS) & APOLLO_STATUS_READY,
					msecs_to_jiffies(100));
		if (!ret) {
			mutex_unlock(&apollo->control_lock);
			return -ETIMEDOUT;
		}

		*data = apollo_read_reg(apollo, APOLLO_REG_STATUS);
	}

	mutex_unlock(&apollo->control_lock);
	return 0;
}

/* ALSA Control Interface */

static int apollo_ctl_master_info(struct snd_kcontrol *kcontrol,
				 struct snd_ctl_elem_info *uinfo)
{
	uinfo->type = SNDRV_CTL_ELEM_TYPE_INTEGER;
	uinfo->count = 2;  /* Left and right channels */
	uinfo->value.integer.min = 0;
	uinfo->value.integer.max = 100;  /* 0-100% */
	return 0;
}

static int apollo_ctl_master_get(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *uvalue)
{
	struct apollo_device *apollo = snd_kcontrol_chip(kcontrol);

	/* Read current master volume from device */
	/* Placeholder - implement based on reverse engineered protocol */
	uvalue->value.integer.value[0] = 75;  /* Left */
	uvalue->value.integer.value[1] = 75;  /* Right */

	return 0;
}

static int apollo_ctl_master_put(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_value *uvalue)
{
	struct apollo_device *apollo = snd_kcontrol_chip(kcontrol);

	/* Set master volume on device */
	/* Placeholder - implement based on reverse engineered protocol */
	dev_info(&apollo->pci->dev, "Setting master volume: L=%ld R=%ld\n",
		 uvalue->value.integer.value[0], uvalue->value.integer.value[1]);

	return 0;
}

static int apollo_ctl_input_info(struct snd_kcontrol *kcontrol,
				struct snd_ctl_elem_info *uinfo)
{
	static const char *const input_names[] = {
		"Analog 1", "Analog 2", "Analog 3", "Analog 4",
		"Digital 1", "Digital 2"
	};

	uinfo->type = SNDRV_CTL_ELEM_TYPE_ENUMERATED;
	uinfo->count = 1;
	uinfo->value.enumerated.items = ARRAY_SIZE(input_names);
	if (uinfo->value.enumerated.item >= ARRAY_SIZE(input_names))
		uinfo->value.enumerated.item = ARRAY_SIZE(input_names) - 1;
	strcpy(uinfo->value.enumerated.name,
	       input_names[uinfo->value.enumerated.item]);
	return 0;
}

static int apollo_ctl_input_get(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *uvalue)
{
	struct apollo_device *apollo = snd_kcontrol_chip(kcontrol);

	/* Read current input selection */
	/* Placeholder - implement based on reverse engineered protocol */
	uvalue->value.enumerated.item[0] = 0;  /* Analog 1 */

	return 0;
}

static int apollo_ctl_input_put(struct snd_kcontrol *kcontrol,
			       struct snd_ctl_elem_value *uvalue)
{
	struct apollo_device *apollo = snd_kcontrol_chip(kcontrol);

	/* Set input selection */
	/* Placeholder - implement based on reverse engineered protocol */
	dev_info(&apollo->pci->dev, "Setting input: %d\n",
		 uvalue->value.enumerated.item[0]);

	return 0;
}

