// SPDX-License-Identifier: GPL-2.0-only
/*
 * Apollo Twin ALSA Driver
 *
 * Core driver for Universal Audio Apollo Twin Thunderbolt audio interface.
 * Provides ALSA PCM interface for audio I/O and control interface for device
 * configuration.
 */

#include <linux/module.h>
#include <linux/pci.h>
#include <linux/interrupt.h>
#include <linux/dma-mapping.h>
#include <linux/firmware.h>
#include <sound/core.h>
#include <sound/pcm.h>
#include <sound/control.h>
#include "apollo.h"

#define DRIVER_NAME "apollo"
#define DRIVER_DESC "Universal Audio Apollo Twin ALSA Driver"

#define APOLLO_VENDOR_ID 0x1176  /* Universal Audio */
#define APOLLO_DEVICE_ID 0x0005  /* Apollo Twin MkII */

/* Device capabilities */
#define APOLLO_MAX_CHANNELS 8
#define APOLLO_MAX_BUFFER_SIZE (1024 * 1024)  /* 1MB */
#define APOLLO_MAX_PERIODS 32

static const struct pci_device_id apollo_ids[] = {
	{ PCI_DEVICE(APOLLO_VENDOR_ID, APOLLO_DEVICE_ID) },
	{ 0, }
};
MODULE_DEVICE_TABLE(pci, apollo_ids);

/* PCI driver operations */
static int apollo_probe(struct pci_dev *pci, const struct pci_device_id *id);
static void apollo_remove(struct pci_dev *pci);
static int apollo_suspend(struct pci_dev *pci, pm_message_t state);
static int apollo_resume(struct pci_dev *pci);

static struct pci_driver apollo_driver = {
	.name = DRIVER_NAME,
	.id_table = apollo_ids,
	.probe = apollo_probe,
	.remove = apollo_remove,
	.suspend = apollo_suspend,
	.resume = apollo_resume,
};

/* ALSA PCM operations */
static struct snd_pcm_ops apollo_pcm_ops = {
	.open = apollo_pcm_open,
	.close = apollo_pcm_close,
	.ioctl = apollo_pcm_ioctl,
	.hw_params = apollo_pcm_hw_params,
	.hw_free = apollo_pcm_hw_free,
	.prepare = apollo_pcm_prepare,
	.trigger = apollo_pcm_trigger,
	.pointer = apollo_pcm_pointer,
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 5, 0)
	.copy_user = apollo_pcm_copy_user,
#endif
};

