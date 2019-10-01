/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_DEVICE_H_
#define _NITROX_DEVICE_H_

#include <rte_bus_pci.h>
#include <rte_cryptodev.h>

struct nitrox_device {
	TAILQ_ENTRY(nitrox_device) next;
	struct rte_pci_device *pdev;
	uint8_t *bar_addr;
	uint16_t nr_queues;
};

#endif /* _NITROX_DEVICE_H_ */
