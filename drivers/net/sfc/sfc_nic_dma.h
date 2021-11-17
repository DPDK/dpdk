/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Xilinx, Inc.
 */

#ifndef _SFC_NIC_DMA_H
#define _SFC_NIC_DMA_H

#include <rte_memzone.h>

#include "efx.h"

#include "sfc.h"

#ifdef __cplusplus
extern "C" {
#endif

int sfc_nic_dma_attach(struct sfc_adapter *sa);
void sfc_nic_dma_detach(struct sfc_adapter *sa);

int sfc_nic_dma_mz_map(struct sfc_adapter *sa, const struct rte_memzone *mz,
		       efx_nic_dma_addr_type_t addr_type,
		       efsys_dma_addr_t *dma_addr);

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_NIC_DMA_H */
