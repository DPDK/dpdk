/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _NTNIC_VFIO_H_
#define _NTNIC_VFIO_H_

#include <rte_dev.h>
#include <rte_bus_pci.h>
#include <ethdev_pci.h>

void
nt_vfio_init(void);

int
nt_vfio_setup(struct rte_pci_device *dev);
int
nt_vfio_remove(int vf_num);

int
nt_vfio_dma_map(int vf_num, void *virt_addr, uint64_t *iova_addr, uint64_t size);
int
nt_vfio_dma_unmap(int vf_num, void *virt_addr, uint64_t iova_addr, uint64_t size);

/* Find device (PF/VF) number from device address */
#endif	/* _NTNIC_VFIO_H_ */
