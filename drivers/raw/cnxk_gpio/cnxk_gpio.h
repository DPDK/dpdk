/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_GPIO_H_
#define _CNXK_GPIO_H_

struct cnxk_gpiochip;

struct cnxk_gpio {
	struct cnxk_gpiochip *gpiochip;
	int num;
};

struct cnxk_gpiochip {
	int num;
	int base;
	int num_gpios;
	struct cnxk_gpio **gpios;
};

#endif /* _CNXK_GPIO_H_ */
