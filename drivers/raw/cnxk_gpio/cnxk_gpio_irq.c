/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <fcntl.h>
#include <pthread.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <unistd.h>

#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_gpio.h"

#define OTX_IOC_MAGIC 0xF2
#define OTX_IOC_SET_GPIO_HANDLER                                               \
	_IOW(OTX_IOC_MAGIC, 1, struct otx_gpio_usr_data)
#define OTX_IOC_CLR_GPIO_HANDLER                                               \
	_IO(OTX_IOC_MAGIC, 2)

struct otx_gpio_usr_data {
	uint64_t isr_base;
	uint64_t sp;
	uint64_t cpu;
	uint64_t gpio_num;
};

struct cnxk_gpio_irq_stack {
	LIST_ENTRY(cnxk_gpio_irq_stack) next;
	void *sp_buffer;
	int cpu;
	int inuse;
};

struct cnxk_gpio_irqchip {
	int fd;
	/* serialize access to this struct */
	pthread_mutex_t lock;
	LIST_HEAD(, cnxk_gpio_irq_stack) stacks;

	struct cnxk_gpiochip *gpiochip;
};

static struct cnxk_gpio_irqchip *irqchip;

static void
cnxk_gpio_irq_stack_free(int cpu)
{
	struct cnxk_gpio_irq_stack *stack;

	LIST_FOREACH(stack, &irqchip->stacks, next) {
		if (stack->cpu == cpu)
			break;
	}

	if (!stack)
		return;

	if (stack->inuse)
		stack->inuse--;

	if (stack->inuse == 0) {
		LIST_REMOVE(stack, next);
		rte_free(stack->sp_buffer);
		rte_free(stack);
	}
}

static void *
cnxk_gpio_irq_stack_alloc(int cpu)
{
#define ARM_STACK_ALIGNMENT (2 * sizeof(void *))
#define IRQ_STACK_SIZE 0x200000

	struct cnxk_gpio_irq_stack *stack;

	LIST_FOREACH(stack, &irqchip->stacks, next) {
		if (stack->cpu == cpu)
			break;
	}

	if (stack) {
		stack->inuse++;
		return (char *)stack->sp_buffer + IRQ_STACK_SIZE;
	}

	stack = rte_malloc(NULL, sizeof(*stack), 0);
	if (!stack)
		return NULL;

	stack->sp_buffer =
		rte_zmalloc(NULL, IRQ_STACK_SIZE * 2, ARM_STACK_ALIGNMENT);
	if (!stack->sp_buffer) {
		rte_free(stack);
		return NULL;
	}

	stack->cpu = cpu;
	stack->inuse = 1;
	LIST_INSERT_HEAD(&irqchip->stacks, stack, next);

	return (char *)stack->sp_buffer + IRQ_STACK_SIZE;
}

static void
cnxk_gpio_irq_handler(int gpio_num)
{
	struct cnxk_gpiochip *gpiochip = irqchip->gpiochip;
	struct cnxk_gpio *gpio;

	if (gpio_num >= gpiochip->num_gpios)
		goto out;

	gpio = gpiochip->gpios[gpio_num];
	if (likely(gpio->handler))
		gpio->handler(gpio_num, gpio->data);

out:
	roc_atf_ret();
}

int
cnxk_gpio_irq_init(struct cnxk_gpiochip *gpiochip)
{
	if (irqchip)
		return 0;

	irqchip = rte_zmalloc(NULL, sizeof(*irqchip), 0);
	if (!irqchip)
		return -ENOMEM;

	irqchip->fd = open("/dev/otx-gpio-ctr", O_RDWR | O_SYNC);
	if (irqchip->fd < 0) {
		rte_free(irqchip);
		return -errno;
	}

	pthread_mutex_init(&irqchip->lock, NULL);
	LIST_INIT(&irqchip->stacks);
	irqchip->gpiochip = gpiochip;

	return 0;
}

void
cnxk_gpio_irq_fini(void)
{
	if (!irqchip)
		return;

	close(irqchip->fd);
	rte_free(irqchip);
	irqchip = NULL;
}

int
cnxk_gpio_irq_request(int gpio, int cpu)
{
	struct otx_gpio_usr_data data;
	void *sp;
	int ret;

	pthread_mutex_lock(&irqchip->lock);

	sp = cnxk_gpio_irq_stack_alloc(cpu);
	if (!sp) {
		ret = -ENOMEM;
		goto out_unlock;
	}

	data.isr_base = (uint64_t)cnxk_gpio_irq_handler;
	data.sp = (uint64_t)sp;
	data.cpu = (uint64_t)cpu;
	data.gpio_num = (uint64_t)gpio;

	mlockall(MCL_CURRENT | MCL_FUTURE);
	ret = ioctl(irqchip->fd, OTX_IOC_SET_GPIO_HANDLER, &data);
	if (ret) {
		ret = -errno;
		goto out_free_stack;
	}

	pthread_mutex_unlock(&irqchip->lock);

	return 0;

out_free_stack:
	cnxk_gpio_irq_stack_free(cpu);
out_unlock:
	pthread_mutex_unlock(&irqchip->lock);

	return ret;
}

int
cnxk_gpio_irq_free(int gpio)
{
	int ret;

	pthread_mutex_lock(&irqchip->lock);

	ret = ioctl(irqchip->fd, OTX_IOC_CLR_GPIO_HANDLER, gpio);
	if (ret) {
		pthread_mutex_unlock(&irqchip->lock);
		return -errno;
	}

	cnxk_gpio_irq_stack_free(irqchip->gpiochip->gpios[gpio]->cpu);

	pthread_mutex_unlock(&irqchip->lock);

	return 0;
}
