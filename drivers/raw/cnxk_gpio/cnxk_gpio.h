/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_GPIO_H_
#define _CNXK_GPIO_H_

extern int cnxk_logtype_gpio;
#define RTE_LOGTYPE_CNXK_GPIO cnxk_logtype_gpio

#define CNXK_GPIO_LOG(level, ...) \
	RTE_LOG_LINE(level, CNXK_GPIO, __VA_ARGS__)

struct gpio_v2_line_event;
struct cnxk_gpiochip;

struct cnxk_gpio {
	struct cnxk_gpiochip *gpiochip;
	struct {
		struct rte_intr_handle *intr_handle;
		void (*handler)(int gpio, void *data);
		void (*handler2)(struct gpio_v2_line_event *event, void *data);
		void *data;
	} intr;
	void *rsp;
	int num;
	int fd;
};

struct cnxk_gpiochip {
	int num;
	int num_gpios;
	int num_queues;
	struct cnxk_gpio **gpios;
	int *allowlist;
};

int cnxk_gpio_selftest(uint16_t dev_id);

#endif /* _CNXK_GPIO_H_ */
