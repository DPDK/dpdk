/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>

#include "roc_api.h"
#include "roc_bphy_irq.h"

#define roc_cpuset_t cpu_set_t

struct roc_bphy_irq_usr_data {
	uint64_t isr_base;
	uint64_t sp;
	uint64_t cpu;
	uint64_t irq_num;
};

struct roc_bphy_irq_stack {
	STAILQ_ENTRY(roc_bphy_irq_stack) entries;
	void *sp_buffer;
	int cpu;
	int inuse;
};

#define ROC_BPHY_MEMZONE_NAME "roc_bphy_mz"
#define ROC_BPHY_CTR_DEV_PATH "/dev/otx-bphy-ctr"

#define ROC_BPHY_IOC_MAGIC 0xF3
#define ROC_BPHY_IOC_SET_BPHY_HANDLER                                          \
	_IOW(ROC_BPHY_IOC_MAGIC, 1, struct roc_bphy_irq_usr_data)
#define ROC_BPHY_IOC_CLR_BPHY_HANDLER	_IO(ROC_BPHY_IOC_MAGIC, 2)
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

static void
roc_bphy_irq_stack_remove(int cpu)
{
	struct roc_bphy_irq_stack *curr_stack;

	if (pthread_mutex_lock(&stacks_mutex))
		return;

	STAILQ_FOREACH(curr_stack, &irq_stacks, entries) {
		if (curr_stack->cpu == cpu)
			break;
	}

	if (curr_stack == NULL)
		goto leave;

	if (curr_stack->inuse > 0)
		curr_stack->inuse--;

	if (curr_stack->inuse == 0) {
		STAILQ_REMOVE(&irq_stacks, curr_stack, roc_bphy_irq_stack,
			      entries);
		plt_free(curr_stack->sp_buffer);
		plt_free(curr_stack);
	}

leave:
	pthread_mutex_unlock(&stacks_mutex);
}

static void *
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

void
roc_bphy_intr_handler(unsigned int irq_num)
{
	struct roc_bphy_irq_chip *irq_chip;
	const struct plt_memzone *mz;

	mz = plt_memzone_lookup(ROC_BPHY_MEMZONE_NAME);
	if (mz == NULL)
		return;

	irq_chip = *(struct roc_bphy_irq_chip **)mz->addr;
	if (irq_chip == NULL)
		return;

	if (irq_chip->irq_vecs[irq_num].handler != NULL)
		irq_chip->irq_vecs[irq_num].handler(
			(int)irq_num, irq_chip->irq_vecs[irq_num].isr_data);

	roc_atf_ret();
}

static int
roc_bphy_irq_handler_set(struct roc_bphy_irq_chip *chip, int irq_num,
			 void (*isr)(int irq_num, void *isr_data),
			 void *isr_data)
{
	roc_cpuset_t orig_cpuset, intr_cpuset;
	struct roc_bphy_irq_usr_data irq_usr;
	const struct plt_memzone *mz;
	int i, retval, curr_cpu, rc;
	char *env;

	mz = plt_memzone_lookup(chip->mz_name);
	if (mz == NULL) {
		/* what we want is just a pointer to chip, not object itself */
		mz = plt_memzone_reserve_cache_align(chip->mz_name,
						     sizeof(chip));
		if (mz == NULL)
			return -ENOMEM;
	}

	if (chip->irq_vecs[irq_num].handler != NULL)
		return -EINVAL;

	rc = pthread_getaffinity_np(pthread_self(), sizeof(orig_cpuset),
				    &orig_cpuset);
	if (rc < 0) {
		plt_err("Failed to get affinity mask");
		return rc;
	}

	for (curr_cpu = -1, i = 0; i < CPU_SETSIZE; i++)
		if (CPU_ISSET(i, &orig_cpuset))
			curr_cpu = i;
	if (curr_cpu < 0)
		return -ENOENT;

	CPU_ZERO(&intr_cpuset);
	CPU_SET(curr_cpu, &intr_cpuset);
	rc = pthread_setaffinity_np(pthread_self(), sizeof(intr_cpuset),
					&intr_cpuset);
	if (rc < 0) {
		plt_err("Failed to set affinity mask");
		return rc;
	}

	irq_usr.isr_base = (uint64_t)roc_bphy_intr_handler;
	irq_usr.sp = (uint64_t)roc_bphy_irq_stack_get(curr_cpu);
	irq_usr.cpu = curr_cpu;
	if (irq_usr.sp == 0) {
		rc = pthread_setaffinity_np(pthread_self(), sizeof(orig_cpuset),
					    &orig_cpuset);
		if (rc < 0)
			plt_err("Failed to restore affinity mask");
		return rc;
	}

	/* On simulator memory locking operation takes much time. We want
	 * to skip this when running in such an environment.
	 */
	env = getenv("BPHY_INTR_MLOCK_DISABLE");
	if (env == NULL) {
		rc = mlockall(MCL_CURRENT | MCL_FUTURE);
		if (rc < 0)
			plt_warn("Failed to lock memory into RAM");
	}

	*((struct roc_bphy_irq_chip **)(mz->addr)) = chip;
	irq_usr.irq_num = irq_num;
	chip->irq_vecs[irq_num].handler_cpu = curr_cpu;
	chip->irq_vecs[irq_num].handler = isr;
	chip->irq_vecs[irq_num].isr_data = isr_data;
	retval = ioctl(chip->intfd, ROC_BPHY_IOC_SET_BPHY_HANDLER, &irq_usr);
	if (retval != 0) {
		roc_bphy_irq_stack_remove(curr_cpu);
		chip->irq_vecs[irq_num].handler = NULL;
		chip->irq_vecs[irq_num].handler_cpu = -1;
	} else {
		chip->n_handlers++;
	}

	rc = pthread_setaffinity_np(pthread_self(), sizeof(orig_cpuset),
				    &orig_cpuset);
	if (rc < 0)
		plt_warn("Failed to restore affinity mask");

	return retval;
}

