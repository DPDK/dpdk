/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTOS_DRV_H__
#define __NTOS_DRV_H__

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_ether.h>

#define NUM_MAC_ADDRS_PER_PORT (16U)
#define NUM_MULTICAST_ADDRS_PER_PORT (16U)

#define NUM_ADAPTER_MAX (8)
#define NUM_ADAPTER_PORTS_MAX (128)

struct pmd_internals {
	const struct rte_pci_device *pci_dev;
	char name[20];
	int n_intf_no;
	struct drv_s *p_drv;
	struct pmd_internals *next;
};

#endif	/* __NTOS_DRV_H__ */
