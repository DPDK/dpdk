/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <rte_mempool.h>

#include "roc_api.h"
#include "cnxk_mempool.h"

#define BATCH_ALLOC_SZ              ROC_CN10K_NPA_BATCH_ALLOC_MAX_PTRS
#define BATCH_OP_DATA_TABLE_MZ_NAME "batch_op_data_table_mz"

enum batch_op_status {
	BATCH_ALLOC_OP_NOT_ISSUED = 0,
	BATCH_ALLOC_OP_ISSUED = 1,
	BATCH_ALLOC_OP_DONE
};

struct batch_op_mem {
	unsigned int sz;
	enum batch_op_status status;
	uint64_t objs[BATCH_ALLOC_SZ] __rte_aligned(ROC_ALIGN);
};

struct batch_op_data {
	uint64_t lmt_addr;
	struct batch_op_mem mem[RTE_MAX_LCORE] __rte_aligned(ROC_ALIGN);
};

static struct batch_op_data **batch_op_data_tbl;

static int
batch_op_data_table_create(void)
{
	const struct rte_memzone *mz;

	/* If table is already set, nothing to do */
	if (batch_op_data_tbl)
		return 0;

	mz = rte_memzone_lookup(BATCH_OP_DATA_TABLE_MZ_NAME);
	if (mz == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			unsigned int maxpools, sz;

			maxpools = roc_idev_npa_maxpools_get();
			sz = maxpools * sizeof(struct batch_op_data *);

			mz = rte_memzone_reserve_aligned(
				BATCH_OP_DATA_TABLE_MZ_NAME, sz, SOCKET_ID_ANY,
				0, ROC_ALIGN);
		}
		if (mz == NULL) {
			plt_err("Failed to reserve batch op data table");
			return -ENOMEM;
		}
	}
	batch_op_data_tbl = mz->addr;
	rte_wmb();
	return 0;
}

static inline struct batch_op_data *
batch_op_data_get(uint64_t pool_id)
{
	uint64_t aura = roc_npa_aura_handle_to_aura(pool_id);

	return batch_op_data_tbl[aura];
}

static inline void
batch_op_data_set(uint64_t pool_id, struct batch_op_data *op_data)
{
	uint64_t aura = roc_npa_aura_handle_to_aura(pool_id);

	batch_op_data_tbl[aura] = op_data;
}

static int
batch_op_init(struct rte_mempool *mp)
{
	struct batch_op_data *op_data;
	int i;

	op_data = batch_op_data_get(mp->pool_id);
	/* The data should not have been allocated previously */
	RTE_ASSERT(op_data == NULL);

	op_data = rte_zmalloc(NULL, sizeof(struct batch_op_data), ROC_ALIGN);
	if (op_data == NULL)
		return -ENOMEM;

	for (i = 0; i < RTE_MAX_LCORE; i++) {
		op_data->mem[i].sz = 0;
		op_data->mem[i].status = BATCH_ALLOC_OP_NOT_ISSUED;
	}

	op_data->lmt_addr = roc_idev_lmt_base_addr_get();
	batch_op_data_set(mp->pool_id, op_data);
	rte_wmb();

	return 0;
}

static void
batch_op_fini(struct rte_mempool *mp)
{
	struct batch_op_data *op_data;
	int i;

	op_data = batch_op_data_get(mp->pool_id);

	rte_wmb();
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct batch_op_mem *mem = &op_data->mem[i];

		if (mem->status == BATCH_ALLOC_OP_ISSUED) {
			mem->sz = roc_npa_aura_batch_alloc_extract(
				mem->objs, mem->objs, BATCH_ALLOC_SZ);
			mem->status = BATCH_ALLOC_OP_DONE;
		}
		if (mem->status == BATCH_ALLOC_OP_DONE) {
			roc_npa_aura_op_bulk_free(mp->pool_id, mem->objs,
						  mem->sz, 1);
			mem->status = BATCH_ALLOC_OP_NOT_ISSUED;
		}
	}

	rte_free(op_data);
	batch_op_data_set(mp->pool_id, NULL);
	rte_wmb();
}

static int __rte_hot
cn10k_mempool_enq(struct rte_mempool *mp, void *const *obj_table,
		  unsigned int n)
{
	const uint64_t *ptr = (const uint64_t *)obj_table;
	uint64_t lmt_addr = 0, lmt_id = 0;
	struct batch_op_data *op_data;

	/* Ensure mbuf init changes are written before the free pointers are
	 * enqueued to the stack.
	 */
	rte_io_wmb();

	if (n == 1) {
		roc_npa_aura_op_free(mp->pool_id, 1, ptr[0]);
		return 0;
	}

	op_data = batch_op_data_get(mp->pool_id);
	lmt_addr = op_data->lmt_addr;
	ROC_LMT_BASE_ID_GET(lmt_addr, lmt_id);
	roc_npa_aura_op_batch_free(mp->pool_id, ptr, n, 1, lmt_addr, lmt_id);

	return 0;
}

