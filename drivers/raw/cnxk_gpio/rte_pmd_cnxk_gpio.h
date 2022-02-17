/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _RTE_PMD_CNXK_GPIO_H_
#define _RTE_PMD_CNXK_GPIO_H_

/**
 * @file rte_pmd_cnxk_gpio.h
 *
 * Marvell GPIO PMD specific structures and interface
 *
 * This API allows applications to manage GPIOs in user space along with
 * installing interrupt handlers for low latency signal processing.
 */

#ifdef __cplusplus
extern "C" {
#endif

/** Available message types */
enum cnxk_gpio_msg_type {
	/** Invalid message type */
	CNXK_GPIO_MSG_TYPE_INVALID,
};

struct cnxk_gpio_msg {
	/** Message type */
	enum cnxk_gpio_msg_type type;
	/** Message data passed to PMD or received from PMD */
	void *data;
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PMD_CNXK_GPIO_H_ */
