// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Twin Hardware Interface
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include "apollo.h"

irqreturn_t apollo_interrupt(int irq, void *dev_id)
{
	struct apollo_device *apollo = dev_id;
	u32 status;

	/* Read interrupt status */
	status = apollo_read_reg(apollo, APOLLO_REG_STATUS);

	if (status & APOLLO_STATUS_ERROR) {
		dev_err(&apollo->pci->dev, "Hardware error detected\n");
		/* Handle error condition */
		atomic_set(&apollo->running, 0);
	}

	if (status & APOLLO_STATUS_READY) {
		/* DMA transfer complete */
		if (apollo->pcm) {
			snd_pcm_period_elapsed(apollo->pcm->streams[SNDRV_PCM_STREAM_PLAYBACK].substream);
			snd_pcm_period_elapsed(apollo->pcm->streams[SNDRV_PCM_STREAM_CAPTURE].substream);
		}
	}

	/* Clear interrupt */
	apollo_write_reg(apollo, APOLLO_REG_STATUS, status);

	return IRQ_HANDLED;
}

int apollo_hw_init(struct apollo_device *apollo)
{
	u32 status;
	int timeout = 100;  /* 100ms timeout */

	dev_info(&apollo->pci->dev, "Initializing Apollo Twin hardware\n");

	/* Reset device */
	apollo_write_reg(apollo, APOLLO_REG_CONTROL, APOLLO_CMD_RESET);
	msleep(10);

	/* Wait for device ready */
	while (timeout--) {
		status = apollo_read_reg(apollo, APOLLO_REG_STATUS);
		if (status & APOLLO_STATUS_READY)
			break;
		msleep(1);
	}

	if (!(status & APOLLO_STATUS_READY)) {
		dev_err(&apollo->pci->dev, "Device failed to become ready\n");
		return -ETIMEDOUT;
	}

	/* Configure default settings */
	apollo_write_reg(apollo, APOLLO_REG_SAMPLE_RATE, APOLLO_RATE_48000);
	apollo_write_reg(apollo, APOLLO_REG_FORMAT, APOLLO_FORMAT_S32_LE);

	dev_info(&apollo->pci->dev, "Apollo Twin hardware initialized\n");
	return 0;
}

void apollo_hw_suspend(struct apollo_device *apollo)
{
	dev_info(&apollo->pci->dev, "Suspending Apollo Twin hardware\n");

	/* Stop any running operations */
	atomic_set(&apollo->running, 0);
	apollo_write_reg(apollo, APOLLO_REG_DMA_CONTROL, APOLLO_CMD_STOP);
}

int apollo_hw_resume(struct apollo_device *apollo)
{
	dev_info(&apollo->pci->dev, "Resuming Apollo Twin hardware\n");

	/* Reinitialize hardware */
	return apollo_hw_init(apollo);
}

int apollo_hw_constraints(struct snd_pcm *pcm)
{
	/* Constraints are now typically set in the open() callback */
	/* This function can be simplified or removed in newer kernels */
	return 0;
}