static unsigned int
cn10k_mempool_get_count(const struct rte_mempool *mp)
{
	struct batch_op_data *op_data;
	unsigned int count = 0;
	int i;

	op_data = batch_op_data_get(mp->pool_id);

	rte_wmb();
	for (i = 0; i < RTE_MAX_LCORE; i++) {
		struct batch_op_mem *mem = &op_data->mem[i];

		if (mem->status == BATCH_ALLOC_OP_ISSUED)
			count += roc_npa_aura_batch_alloc_count(mem->objs,
								BATCH_ALLOC_SZ);

		if (mem->status == BATCH_ALLOC_OP_DONE)
			count += mem->sz;
	}

	count += cnxk_mempool_get_count(mp);

	return count;
}

static int __rte_hot
cn10k_mempool_deq(struct rte_mempool *mp, void **obj_table, unsigned int n)
{
	struct batch_op_data *op_data;
	struct batch_op_mem *mem;
	unsigned int count = 0;
	int tid, rc, retry;
	bool loop = true;

	op_data = batch_op_data_get(mp->pool_id);
	tid = rte_lcore_id();
	mem = &op_data->mem[tid];

	/* Issue batch alloc */
	if (mem->status == BATCH_ALLOC_OP_NOT_ISSUED) {
		rc = roc_npa_aura_batch_alloc_issue(mp->pool_id, mem->objs,
						    BATCH_ALLOC_SZ, 0, 1);
		/* If issue fails, try falling back to default alloc */
		if (unlikely(rc))
			return cnxk_mempool_deq(mp, obj_table, n);
		mem->status = BATCH_ALLOC_OP_ISSUED;
	}

	retry = 4;
	while (loop) {
		unsigned int cur_sz;

		if (mem->status == BATCH_ALLOC_OP_ISSUED) {
			mem->sz = roc_npa_aura_batch_alloc_extract(
				mem->objs, mem->objs, BATCH_ALLOC_SZ);

			/* If partial alloc reduce the retry count */
			retry -= (mem->sz != BATCH_ALLOC_SZ);
			/* Break the loop if retry count exhausted */
			loop = !!retry;
			mem->status = BATCH_ALLOC_OP_DONE;
		}

		cur_sz = n - count;
		if (cur_sz > mem->sz)
			cur_sz = mem->sz;

		/* Dequeue the pointers */
		memcpy(&obj_table[count], &mem->objs[mem->sz - cur_sz],
		       cur_sz * sizeof(uintptr_t));
		mem->sz -= cur_sz;
		count += cur_sz;

		/* Break loop if the required pointers has been dequeued */
		loop &= (count != n);

		/* Issue next batch alloc if pointers are exhausted */
		if (mem->sz == 0) {
			rc = roc_npa_aura_batch_alloc_issue(
				mp->pool_id, mem->objs, BATCH_ALLOC_SZ, 0, 1);
			/* Break loop if issue failed and set status */
			loop &= !rc;
			mem->status = !rc;
		}
	}

	if (unlikely(count != n)) {
		/* No partial alloc allowed. Free up allocated pointers */
		cn10k_mempool_enq(mp, obj_table, count);
		return -ENOENT;
	}

	return 0;
}

static int
cn10k_mempool_alloc(struct rte_mempool *mp)
{
	uint32_t block_size;
	size_t padding;
	int rc;

	block_size = mp->elt_size + mp->header_size + mp->trailer_size;
	/* Align header size to ROC_ALIGN */
	if (mp->header_size % ROC_ALIGN != 0) {
		padding = RTE_ALIGN_CEIL(mp->header_size, ROC_ALIGN) -
			  mp->header_size;
		mp->header_size += padding;
		block_size += padding;
	}

	/* Align block size to ROC_ALIGN */
	if (block_size % ROC_ALIGN != 0) {
		padding = RTE_ALIGN_CEIL(block_size, ROC_ALIGN) - block_size;
		mp->trailer_size += padding;
		block_size += padding;
	}

	rc = cnxk_mempool_alloc(mp);
	if (rc)
		return rc;

	rc = batch_op_init(mp);
	if (rc) {
		plt_err("Failed to init batch alloc mem rc=%d", rc);
		goto error;
	}

	return 0;
error:
	cnxk_mempool_free(mp);
	return rc;
}

static void
cn10k_mempool_free(struct rte_mempool *mp)
{
	batch_op_fini(mp);
	cnxk_mempool_free(mp);
}

int
cn10k_mempool_plt_init(void)
{
	return batch_op_data_table_create();
}

static struct rte_mempool_ops cn10k_mempool_ops = {
	.name = "cn10k_mempool_ops",
	.alloc = cn10k_mempool_alloc,
	.free = cn10k_mempool_free,
	.enqueue = cn10k_mempool_enq,
	.dequeue = cn10k_mempool_deq,
	.get_count = cn10k_mempool_get_count,
	.calc_mem_size = cnxk_mempool_calc_mem_size,
	.populate = cnxk_mempool_populate,
};

RTE_MEMPOOL_REGISTER_OPS(cn10k_mempool_ops);
