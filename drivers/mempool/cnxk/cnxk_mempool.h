/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef _CNXK_MEMPOOL_H_
#define _CNXK_MEMPOOL_H_

#include <rte_mempool.h>

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