bool
roc_bphy_intr_available(struct roc_bphy_irq_chip *irq_chip, int irq_num)
{
	if (irq_num < 0 || (uint64_t)irq_num >= irq_chip->max_irq)
		return false;

	return irq_chip->avail_irq_bmask & BIT(irq_num);
}

uint64_t
roc_bphy_intr_max_get(struct roc_bphy_irq_chip *irq_chip)
{
	return irq_chip->max_irq;
}

int
roc_bphy_intr_clear(struct roc_bphy_irq_chip *chip, int irq_num)
{
	roc_cpuset_t orig_cpuset, intr_cpuset;
	const struct plt_memzone *mz;
	int retval;

	if (chip == NULL)
		return -EINVAL;
	if ((uint64_t)irq_num >= chip->max_irq || irq_num < 0)
		return -EINVAL;
	if (!roc_bphy_intr_available(chip, irq_num))
		return -ENOTSUP;
	if (chip->irq_vecs[irq_num].handler == NULL)
		return -EINVAL;
	mz = plt_memzone_lookup(chip->mz_name);
	if (mz == NULL)
		return -ENXIO;

	retval = pthread_getaffinity_np(pthread_self(), sizeof(orig_cpuset),
					&orig_cpuset);
	if (retval < 0) {
		plt_warn("Failed to get affinity mask");
		CPU_ZERO(&orig_cpuset);
		CPU_SET(0, &orig_cpuset);
	}

	CPU_ZERO(&intr_cpuset);
	CPU_SET(chip->irq_vecs[irq_num].handler_cpu, &intr_cpuset);
	retval = pthread_setaffinity_np(pthread_self(), sizeof(intr_cpuset),
					&intr_cpuset);
	if (retval < 0) {
		plt_warn("Failed to set affinity mask");
		CPU_ZERO(&orig_cpuset);
		CPU_SET(0, &orig_cpuset);
	}

	retval = ioctl(chip->intfd, ROC_BPHY_IOC_CLR_BPHY_HANDLER, irq_num);
	if (retval == 0) {
		roc_bphy_irq_stack_remove(chip->irq_vecs[irq_num].handler_cpu);
		chip->n_handlers--;
		chip->irq_vecs[irq_num].isr_data = NULL;
		chip->irq_vecs[irq_num].handler = NULL;
		chip->irq_vecs[irq_num].handler_cpu = -1;
		if (chip->n_handlers == 0) {
			retval = plt_memzone_free(mz);
			if (retval < 0)
				plt_err("Failed to free memzone: irq %d",
					irq_num);
		}
	} else {
		plt_err("Failed to clear bphy interrupt handler");
	}

	retval = pthread_setaffinity_np(pthread_self(), sizeof(orig_cpuset),
					&orig_cpuset);
	if (retval < 0) {
		plt_warn("Failed to restore affinity mask");
		CPU_ZERO(&orig_cpuset);
		CPU_SET(0, &orig_cpuset);
	}

	return retval;
}

int
roc_bphy_intr_register(struct roc_bphy_irq_chip *irq_chip,
		       struct roc_bphy_intr *intr)
{
	roc_cpuset_t orig_cpuset, intr_cpuset;
	int retval;
	int ret;

	if (!roc_bphy_intr_available(irq_chip, intr->irq_num))
		return -ENOTSUP;

	retval = pthread_getaffinity_np(pthread_self(), sizeof(orig_cpuset),
					&orig_cpuset);
	if (retval < 0) {
		plt_err("Failed to get affinity mask");
		return retval;
	}

	CPU_ZERO(&intr_cpuset);
	CPU_SET(intr->cpu, &intr_cpuset);
	retval = pthread_setaffinity_np(pthread_self(), sizeof(intr_cpuset),
					&intr_cpuset);
	if (retval < 0) {
		plt_err("Failed to set affinity mask");
		return retval;
	}

	ret = roc_bphy_irq_handler_set(irq_chip, intr->irq_num,
				       intr->intr_handler, intr->isr_data);

	retval = pthread_setaffinity_np(pthread_self(), sizeof(orig_cpuset),
					&orig_cpuset);
	if (retval < 0)
		plt_warn("Failed to restore affinity mask");

	return ret;
}
