/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _ROC_NPA_H_
#define _ROC_NPA_H_

#define ROC_AURA_ID_MASK       (BIT_ULL(16) - 1)
#define ROC_AURA_OP_LIMIT_MASK (BIT_ULL(36) - 1)

/*
 * Generate 64bit handle to have optimized alloc and free aura operation.
 * 0 - ROC_AURA_ID_MASK for storing the aura_id.
 * [ROC_AURA_ID_MASK+1, (2^64 - 1)] for storing the lf base address.
 * This scheme is valid when OS can give ROC_AURA_ID_MASK
 * aligned address for lf base address.
 */
static inline uint64_t
roc_npa_aura_handle_gen(uint32_t aura_id, uintptr_t addr)
{
	uint64_t val;

	val = aura_id & ROC_AURA_ID_MASK;
	return (uint64_t)addr | val;
}

static inline uint64_t
roc_npa_aura_handle_to_aura(uint64_t aura_handle)
{
	return aura_handle & ROC_AURA_ID_MASK;
}

static inline uintptr_t
roc_npa_aura_handle_to_base(uint64_t aura_handle)
{
	return (uintptr_t)(aura_handle & ~ROC_AURA_ID_MASK);
}

static inline uint64_t
roc_npa_aura_op_alloc(uint64_t aura_handle, const int drop)
{
	uint64_t wdata = roc_npa_aura_handle_to_aura(aura_handle);
	int64_t *addr;

	if (drop)
		wdata |= BIT_ULL(63); /* DROP */

	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_AURA_OP_ALLOCX(0));
	return roc_atomic64_add_nosync(wdata, addr);
}

static inline void
roc_npa_aura_op_free(uint64_t aura_handle, const int fabs, uint64_t iova)
{
	uint64_t reg = roc_npa_aura_handle_to_aura(aura_handle);
	const uint64_t addr =
		roc_npa_aura_handle_to_base(aura_handle) + NPA_LF_AURA_OP_FREE0;
	if (fabs)
		reg |= BIT_ULL(63); /* FABS */

	roc_store_pair(iova, reg, addr);
}

static inline uint64_t
roc_npa_aura_op_cnt_get(uint64_t aura_handle)
{
	uint64_t wdata;
	int64_t *addr;
	uint64_t reg;

	wdata = roc_npa_aura_handle_to_aura(aura_handle) << 44;
	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_AURA_OP_CNT);
	reg = roc_atomic64_add_nosync(wdata, addr);

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFF;
}

static inline void
roc_npa_aura_op_cnt_set(uint64_t aura_handle, const int sign, uint64_t count)
{
	uint64_t reg = count & (BIT_ULL(36) - 1);

	if (sign)
		reg |= BIT_ULL(43); /* CNT_ADD */

	reg |= (roc_npa_aura_handle_to_aura(aura_handle) << 44);

	plt_write64(reg, roc_npa_aura_handle_to_base(aura_handle) +
				 NPA_LF_AURA_OP_CNT);
}

static inline uint64_t
roc_npa_aura_op_limit_get(uint64_t aura_handle)
{
	uint64_t wdata;
	int64_t *addr;
	uint64_t reg;

	wdata = roc_npa_aura_handle_to_aura(aura_handle) << 44;
	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_AURA_OP_LIMIT);
	reg = roc_atomic64_add_nosync(wdata, addr);

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & ROC_AURA_OP_LIMIT_MASK;
}

static inline void
roc_npa_aura_op_limit_set(uint64_t aura_handle, uint64_t limit)
{
	uint64_t reg = limit & ROC_AURA_OP_LIMIT_MASK;

	reg |= (roc_npa_aura_handle_to_aura(aura_handle) << 44);

	plt_write64(reg, roc_npa_aura_handle_to_base(aura_handle) +
				 NPA_LF_AURA_OP_LIMIT);
}

static inline uint64_t
roc_npa_aura_op_available(uint64_t aura_handle)
{
	uint64_t wdata;
	uint64_t reg;
	int64_t *addr;

	wdata = roc_npa_aura_handle_to_aura(aura_handle) << 44;
	addr = (int64_t *)(roc_npa_aura_handle_to_base(aura_handle) +
			   NPA_LF_POOL_OP_AVAILABLE);
	reg = roc_atomic64_add_nosync(wdata, addr);

	if (reg & BIT_ULL(42) /* OP_ERR */)
		return 0;
	else
		return reg & 0xFFFFFFFFF;
}

struct roc_npa {
	struct plt_pci_device *pci_dev;

#define ROC_NPA_MEM_SZ (1 * 1024)
	uint8_t reserved[ROC_NPA_MEM_SZ] __plt_cache_aligned;
} __plt_cache_aligned;

int __roc_api roc_npa_dev_init(struct roc_npa *roc_npa);
int __roc_api roc_npa_dev_fini(struct roc_npa *roc_npa);

/* NPA pool */
int __roc_api roc_npa_pool_create(uint64_t *aura_handle, uint32_t block_size,
				  uint32_t block_count, struct npa_aura_s *aura,
				  struct npa_pool_s *pool);
int __roc_api roc_npa_aura_limit_modify(uint64_t aura_handle,
					uint16_t aura_limit);
int __roc_api roc_npa_pool_destroy(uint64_t aura_handle);
int __roc_api roc_npa_pool_range_update_check(uint64_t aura_handle);
void __roc_api roc_npa_aura_op_range_set(uint64_t aura_handle,
					 uint64_t start_iova,
					 uint64_t end_iova);

/* Debug */
int __roc_api roc_npa_ctx_dump(void);
int __roc_api roc_npa_dump(void);

#endif /* _ROC_NPA_H_ */
