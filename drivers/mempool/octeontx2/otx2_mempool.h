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

struct npa_aura_lim {
	uint64_t ptr_start;
	uint64_t ptr_end;
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
	struct npa_aura_lim *aura_lim;
	struct rte_pci_device *pci_dev;
	struct rte_intr_handle *intr_handle;
};

#define AURA_ID_MASK  (BIT_ULL(16) - 1)

/*
 * Generate 64bit handle to have optimized alloc and free aura operation.
 * 0 - AURA_ID_MASK for storing the aura_id.
 * AURA_ID_MASK+1 - (2^64 - 1) for storing the lf base address.
 * This scheme is valid when OS can give AURA_ID_MASK
 * aligned address for lf base address.
 */
static inline uint64_t
npa_lf_aura_handle_gen(uint32_t aura_id, uintptr_t addr)
{
	uint64_t val;

	val = aura_id & AURA_ID_MASK;
	return (uint64_t)addr | val;
}

static inline uint64_t
npa_lf_aura_handle_to_aura(uint64_t aura_handle)
{
	return aura_handle & AURA_ID_MASK;
}

static inline uintptr_t
npa_lf_aura_handle_to_base(uint64_t aura_handle)
{
	return (uintptr_t)(aura_handle & ~AURA_ID_MASK);
}

static inline uint64_t
npa_lf_aura_op_alloc(uint64_t aura_handle, const int drop)
{
	uint64_t wdata = npa_lf_aura_handle_to_aura(aura_handle);

	if (drop)
		wdata |= BIT_ULL(63); /* DROP */

	return otx2_atomic64_add_nosync(wdata,
		(int64_t *)(npa_lf_aura_handle_to_base(aura_handle) +
		NPA_LF_AURA_OP_ALLOCX(0)));
}

static inline void
npa_lf_aura_op_free(uint64_t aura_handle, const int fabs, uint64_t iova)
{
	uint64_t reg = npa_lf_aura_handle_to_aura(aura_handle);

	if (fabs)
		reg |= BIT_ULL(63); /* FABS */

	otx2_store_pair(iova, reg,
		npa_lf_aura_handle_to_base(aura_handle) + NPA_LF_AURA_OP_FREE0);
}

static inline uint64_t
npa_lf_aura_op_cnt_get(uint64_t aura_handle)
{
	uint64_t wdata;
	uint64_t reg;

	wdata = npa_lf_aura_handle_to_aura(aura_handle) << 44;

	reg = otx2_atomic64_add_nosync(wdata,
			(int64_t *)(npa_lf_aura_handle_to_base(aura_handle) +
			 NPA_LF_AURA_OP_CNT));

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFF;
}

static inline void
npa_lf_aura_op_cnt_set(uint64_t aura_handle, const int sign, uint64_t count)
{
	uint64_t reg = count & (BIT_ULL(36) - 1);

	if (sign)
		reg |= BIT_ULL(43); /* CNT_ADD */

	reg |= (npa_lf_aura_handle_to_aura(aura_handle) << 44);

	otx2_write64(reg,
		npa_lf_aura_handle_to_base(aura_handle) + NPA_LF_AURA_OP_CNT);
}

static inline uint64_t
npa_lf_aura_op_limit_get(uint64_t aura_handle)
{
	uint64_t wdata;
	uint64_t reg;

	wdata = npa_lf_aura_handle_to_aura(aura_handle) << 44;

	reg = otx2_atomic64_add_nosync(wdata,
			(int64_t *)(npa_lf_aura_handle_to_base(aura_handle) +
			 NPA_LF_AURA_OP_LIMIT));

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFF;
}

static inline void
npa_lf_aura_op_limit_set(uint64_t aura_handle, uint64_t limit)
{
	uint64_t reg = limit & (BIT_ULL(36) - 1);

	reg |= (npa_lf_aura_handle_to_aura(aura_handle) << 44);

	otx2_write64(reg,
		npa_lf_aura_handle_to_base(aura_handle) + NPA_LF_AURA_OP_LIMIT);
}

static inline uint64_t
npa_lf_aura_op_available(uint64_t aura_handle)
{
	uint64_t wdata;
	uint64_t reg;

	wdata = npa_lf_aura_handle_to_aura(aura_handle) << 44;

	reg = otx2_atomic64_add_nosync(wdata,
			    (int64_t *)(npa_lf_aura_handle_to_base(
			     aura_handle) + NPA_LF_POOL_OP_AVAILABLE));

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFF;
}

static inline void
npa_lf_aura_op_range_set(uint64_t aura_handle, uint64_t start_iova,
				uint64_t end_iova)
{
	uint64_t reg = npa_lf_aura_handle_to_aura(aura_handle);
	struct otx2_npa_lf *lf = otx2_npa_lf_obj_get();
	struct npa_aura_lim *lim = lf->aura_lim;

	lim[reg].ptr_start = RTE_MIN(lim[reg].ptr_start, start_iova);
	lim[reg].ptr_end = RTE_MAX(lim[reg].ptr_end, end_iova);

	otx2_store_pair(lim[reg].ptr_start, reg,
			npa_lf_aura_handle_to_base(aura_handle) +
			NPA_LF_POOL_OP_PTR_START0);
	otx2_store_pair(lim[reg].ptr_end, reg,
			npa_lf_aura_handle_to_base(aura_handle) +
			NPA_LF_POOL_OP_PTR_END0);
}

/* NPA LF */
__rte_internal
int otx2_npa_lf_init(struct rte_pci_device *pci_dev, void *otx2_dev);
__rte_internal
int otx2_npa_lf_fini(void);

/* IRQ */
int otx2_npa_register_irqs(struct otx2_npa_lf *lf);
void otx2_npa_unregister_irqs(struct otx2_npa_lf *lf);

/* Debug */
int otx2_mempool_ctx_dump(struct otx2_npa_lf *lf);

#endif /* __OTX2_MEMPOOL_H__ */
