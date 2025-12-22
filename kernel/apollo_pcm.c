// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Twin PCM Interface
 */

#include <linux/module.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/pcm_params.h>
#include "apollo.h"

static const struct snd_pcm_hardware apollo_pcm_hardware = {
	.info = (SNDRV_PCM_INFO_MMAP |
		 SNDRV_PCM_INFO_INTERLEAVED |
		 SNDRV_PCM_INFO_BLOCK_TRANSFER |
		 SNDRV_PCM_INFO_MMAP_VALID),
	.formats = (SNDRV_PCM_FMTBIT_S16_LE |
		    SNDRV_PCM_FMTBIT_S24_3LE |
		    SNDRV_PCM_FMTBIT_S32_LE),
	.rates = (SNDRV_PCM_RATE_44100 |
		  SNDRV_PCM_RATE_48000 |
		  SNDRV_PCM_RATE_88200 |
		  SNDRV_PCM_RATE_96000 |
		  SNDRV_PCM_RATE_176400 |
		  SNDRV_PCM_RATE_192000),
	.rate_min = 44100,
	.rate_max = 192000,
	.channels_min = 2,
	.channels_max = 8,
	.buffer_bytes_max = 1024 * 1024,  /* 1MB */
	.period_bytes_min = 64,
	.period_bytes_max = 1024 * 512,   /* 512KB */
	.periods_min = 2,
	.periods_max = 32,
	.fifo_size = 0,
};

int apollo_pcm_open(struct snd_pcm_substream *substream)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	dev_dbg(&apollo->pci->dev, "PCM open: stream %d\n", substream->stream);

	runtime->hw = apollo_pcm_hardware;

	/* Set initial device state */
	apollo->sample_rate = 48000;
	apollo->format = APOLLO_FORMAT_S32_LE;
	apollo->channels = 2;

	return 0;
}

int apollo_pcm_close(struct snd_pcm_substream *substream)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);

	dev_dbg(&apollo->pci->dev, "PCM close: stream %d\n", substream->stream);

	/* Stop any running transfers */
	atomic_set(&apollo->running, 0);

	return 0;
}

int apollo_pcm_ioctl(struct snd_pcm_substream *substream, unsigned int cmd, void *arg)
{
	dev_dbg(substream->pcm->card->dev, "PCM ioctl: 0x%x\n", cmd);

	return snd_pcm_lib_ioctl(substream, cmd, arg);
}

int apollo_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);

	dev_dbg(&apollo->pci->dev, "PCM hw_params\n");

	/* Extract parameters */
	apollo->sample_rate = params_rate(params);
	apollo->channels = params_channels(params);

	switch (params_format(params)) {
	case SNDRV_PCM_FORMAT_S16_LE:
		apollo->format = APOLLO_FORMAT_S16_LE;
		break;
	case SNDRV_PCM_FORMAT_S24_3LE:
		apollo->format = APOLLO_FORMAT_S24_3LE;
		break;
	case SNDRV_PCM_FORMAT_S32_LE:
		apollo->format = APOLLO_FORMAT_S32_LE;
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int apollo_pcm_hw_free(struct snd_pcm_substream *substream)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);

	dev_dbg(&apollo->pci->dev, "PCM hw_free\n");

	/* Reset device state */
	atomic_set(&apollo->running, 0);

	return 0;
}

int apollo_pcm_prepare(struct snd_pcm_substream *substream)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;

	dev_dbg(&apollo->pci->dev, "PCM prepare\n");

	/* Configure device registers */
	apollo_write_reg(apollo, APOLLO_REG_SAMPLE_RATE, apollo->sample_rate);
	apollo_write_reg(apollo, APOLLO_REG_FORMAT, apollo->format);

	/* Set up DMA */
	apollo_write_reg(apollo, APOLLO_REG_DMA_ADDR, lower_32_bits(apollo->dma_addr));
	apollo_write_reg(apollo, APOLLO_REG_DMA_SIZE, runtime->dma_bytes);

	return 0;
}

int apollo_pcm_trigger(struct snd_pcm_substream *substream, int cmd)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);

	dev_dbg(&apollo->pci->dev, "PCM trigger: %d\n", cmd);

	switch (cmd) {
	case SNDRV_PCM_TRIGGER_START:
		atomic_set(&apollo->running, 1);
		apollo_write_reg(apollo, APOLLO_REG_DMA_CONTROL, APOLLO_CMD_START);
		break;
	case SNDRV_PCM_TRIGGER_STOP:
		atomic_set(&apollo->running, 0);
		apollo_write_reg(apollo, APOLLO_REG_DMA_CONTROL, APOLLO_CMD_STOP);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

snd_pcm_uframes_t apollo_pcm_pointer(struct snd_pcm_substream *substream)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	u32 position;

	/* Read current DMA position from device */
	position = apollo_read_reg(apollo, APOLLO_REG_DMA_ADDR);

	/* Convert to frames */
	return bytes_to_frames(runtime, position - lower_32_bits(apollo->dma_addr));
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
int apollo_pcm_copy_user(struct snd_pcm_substream *substream, int channel,
			unsigned long pos, void __user *buf, unsigned long bytes)
{
	struct apollo_device *apollo = snd_pcm_substream_chip(substream);
	struct snd_pcm_runtime *runtime = substream->runtime;
	void *dma_ptr;

	/* Calculate DMA buffer position */
	dma_ptr = apollo->dma_area + frames_to_bytes(runtime, pos);

	if (substream->stream == SNDRV_PCM_STREAM_PLAYBACK) {
		/* Copy from user space to DMA buffer */
		if (copy_from_user(dma_ptr, buf, bytes))
			return -EFAULT;
	} else {
		/* Copy from DMA buffer to user space */
		if (copy_to_user(buf, dma_ptr, bytes))
			return -EFAULT;
	}

	return 0;
}
#endif