static int apollo_probe(struct pci_dev *pci, const struct pci_device_id *id)
{
	struct apollo_device *apollo;
	struct snd_card *card;
	int err;

	dev_info(&pci->dev, "Apollo Twin PCI probe: vendor=0x%04x device=0x%04x\n",
		 pci->vendor, pci->device);

	/* Allocate device structure */
	apollo = devm_kzalloc(&pci->dev, sizeof(*apollo), GFP_KERNEL);
	if (!apollo)
		return -ENOMEM;

	apollo->pci = pci;
	pci_set_drvdata(pci, apollo);

	/* Enable PCI device */
	err = pci_enable_device(pci);
	if (err) {
		dev_err(&pci->dev, "Failed to enable PCI device\n");
		return err;
	}

	/* Request memory regions */
	err = pci_request_regions(pci, DRIVER_NAME);
	if (err) {
		dev_err(&pci->dev, "Failed to request PCI regions\n");
		goto disable_pci;
	}

	/* Map device registers */
	apollo->regs = pci_ioremap_bar(pci, 0);
	if (!apollo->regs) {
		dev_err(&pci->dev, "Failed to map device registers\n");
		err = -EIO;
		goto release_regions;
	}
	apollo->regs_size = pci_resource_len(pci, 0);

	/* Enable bus mastering and DMA */
	pci_set_master(pci);

	/* Allocate DMA buffer */
	apollo->dma_size = APOLLO_MAX_BUFFER_SIZE;
	apollo->dma_area = dma_alloc_coherent(&pci->dev, apollo->dma_size,
					     &apollo->dma_addr, GFP_KERNEL);
	if (!apollo->dma_area) {
		dev_err(&pci->dev, "Failed to allocate DMA buffer\n");
		err = -ENOMEM;
		goto unmap_regs;
	}

	/* Initialize device state */
	atomic_set(&apollo->running, 0);
	mutex_init(&apollo->control_lock);
	init_waitqueue_head(&apollo->control_wait);

	/* Create ALSA card */
	err = snd_card_new(&pci->dev, -1, DRIVER_NAME,
			  THIS_MODULE, 0, &card);
	if (err) {
		dev_err(&pci->dev, "Failed to create ALSA card\n");
		goto free_dma;
	}

	apollo->card = card;
	strcpy(card->driver, DRIVER_NAME);
	strcpy(card->shortname, "Apollo Twin");
	strcpy(card->longname, "Universal Audio Apollo Twin");

	/* Create PCM device */
	err = snd_pcm_new(card, "Apollo Twin PCM", 0, 1, 1, &apollo->pcm);
	if (err) {
		dev_err(&pci->dev, "Failed to create PCM device\n");
		goto free_card;
	}

	apollo->pcm->private_data = apollo;
	strcpy(apollo->pcm->name, "Apollo Twin PCM");

	/* Configure PCM hardware parameters */
	snd_pcm_set_ops(apollo->pcm, SNDRV_PCM_STREAM_PLAYBACK, &apollo_pcm_ops);
	snd_pcm_set_ops(apollo->pcm, SNDRV_PCM_STREAM_CAPTURE, &apollo_pcm_ops);

	/* Set up hardware constraints */
	err = apollo_hw_constraints(apollo->pcm);
	if (err) {
		dev_err(&pci->dev, "Failed to set hardware constraints\n");
		goto free_card;
	}

	/* Register the card */
	err = snd_card_register(card);
	if (err) {
		dev_err(&pci->dev, "Failed to register ALSA card\n");
		goto free_card;
	}

	/* Request interrupt */
	err = request_irq(pci->irq, apollo_interrupt, IRQF_SHARED,
			 DRIVER_NAME, apollo);
	if (err) {
		dev_err(&pci->dev, "Failed to request IRQ\n");
		goto unregister_card;
	}
	apollo->irq = pci->irq;

	/* Initialize hardware */
	err = apollo_hw_init(apollo);
	if (err) {
		dev_err(&pci->dev, "Failed to initialize hardware\n");
		goto free_irq;
	}

	dev_info(&pci->dev, "Apollo Twin initialized successfully\n");
	return 0;

free_irq:
	free_irq(apollo->irq, apollo);
unregister_card:
	snd_card_free(card);
free_card:
free_dma:
	dma_free_coherent(&pci->dev, apollo->dma_size, apollo->dma_area,
			 apollo->dma_addr);
unmap_regs:
	iounmap(apollo->regs);
release_regions:
	pci_release_regions(pci);
disable_pci:
	pci_disable_device(pci);
	return err;
}

static void apollo_remove(struct pci_dev *pci)
{
	struct apollo_device *apollo = pci_get_drvdata(pci);

	dev_info(&pci->dev, "Removing Apollo Twin driver\n");

	if (apollo->irq)
		free_irq(apollo->irq, apollo);

	if (apollo->card)
		snd_card_free(apollo->card);

	if (apollo->dma_area)
		dma_free_coherent(&pci->dev, apollo->dma_size, apollo->dma_area,
				 apollo->dma_addr);

	if (apollo->regs)
		iounmap(apollo->regs);

	pci_release_regions(pci);
	pci_disable_device(pci);
}

static int apollo_suspend(struct pci_dev *pci, pm_message_t state)
{
	struct apollo_device *apollo = pci_get_drvdata(pci);

	dev_info(&pci->dev, "Suspending Apollo Twin\n");

	atomic_set(&apollo->running, 0);
	apollo_hw_suspend(apollo);

	return 0;
}

static int apollo_resume(struct pci_dev *pci)
{
	struct apollo_device *apollo = pci_get_drvdata(pci);
	int err;

	dev_info(&pci->dev, "Resuming Apollo Twin\n");

	err = apollo_hw_resume(apollo);
	if (err)
		return err;

	return 0;
}

static int __init apollo_init(void)
{
	int err;

	pr_info(DRIVER_DESC " loading\n");

	err = pci_register_driver(&apollo_driver);
	if (err) {
		pr_err("Failed to register PCI driver\n");
		return err;
	}

	return 0;
}

static void __exit apollo_exit(void)
{
	pr_info(DRIVER_DESC " unloading\n");
	pci_unregister_driver(&apollo_driver);
}

module_init(apollo_init);
module_exit(apollo_exit);

MODULE_AUTHOR("Apollo Driver Team");
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1.0");

