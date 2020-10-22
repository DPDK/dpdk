/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#ifndef _IOAT_H_
#define _IOAT_H_

#include <rte_vhost.h>
#include <rte_pci.h>

#define MAX_VHOST_DEVICE 1024
#define IOAT_RING_SIZE 4096

struct dma_info {
	struct rte_pci_addr addr;
	uint16_t dev_id;
	bool is_valid;
};

struct dma_for_vhost {
	struct dma_info dmas[RTE_MAX_QUEUES_PER_PORT * 2];
	uint16_t nr;
};

#ifdef RTE_ARCH_X86
int open_ioat(const char *value);
#else
static int open_ioat(const char *value __rte_unused)
{
	return -1;
}
#endif
#endif /* _IOAT_H_ */
