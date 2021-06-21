/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/queue.h>
#include <unistd.h>

#include "roc_api.h"
#include "roc_bphy_irq.h"

struct roc_bphy_irq_stack {
	STAILQ_ENTRY(roc_bphy_irq_stack) entries;
	void *sp_buffer;
	int cpu;
	int inuse;
};

#define ROC_BPHY_MEMZONE_NAME "roc_bphy_mz"
#define ROC_BPHY_CTR_DEV_PATH "/dev/otx-bphy-ctr"

#define ROC_BPHY_IOC_MAGIC 0xF3
#define ROC_BPHY_IOC_GET_BPHY_MAX_IRQ	_IOR(ROC_BPHY_IOC_MAGIC, 3, uint64_t)
#define ROC_BPHY_IOC_GET_BPHY_BMASK_IRQ _IOR(ROC_BPHY_IOC_MAGIC, 4, uint64_t)

static STAILQ_HEAD(slisthead, roc_bphy_irq_stack)
	irq_stacks = STAILQ_HEAD_INITIALIZER(irq_stacks);

/* Note: it is assumed that as for now there is no multiprocess support */
static pthread_mutex_t stacks_mutex = PTHREAD_MUTEX_INITIALIZER;

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

void *
roc_bphy_irq_stack_get(int cpu)
{
#define ARM_STACK_ALIGNMENT (2 * sizeof(void *))
#define IRQ_ISR_STACK_SIZE  0x200000

	struct roc_bphy_irq_stack *curr_stack;
	void *retval = NULL;

	if (pthread_mutex_lock(&stacks_mutex))
		return NULL;

	STAILQ_FOREACH(curr_stack, &irq_stacks, entries) {
		if (curr_stack->cpu == cpu) {
			curr_stack->inuse++;
			retval = ((char *)curr_stack->sp_buffer) +
				 IRQ_ISR_STACK_SIZE;
			goto found_stack;
		}
	}

	curr_stack = plt_zmalloc(sizeof(struct roc_bphy_irq_stack), 0);
	if (curr_stack == NULL)
		goto err_stack;

	curr_stack->sp_buffer =
		plt_zmalloc(IRQ_ISR_STACK_SIZE * 2, ARM_STACK_ALIGNMENT);
	if (curr_stack->sp_buffer == NULL)
		goto err_buffer;

	curr_stack->cpu = cpu;
	curr_stack->inuse = 0;
	STAILQ_INSERT_TAIL(&irq_stacks, curr_stack, entries);
	retval = ((char *)curr_stack->sp_buffer) + IRQ_ISR_STACK_SIZE;

found_stack:
	pthread_mutex_unlock(&stacks_mutex);
	return retval;

err_buffer:
	plt_free(curr_stack);

err_stack:
	pthread_mutex_unlock(&stacks_mutex);
	return NULL;
}

bool
roc_bphy_intr_available(struct roc_bphy_irq_chip *irq_chip, int irq_num)
{
	if (irq_num < 0 || (uint64_t)irq_num >= irq_chip->max_irq)
		return false;

	return irq_chip->avail_irq_bmask & BIT(irq_num);
}
