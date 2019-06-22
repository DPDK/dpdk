/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_MEMPOOL_H__
#define __OTX2_MEMPOOL_H__

#include <rte_bitmap.h>
#include <rte_bus_pci.h>
#include <rte_devargs.h>
#include <rte_mempool.h>

#include "otx2_common.h"
#include "otx2_mbox.h"

enum npa_lf_status {
	NPA_LF_ERR_PARAM	    = -512,
	NPA_LF_ERR_ALLOC	    = -513,
	NPA_LF_ERR_INVALID_BLOCK_SZ = -514,
	NPA_LF_ERR_AURA_ID_ALLOC    = -515,
	NPA_LF_ERR_AURA_POOL_INIT   = -516,
	NPA_LF_ERR_AURA_POOL_FINI   = -517,
	NPA_LF_ERR_BASE_INVALID     = -518,
};

struct otx2_npa_lf;
struct otx2_npa_qint {
	struct otx2_npa_lf *lf;
	uint8_t qintx;
};

struct otx2_npa_lf {
	uint16_t qints;
	uintptr_t base;
	uint8_t aura_sz;
	uint16_t pf_func;
	uint32_t nr_pools;
	void *npa_bmp_mem;
	void *npa_qint_mem;
	uint16_t npa_msixoff;
	struct otx2_mbox *mbox;
	uint32_t stack_pg_ptrs;
	uint32_t stack_pg_bytes;
	struct rte_bitmap *npa_bmp;
	struct rte_pci_device *pci_dev;
	struct rte_intr_handle *intr_handle;
};

#define AURA_ID_MASK  (BIT_ULL(16) - 1)

/* NPA LF */
int otx2_npa_lf_init(struct rte_pci_device *pci_dev, void *otx2_dev);
int otx2_npa_lf_fini(void);

#endif /* __OTX2_MEMPOOL_H__ */
