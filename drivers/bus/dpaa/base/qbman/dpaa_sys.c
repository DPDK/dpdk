/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2013-2016 Freescale Semiconductor Inc.
 * Copyright 2017 NXP
 *
 */

#include <process.h>
#include "dpaa_sys.h"

struct process_interrupt {
	int irq;
	irqreturn_t (*isr)(int irq, void *arg);
	unsigned long flags;
	const char *name;
	void *arg;
	struct list_head node;
};

static COMPAT_LIST_HEAD(process_irq_list);
static pthread_mutex_t process_irq_lock = PTHREAD_MUTEX_INITIALIZER;

static void process_interrupt_install(struct process_interrupt *irq)
{
	int ret;
	/* Add the irq to the end of the list */
	ret = pthread_mutex_lock(&process_irq_lock);
	assert(!ret);
	list_add_tail(&irq->node, &process_irq_list);
	ret = pthread_mutex_unlock(&process_irq_lock);
	assert(!ret);
}

static void process_interrupt_remove(struct process_interrupt *irq)
{
	int ret;

	ret = pthread_mutex_lock(&process_irq_lock);
	assert(!ret);
	list_del(&irq->node);
	ret = pthread_mutex_unlock(&process_irq_lock);
	assert(!ret);
}

static struct process_interrupt *process_interrupt_find(int irq_num)
{
	int ret;
	struct process_interrupt *i = NULL;

	ret = pthread_mutex_lock(&process_irq_lock);
	assert(!ret);
	list_for_each_entry(i, &process_irq_list, node) {
		if (i->irq == irq_num)
			goto done;
	}
done:
	ret = pthread_mutex_unlock(&process_irq_lock);
	assert(!ret);
	return i;
}

/* This is the interface from the platform-agnostic driver code to (de)register
 * interrupt handlers. We simply create/destroy corresponding structs.
 */
int qbman_request_irq(int irq, irqreturn_t (*isr)(int irq, void *arg),
		      unsigned long flags, const char *name,
		      void *arg __maybe_unused)
{
	struct process_interrupt *irq_node =
		kmalloc(sizeof(*irq_node), GFP_KERNEL);

	if (!irq_node)
		return -ENOMEM;
	irq_node->irq = irq;
	irq_node->isr = isr;
	irq_node->flags = flags;
	irq_node->name = name;
	irq_node->arg = arg;
	process_interrupt_install(irq_node);
	return 0;
}

int qbman_free_irq(int irq, __maybe_unused void *arg)
{
	struct process_interrupt *irq_node = process_interrupt_find(irq);

	if (!irq_node)
		return -EINVAL;
	process_interrupt_remove(irq_node);
	kfree(irq_node);
	return 0;
}

/* This is the interface from the platform-specific driver code to obtain
 * interrupt handlers that have been registered.
 */
void qbman_invoke_irq(int irq)
{
	struct process_interrupt *irq_node = process_interrupt_find(irq);

	if (irq_node)
		irq_node->isr(irq, irq_node->arg);
}
