/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#ifndef _SFC_VDPA_H
#define _SFC_VDPA_H

#include <stdint.h>
#include <sys/queue.h>

#include <rte_bus_pci.h>

#include "sfc_vdpa_log.h"

/* Adapter private data */
struct sfc_vdpa_adapter {
	TAILQ_ENTRY(sfc_vdpa_adapter)	next;
	struct rte_pci_device		*pdev;

	char				log_prefix[SFC_VDPA_LOG_PREFIX_MAX];
	uint32_t			logtype_main;

	int				vfio_group_fd;
	int				vfio_dev_fd;
	int				vfio_container_fd;
	int				iommu_group_num;
};

uint32_t
sfc_vdpa_register_logtype(const struct rte_pci_addr *pci_addr,
			  const char *lt_prefix_str,
			  uint32_t ll_default);

struct sfc_vdpa_adapter *
sfc_vdpa_get_adapter_by_dev(struct rte_pci_device *pdev);

#endif  /* _SFC_VDPA_H */

