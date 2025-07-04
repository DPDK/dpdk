/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_MEMPOOL_H_
#define _CNXK_MEMPOOL_H_

#include <rte_mempool.h>

enum cnxk_mempool_flags {
	/* This flag is used to ensure that only aura zero is allocated.
	 * If aura zero is not available, then mempool creation fails.
	 */
	CNXK_MEMPOOL_F_ZERO_AURA = RTE_BIT64(0),
	/* Here the pool create will use the npa_aura_s structure passed
	 * as pool config to create the pool.
	 */
	CNXK_MEMPOOL_F_CUSTOM_AURA = RTE_BIT64(1),
	/* This flag indicates whether the pool is a hardware pool or not.
	 * This flag is set by the driver.
	 */
	CNXK_MEMPOOL_F_IS_HWPOOL = RTE_BIT64(2),
	/* This flag indicates whether range check has been disabled for
	 * the pool. This flag is set by the driver.
	 */
	CNXK_MEMPOOL_F_NO_RANGE_CHECK = RTE_BIT64(3),
};

#define CNXK_MEMPOOL_F_MASK 0xFUL

#define CNXK_MEMPOOL_FLAGS(_m)                                                 \
	(PLT_U64_CAST((_m)->pool_config) & CNXK_MEMPOOL_F_MASK)
#define CNXK_MEMPOOL_CONFIG(_m)                                                \
	(PLT_PTR_CAST(PLT_U64_CAST((_m)->pool_config) & ~CNXK_MEMPOOL_F_MASK))
#define CNXK_MEMPOOL_SET_FLAGS(_m, _f)                                         \
	do {                                                                   \
		void *_c = CNXK_MEMPOOL_CONFIG(_m);                            \
		uint64_t _flags = CNXK_MEMPOOL_FLAGS(_m) | (_f);               \
		(_m)->pool_config = PLT_PTR_CAST(PLT_U64_CAST(_c) | _flags);   \
	} while (0)

unsigned int cnxk_mempool_get_count(const struct rte_mempool *mp);
ssize_t cnxk_mempool_calc_mem_size(const struct rte_mempool *mp,
				   uint32_t obj_num, uint32_t pg_shift,
				   size_t *min_chunk_size, size_t *align);
int cnxk_mempool_populate(struct rte_mempool *mp, unsigned int max_objs,
			  void *vaddr, rte_iova_t iova, size_t len,
			  rte_mempool_populate_obj_cb_t *obj_cb,
			  void *obj_cb_arg);
int cnxk_mempool_alloc(struct rte_mempool *mp);
void cnxk_mempool_free(struct rte_mempool *mp);

int __rte_hot cnxk_mempool_enq(struct rte_mempool *mp, void *const *obj_table,
			       unsigned int n);
int __rte_hot cnxk_mempool_deq(struct rte_mempool *mp, void **obj_table,
			       unsigned int n);

int cn10k_mempool_plt_init(void);

#endif
