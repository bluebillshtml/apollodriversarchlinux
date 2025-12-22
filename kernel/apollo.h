// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Twin Driver Header
 */

#ifndef _APOLLO_H
#define _APOLLO_H

#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/version.h>
#include <sound/core.h>
#include <sound/pcm.h>

/* Device register offsets (placeholder - requires reverse engineering) */
#define APOLLO_REG_CONTROL	0x00
#define APOLLO_REG_STATUS	0x04
#define APOLLO_REG_SAMPLE_RATE	0x08
#define APOLLO_REG_FORMAT	0x0C
#define APOLLO_REG_DMA_ADDR	0x10
#define APOLLO_REG_DMA_SIZE	0x14
#define APOLLO_REG_DMA_CONTROL	0x18

/* Control commands */
#define APOLLO_CMD_START	0x01
#define APOLLO_CMD_STOP		0x02
#define APOLLO_CMD_RESET	0x03

/* Status bits */
#define APOLLO_STATUS_READY	(1 << 0)
#define APOLLO_STATUS_RUNNING	(1 << 1)
#define APOLLO_STATUS_ERROR	(1 << 2)

/* Audio formats */
#define APOLLO_FORMAT_S16_LE	0
#define APOLLO_FORMAT_S24_3LE	1
#define APOLLO_FORMAT_S32_LE	2

/* Sample rates */
#define APOLLO_RATE_44100	44100
#define APOLLO_RATE_48000	48000
#define APOLLO_RATE_88200	88200
#define APOLLO_RATE_96000	96000
#define APOLLO_RATE_176400	176400
#define APOLLO_RATE_192000	192000

struct apollo_device {
	struct pci_dev *pci;
	struct snd_card *card;
	struct snd_pcm *pcm;

	/* Device registers */
	void __iomem *regs;
	resource_size_t regs_size;

	/* DMA resources */
	dma_addr_t dma_addr;
	void *dma_area;
	size_t dma_size;

	/* Interrupt handling */
	int irq;
	atomic_t running;

	/* Device state */
	u32 sample_rate;
	u32 format;
	u32 channels;

	/* Control interface */
	struct mutex control_lock;
	wait_queue_head_t control_wait;

	/* Firmware info */
	const struct firmware *fw;
};

/* Function declarations */

/* Main driver */
irqreturn_t apollo_interrupt(int irq, void *dev_id);

/* PCM interface */
int apollo_pcm_open(struct snd_pcm_substream *substream);
int apollo_pcm_close(struct snd_pcm_substream *substream);
int apollo_pcm_ioctl(struct snd_pcm_substream *substream, unsigned int cmd, void *arg);
int apollo_pcm_hw_params(struct snd_pcm_substream *substream, struct snd_pcm_hw_params *params);
int apollo_pcm_hw_free(struct snd_pcm_substream *substream);
int apollo_pcm_prepare(struct snd_pcm_substream *substream);
int apollo_pcm_trigger(struct snd_pcm_substream *substream, int cmd);
snd_pcm_uframes_t apollo_pcm_pointer(struct snd_pcm_substream *substream);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
int apollo_pcm_copy_user(struct snd_pcm_substream *substream, int channel, unsigned long pos,
			void __user *buf, unsigned long bytes);
#endif

/* Hardware interface */
int apollo_hw_init(struct apollo_device *apollo);
void apollo_hw_suspend(struct apollo_device *apollo);
int apollo_hw_resume(struct apollo_device *apollo);
int apollo_hw_constraints(struct snd_pcm *pcm);

/* Control interface */
int apollo_control_init(struct apollo_device *apollo);
void apollo_control_cleanup(struct apollo_device *apollo);
int apollo_control_command(struct apollo_device *apollo, u32 cmd, u32 *data);

/* Utility functions */
static inline void apollo_write_reg(struct apollo_device *apollo, u32 offset, u32 value)
{
	writel(value, apollo->regs + offset);
}

static inline u32 apollo_read_reg(struct apollo_device *apollo, u32 offset)
{
	return readl(apollo->regs + offset);
}

#endif /* _APOLLO_H */

