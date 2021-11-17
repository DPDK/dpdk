/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Xilinx, Inc.
 */

#ifndef _SFC_NIC_DMA_DP_H
#define _SFC_NIC_DMA_DP_H

#include <rte_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define SFC_NIC_DMA_REGIONS_MAX 2

struct sfc_nic_dma_region {
	rte_iova_t	nic_base;
	rte_iova_t	trgt_base;
	rte_iova_t	trgt_end;
};

/** Driver cache for NIC DMA regions */
struct sfc_nic_dma_info {
	struct sfc_nic_dma_region		regions[SFC_NIC_DMA_REGIONS_MAX];
	unsigned int				nb_regions;
};

static inline rte_iova_t
sfc_nic_dma_map(const struct sfc_nic_dma_info *nic_dma_info,
		rte_iova_t trgt_addr, size_t len)
{
	unsigned int i;

	for (i = 0; i < nic_dma_info->nb_regions; i++) {
		const struct sfc_nic_dma_region *region;

		region = &nic_dma_info->regions[i];
		/*
		 * Do not sum trgt_addr and len to avoid overflow
		 * checking.
		 */
		if (region->trgt_base <= trgt_addr &&
		    trgt_addr <= region->trgt_end &&
		    len <= region->trgt_end - trgt_addr) {
			return region->nic_base +
					(trgt_addr - region->trgt_base);
		}
	}

	return RTE_BAD_IOVA;
}

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_NIC_DMA_DP_H */
