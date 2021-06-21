/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include "roc_api.h"
#include "roc_bphy_irq.h"

#define ROC_BPHY_MEMZONE_NAME "roc_bphy_mz"
#define ROC_BPHY_CTR_DEV_PATH "/dev/otx-bphy-ctr"

#define ROC_BPHY_IOC_MAGIC 0xF3
#define ROC_BPHY_IOC_GET_BPHY_MAX_IRQ	_IOR(ROC_BPHY_IOC_MAGIC, 3, uint64_t)
#define ROC_BPHY_IOC_GET_BPHY_BMASK_IRQ _IOR(ROC_BPHY_IOC_MAGIC, 4, uint64_t)

struct roc_bphy_irq_chip *
roc_bphy_intr_init(void)
{
	struct roc_bphy_irq_chip *irq_chip;
	uint64_t max_irq, i, avail_irqs;
	int fd, ret;

	fd = open(ROC_BPHY_CTR_DEV_PATH, O_RDWR | O_SYNC);
	if (fd < 0) {
		plt_err("Failed to open %s", ROC_BPHY_CTR_DEV_PATH);
		return NULL;
	}

	ret = ioctl(fd, ROC_BPHY_IOC_GET_BPHY_MAX_IRQ, &max_irq);
	if (ret < 0) {
		plt_err("Failed to get max irq number via ioctl");
		goto err_ioctl;
	}

	ret = ioctl(fd, ROC_BPHY_IOC_GET_BPHY_BMASK_IRQ, &avail_irqs);
	if (ret < 0) {
		plt_err("Failed to get available irqs bitmask via ioctl");
		goto err_ioctl;
	}

	irq_chip = plt_zmalloc(sizeof(*irq_chip), 0);
	if (irq_chip == NULL) {
		plt_err("Failed to alloc irq_chip");
		goto err_alloc_chip;
	}

	irq_chip->intfd = fd;
	irq_chip->max_irq = max_irq;
	irq_chip->avail_irq_bmask = avail_irqs;
	irq_chip->irq_vecs =
		plt_zmalloc(irq_chip->max_irq * sizeof(*irq_chip->irq_vecs), 0);
	if (irq_chip->irq_vecs == NULL) {
		plt_err("Failed to alloc irq_chip irq_vecs");
		goto err_alloc_irq;
	}

	irq_chip->mz_name = plt_zmalloc(strlen(ROC_BPHY_MEMZONE_NAME) + 1, 0);
	if (irq_chip->mz_name == NULL) {
		plt_err("Failed to alloc irq_chip name");
		goto err_alloc_name;
	}
	plt_strlcpy(irq_chip->mz_name, ROC_BPHY_MEMZONE_NAME,
		    strlen(ROC_BPHY_MEMZONE_NAME) + 1);

	for (i = 0; i < irq_chip->max_irq; i++) {
		irq_chip->irq_vecs[i].fd = -1;
		irq_chip->irq_vecs[i].handler_cpu = -1;
	}

	return irq_chip;

err_alloc_name:
	plt_free(irq_chip->irq_vecs);

err_alloc_irq:
	plt_free(irq_chip);

err_ioctl:
err_alloc_chip:
	close(fd);
	return NULL;
}

void
roc_bphy_intr_fini(struct roc_bphy_irq_chip *irq_chip)
{
	if (irq_chip == NULL)
		return;

	close(irq_chip->intfd);
	plt_free(irq_chip->mz_name);
	plt_free(irq_chip->irq_vecs);
	plt_free(irq_chip);
}

bool
roc_bphy_intr_available(struct roc_bphy_irq_chip *irq_chip, int irq_num)
{
	if (irq_num < 0 || (uint64_t)irq_num >= irq_chip->max_irq)
		return false;

	return irq_chip->avail_irq_bmask & BIT(irq_num);
}
