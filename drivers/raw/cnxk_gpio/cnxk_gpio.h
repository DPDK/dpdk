/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_GPIO_H_
#define _CNXK_GPIO_H_

struct cnxk_gpiochip;

struct cnxk_gpio {
	struct cnxk_gpiochip *gpiochip;
	void *rsp;
	int num;
	void (*handler)(int gpio, void *data);
	void *data;
	int cpu;
};

struct cnxk_gpiochip {
	int num;
	int base;
	int num_gpios;
	int num_queues;
	struct cnxk_gpio **gpios;
	int *allowlist;
};

int cnxk_gpio_selftest(uint16_t dev_id);

int cnxk_gpio_irq_init(struct cnxk_gpiochip *gpiochip);
void cnxk_gpio_irq_fini(void);
int cnxk_gpio_irq_request(int gpio, int cpu);
int cnxk_gpio_irq_free(int gpio);

#endif /* _CNXK_GPIO_H_ */
