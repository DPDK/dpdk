/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _NITROX_DEVICE_H_
#define _NITROX_DEVICE_H_

#include <bus_pci_driver.h>
#include <rte_compat.h>

struct nitrox_sym_device;
struct nitrox_comp_device;

struct nitrox_device {
	TAILQ_ENTRY(nitrox_device) next;
	struct rte_pci_device *pdev;
	uint8_t *bar_addr;
	struct nitrox_sym_device *sym_dev;
	struct nitrox_comp_device *comp_dev;
	struct rte_device rte_sym_dev;
	struct rte_device rte_comp_dev;
	uint16_t nr_queues;
};

struct nitrox_driver {
	TAILQ_ENTRY(nitrox_driver) next;
	int (*create)(struct nitrox_device *ndev);
	int (*destroy)(struct nitrox_device *ndev);
};

__rte_internal
void nitrox_register_driver(struct nitrox_driver *ndrv);

#define NITROX_REGISTER_DRIVER(ndrv) \
RTE_INIT(ndrv ## _register) \
{ \
	nitrox_register_driver(&ndrv); \
}

#endif /* _NITROX_DEVICE_H_ */
