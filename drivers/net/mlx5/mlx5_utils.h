/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_UTILS_H_
#define RTE_PMD_MLX5_UTILS_H_

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <limits.h>
#include <errno.h>

#include <rte_spinlock.h>
#include <rte_rwlock.h>
#include <rte_memory.h>
#include <rte_bitmap.h>

#include <mlx5_common.h>
#include <mlx5_common_utils.h>

#include "mlx5_defs.h"

/* Convert a bit number to the corresponding 64-bit mask */
#define MLX5_BITSHIFT(v) (UINT64_C(1) << (v))

/* Save and restore errno around argument evaluation. */
#define ERRNO_SAFE(x) ((errno = (int []){ errno, ((x), 0) }[0]))

extern int mlx5_logtype;

#define MLX5_NET_LOG_PREFIX "mlx5_net"

/* Generic printf()-like logging macro with automatic line feed. */
#define DRV_LOG(level, ...) \
	PMD_DRV_LOG_(level, mlx5_logtype, MLX5_NET_LOG_PREFIX, \
		__VA_ARGS__ PMD_DRV_LOG_STRIP PMD_DRV_LOG_OPAREN, \
		PMD_DRV_LOG_CPAREN)

/* Convenience macros for accessing mbuf fields. */
#define NEXT(m) ((m)->next)
#define DATA_LEN(m) ((m)->data_len)
#define PKT_LEN(m) ((m)->pkt_len)
#define DATA_OFF(m) ((m)->data_off)
#define SET_DATA_OFF(m, o) ((m)->data_off = (o))
#define NB_SEGS(m) ((m)->nb_segs)
#define PORT(m) ((m)->port)

/* Transpose flags. Useful to convert IBV to DPDK flags. */
#define TRANSPOSE(val, from, to) \
	(((from) >= (to)) ? \
	 (((val) & (from)) / ((from) / (to))) : \
	 (((val) & (from)) * ((to) / (from))))

/*
 * For the case which data is linked with sequence increased index, the
 * array table will be more efficient than hash table once need to search
 * one data entry in large numbers of entries. Since the traditional hash
 * tables has fixed table size, when huge numbers of data saved to the hash
 * table, it also comes lots of hash conflict.
 *
 * But simple array table also has fixed size, allocates all the needed
 * memory at once will waste lots of memory. For the case don't know the
 * exactly number of entries will be impossible to allocate the array.
 *
 * Then the multiple level table helps to balance the two disadvantages.
 * Allocate a global high level table with sub table entries at first,
 * the global table contains the sub table entries, and the sub table will
 * be allocated only once the corresponding index entry need to be saved.
 * e.g. for up to 32-bits index, three level table with 10-10-12 splitting,
 * with sequence increased index, the memory grows with every 4K entries.
 *
 * The currently implementation introduces 10-10-12 32-bits splitting
 * Three-Level table to help the cases which have millions of enties to
 * save. The index entries can be addressed directly by the index, no
 * search will be needed.q
 */

/* L3 table global table define. */
#define MLX5_L3T_GT_OFFSET 22
#define MLX5_L3T_GT_SIZE (1 << 10)
#define MLX5_L3T_GT_MASK (MLX5_L3T_GT_SIZE - 1)

/* L3 table middle table define. */
#define MLX5_L3T_MT_OFFSET 12
#define MLX5_L3T_MT_SIZE (1 << 10)
#define MLX5_L3T_MT_MASK (MLX5_L3T_MT_SIZE - 1)

/* L3 table entry table define. */
#define MLX5_L3T_ET_OFFSET 0
#define MLX5_L3T_ET_SIZE (1 << 12)
#define MLX5_L3T_ET_MASK (MLX5_L3T_ET_SIZE - 1)

/* L3 table type. */
enum mlx5_l3t_type {
	MLX5_L3T_TYPE_WORD = 0,
	MLX5_L3T_TYPE_DWORD,
	MLX5_L3T_TYPE_QWORD,
	MLX5_L3T_TYPE_PTR,
	MLX5_L3T_TYPE_MAX,
};

struct mlx5_indexed_pool;

/* Generic data struct. */
union mlx5_l3t_data {
	uint16_t word;
	uint32_t dword;
	uint64_t qword;
	void *ptr;
};

/* L3 level table data structure. */
struct mlx5_l3t_level_tbl {
	uint64_t ref_cnt; /* Table ref_cnt. */
	void *tbl[]; /* Table array. */
};

/* L3 word entry table data structure. */
struct mlx5_l3t_entry_word {
	uint32_t idx; /* Table index. */
	uint64_t ref_cnt; /* Table ref_cnt. */
	struct {
		uint16_t data;
		uint32_t ref_cnt;
	} entry[MLX5_L3T_ET_SIZE]; /* Entry array */
} __rte_packed;

/* L3 double word entry table data structure. */
struct mlx5_l3t_entry_dword {
	uint32_t idx; /* Table index. */
	uint64_t ref_cnt; /* Table ref_cnt. */
	struct {
		uint32_t data;
		int32_t ref_cnt;
	} entry[MLX5_L3T_ET_SIZE]; /* Entry array */
} __rte_packed;

/* L3 quad word entry table data structure. */
struct mlx5_l3t_entry_qword {
	uint32_t idx; /* Table index. */
	uint64_t ref_cnt; /* Table ref_cnt. */
	struct {
		uint64_t data;
		uint32_t ref_cnt;
	} entry[MLX5_L3T_ET_SIZE]; /* Entry array */
} __rte_packed;

/* L3 pointer entry table data structure. */
struct mlx5_l3t_entry_ptr {
	uint32_t idx; /* Table index. */
	uint64_t ref_cnt; /* Table ref_cnt. */
	struct {
		void *data;
		uint32_t ref_cnt;
	} entry[MLX5_L3T_ET_SIZE]; /* Entry array */
} __rte_packed;

/* L3 table data structure. */
struct mlx5_l3t_tbl {
	enum mlx5_l3t_type type; /* Table type. */
	struct mlx5_indexed_pool *eip;
	/* Table index pool handles. */
	struct mlx5_l3t_level_tbl *tbl; /* Global table index. */
	rte_spinlock_t sl; /* The table lock. */
};

/** Type of function that is used to handle the data before freeing. */
typedef int32_t (*mlx5_l3t_alloc_callback_fn)(void *ctx,
					   union mlx5_l3t_data *data);

/*
 * The indexed memory entry index is made up of trunk index and offset of
 * the entry in the trunk. Since the entry index is 32 bits, in case user
 * prefers to have small trunks, user can change the macro below to a big
 * number which helps the pool contains more trunks with lots of entries
 * allocated.
 */
#define TRUNK_IDX_BITS 16
#define TRUNK_MAX_IDX ((1 << TRUNK_IDX_BITS) - 1)
#define TRUNK_INVALID TRUNK_MAX_IDX
#define MLX5_IPOOL_DEFAULT_TRUNK_SIZE (1 << (28 - TRUNK_IDX_BITS))
#ifdef RTE_LIBRTE_MLX5_DEBUG
#define POOL_DEBUG 1
#endif

struct mlx5_indexed_pool_config {
	uint32_t size; /* Pool entry size. */
	uint32_t trunk_size:22;
	/*
	 * Trunk entry number. Must be power of 2. It can be increased
	 * if trunk_grow enable. The trunk entry number increases with
	 * left shift grow_shift. Trunks with index are after grow_trunk
	 * will keep the entry number same with the last grow trunk.
	 */
	uint32_t grow_trunk:4;
	/*
	 * Trunks with entry number increase in the pool. Set it to 0
	 * to make the pool works as trunk entry fixed pool. It works
	 * only if grow_shift is not 0.
	 */
	uint32_t grow_shift:4;
	/*
	 * Trunk entry number increase shift value, stop after grow_trunk.
	 * It works only if grow_trunk is not 0.
	 */
	uint32_t need_lock:1;
	/* Lock is needed for multiple thread usage. */
	uint32_t release_mem_en:1; /* Rlease trunk when it is free. */
	uint32_t max_idx; /* The maximum index can be allocated. */
	uint32_t per_core_cache;
	/*
	 * Cache entry number per core for performance. Should not be
	 * set with release_mem_en.
	 */
	const char *type; /* Memory allocate type name. */
	void *(*malloc)(uint32_t flags, size_t size, unsigned int align,
			int socket);
	/* User defined memory allocator. */
	void (*free)(void *addr); /* User defined memory release. */
};

struct mlx5_indexed_trunk {
	uint32_t idx; /* Trunk id. */
	uint32_t prev; /* Previous free trunk in free list. */
	uint32_t next; /* Next free trunk in free list. */
	uint32_t free; /* Free entries available */
	struct rte_bitmap *bmp;
	uint8_t data[] __rte_cache_aligned; /* Entry data start. */
};

struct mlx5_indexed_cache {
	struct mlx5_indexed_trunk **trunks;
	volatile uint32_t n_trunk_valid; /* Trunks allocated. */
	uint32_t n_trunk; /* Trunk pointer array size. */
	uint32_t ref_cnt;
	uint32_t len;
	uint32_t idx[];
};

struct mlx5_ipool_per_lcore {
	struct mlx5_indexed_cache *lc;
	uint32_t len; /**< Current cache count. */
	uint32_t idx[]; /**< Cache objects. */
};

struct mlx5_indexed_pool {
	struct mlx5_indexed_pool_config cfg; /* Indexed pool configuration. */
	rte_spinlock_t rsz_lock; /* Pool lock for multiple thread usage. */
	rte_spinlock_t lcore_lock;
	/* Dim of trunk pointer array. */
	union {
		struct {
			uint32_t n_trunk_valid; /* Trunks allocated. */
			uint32_t n_trunk; /* Trunk pointer array size. */
			struct mlx5_indexed_trunk **trunks;
			uint32_t free_list; /* Index to first free trunk. */
		};
		struct {
			struct mlx5_indexed_cache *gc;
			/* Global cache. */
			struct mlx5_ipool_per_lcore *cache[RTE_MAX_LCORE + 1];
			/* Local cache. */
			struct rte_bitmap *ibmp;
			void *bmp_mem;
			/* Allocate objects bitmap. Use during flush. */
		};
	};
#ifdef POOL_DEBUG
	uint32_t n_entry;
	uint32_t trunk_new;
	uint32_t trunk_avail;
	uint32_t trunk_empty;
	uint32_t trunk_free;
#endif
	uint32_t grow_tbl[]; /* Save the index offset for the grow trunks. */
};

/**
 * Return logarithm of the nearest power of two above input value.
 *
 * @param v
 *   Input value.
 *
 * @return
 *   Logarithm of the nearest power of two above input value.
 */
static inline unsigned int
log2above(unsigned int v)
{
	unsigned int l;
	unsigned int r;

	for (l = 0, r = 0; (v >> 1); ++l, v >>= 1)
		r |= (v & 1);
	return l + r;
}

/********************************* indexed pool *************************/

/**
 * This function allocates non-initialized memory entry from pool.
 * In NUMA systems, the memory entry allocated resides on the same
 * NUMA socket as the core that calls this function.
 *
 * Memory entry is allocated from memory trunk, no alignment.
 *
 * @param pool
 *   Pointer to indexed memory entry pool.
 *   No initialization required.
 * @param[out] idx
 *   Pointer to memory to save allocated index.
 *   Memory index always positive value.
 * @return
 *   - Pointer to the allocated memory entry.
 *   - NULL on error. Not enough memory, or invalid arguments.
 */
void *mlx5_ipool_malloc(struct mlx5_indexed_pool *pool, uint32_t *idx);

/**
 * This function allocates zero initialized memory entry from pool.
 * In NUMA systems, the memory entry allocated resides on the same
 * NUMA socket as the core that calls this function.
 *
 * Memory entry is allocated from memory trunk, no alignment.
 *
 * @param pool
 *   Pointer to indexed memory pool.
 *   No initialization required.
 * @param[out] idx
 *   Pointer to memory to save allocated index.
 *   Memory index always positive value.
 * @return
 *   - Pointer to the allocated memory entry .
 *   - NULL on error. Not enough memory, or invalid arguments.
 */
void *mlx5_ipool_zmalloc(struct mlx5_indexed_pool *pool, uint32_t *idx);

/**
 * This function frees indexed memory entry to pool.
 * Caller has to make sure that the index is allocated from same pool.
 *
 * @param pool
 *   Pointer to indexed memory pool.
 * @param idx
 *   Allocated memory entry index.
 */
void mlx5_ipool_free(struct mlx5_indexed_pool *pool, uint32_t idx);

/**
 * This function returns pointer of indexed memory entry from index.
 * Caller has to make sure that the index is valid, and allocated
 * from same pool.
 *
 * @param pool
 *   Pointer to indexed memory pool.
 * @param idx
 *   Allocated memory index.
 * @return
 *   - Pointer to indexed memory entry.
 */
void *mlx5_ipool_get(struct mlx5_indexed_pool *pool, uint32_t idx);

/**
 * This function creates indexed memory pool.
 * Caller has to configure the configuration accordingly.
 *
 * @param pool
 *   Pointer to indexed memory pool.
 * @param cfg
 *   Allocated memory index.
 */
struct mlx5_indexed_pool *
mlx5_ipool_create(struct mlx5_indexed_pool_config *cfg);

/**
 * This function releases all resources of pool.
 * Caller has to make sure that all indexes and memories allocated
 * from this pool not referenced anymore.
 *
 * @param pool
 *   Pointer to indexed memory pool.
 * @return
 *   - non-zero value on error.
 *   - 0 on success.
 */
int mlx5_ipool_destroy(struct mlx5_indexed_pool *pool);

/**
 * This function dumps debug info of pool.
 *
 * @param pool
 *   Pointer to indexed memory pool.
 */
void mlx5_ipool_dump(struct mlx5_indexed_pool *pool);

/**
 * This function flushes all the cache index back to pool trunk.
 *
 * @param pool
 *   Pointer to the index memory pool handler.
 *
 */

void mlx5_ipool_flush_cache(struct mlx5_indexed_pool *pool);

/**
 * This function gets the available entry from pos.
 *
 * @param pool
 *   Pointer to the index memory pool handler.
 * @param pos
 *   Pointer to the index position start from.
 *
 * @return
 *  - Pointer to the next available entry.
 *
 */
void *mlx5_ipool_get_next(struct mlx5_indexed_pool *pool, uint32_t *pos);

/**
 * This function allocates new empty Three-level table.
 *
 * @param type
 *   The l3t can set as word, double word, quad word or pointer with index.
 *
 * @return
 *   - Pointer to the allocated l3t.
 *   - NULL on error. Not enough memory, or invalid arguments.
 */
struct mlx5_l3t_tbl *mlx5_l3t_create(enum mlx5_l3t_type type);

/**
 * This function destroys Three-level table.
 *
 * @param tbl
 *   Pointer to the l3t.
 */
void mlx5_l3t_destroy(struct mlx5_l3t_tbl *tbl);

/**
 * This function gets the index entry from Three-level table.
 *
 * @param tbl
 *   Pointer to the l3t.
 * @param idx
 *   Index to the entry.
 * @param data
 *   Pointer to the memory which saves the entry data.
 *   When function call returns 0, data contains the entry data get from
 *   l3t.
 *   When function call returns -1, data is not modified.
 *
 * @return
 *   0 if success, -1 on error.
 */

int32_t mlx5_l3t_get_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
			    union mlx5_l3t_data *data);

/**
 * This function decreases and clear index entry if reference
 * counter is 0 from Three-level table.
 *
 * @param tbl
 *   Pointer to the l3t.
 * @param idx
 *   Index to the entry.
 *
 * @return
 *   The remaining reference count, 0 means entry be cleared, -1 on error.
 */
int32_t mlx5_l3t_clear_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx);

/**
 * This function sets the index entry to Three-level table.
 * If the entry is already set, the EEXIST errno will be given, and
 * the set data will be filled to the data.
 *
 * @param tbl[in]
 *   Pointer to the l3t.
 * @param idx[in]
 *   Index to the entry.
 * @param data[in/out]
 *   Pointer to the memory which contains the entry data save to l3t.
 *   If the entry is already set, the set data will be filled.
 *
 * @return
 *   0 if success, -1 on error.
 */
int32_t mlx5_l3t_set_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
			    union mlx5_l3t_data *data);

static inline void *
mlx5_l3t_get_next(struct mlx5_l3t_tbl *tbl, uint32_t *pos)
{
	struct mlx5_l3t_level_tbl *g_tbl, *m_tbl;
	uint32_t i, j, k, g_start, m_start, e_start;
	uint32_t idx = *pos;
	void *e_tbl;
	struct mlx5_l3t_entry_word *w_e_tbl;
	struct mlx5_l3t_entry_dword *dw_e_tbl;
	struct mlx5_l3t_entry_qword *qw_e_tbl;
	struct mlx5_l3t_entry_ptr *ptr_e_tbl;

	if (!tbl)
		return NULL;
	g_tbl = tbl->tbl;
	if (!g_tbl)
		return NULL;
	g_start = (idx >> MLX5_L3T_GT_OFFSET) & MLX5_L3T_GT_MASK;
	m_start = (idx >> MLX5_L3T_MT_OFFSET) & MLX5_L3T_MT_MASK;
	e_start = idx & MLX5_L3T_ET_MASK;
	for (i = g_start; i < MLX5_L3T_GT_SIZE; i++) {
		m_tbl = g_tbl->tbl[i];
		if (!m_tbl) {
			/* Jump to new table, reset the sub table start. */
			m_start = 0;
			e_start = 0;
			continue;
		}
		for (j = m_start; j < MLX5_L3T_MT_SIZE; j++) {
			if (!m_tbl->tbl[j]) {
				/*
				 * Jump to new table, reset the sub table
				 * start.
				 */
				e_start = 0;
				continue;
			}
			e_tbl = m_tbl->tbl[j];
			switch (tbl->type) {
			case MLX5_L3T_TYPE_WORD:
				w_e_tbl = (struct mlx5_l3t_entry_word *)e_tbl;
				for (k = e_start; k < MLX5_L3T_ET_SIZE; k++) {
					if (!w_e_tbl->entry[k].data)
						continue;
					*pos = (i << MLX5_L3T_GT_OFFSET) |
					       (j << MLX5_L3T_MT_OFFSET) | k;
					return (void *)&w_e_tbl->entry[k].data;
				}
				break;
			case MLX5_L3T_TYPE_DWORD:
				dw_e_tbl = (struct mlx5_l3t_entry_dword *)e_tbl;
				for (k = e_start; k < MLX5_L3T_ET_SIZE; k++) {
					if (!dw_e_tbl->entry[k].data)
						continue;
					*pos = (i << MLX5_L3T_GT_OFFSET) |
					       (j << MLX5_L3T_MT_OFFSET) | k;
					return (void *)&dw_e_tbl->entry[k].data;
				}
				break;
			case MLX5_L3T_TYPE_QWORD:
				qw_e_tbl = (struct mlx5_l3t_entry_qword *)e_tbl;
				for (k = e_start; k < MLX5_L3T_ET_SIZE; k++) {
					if (!qw_e_tbl->entry[k].data)
						continue;
					*pos = (i << MLX5_L3T_GT_OFFSET) |
					       (j << MLX5_L3T_MT_OFFSET) | k;
					return (void *)&qw_e_tbl->entry[k].data;
				}
				break;
			default:
				ptr_e_tbl = (struct mlx5_l3t_entry_ptr *)e_tbl;
				for (k = e_start; k < MLX5_L3T_ET_SIZE; k++) {
					if (!ptr_e_tbl->entry[k].data)
						continue;
					*pos = (i << MLX5_L3T_GT_OFFSET) |
					       (j << MLX5_L3T_MT_OFFSET) | k;
					return ptr_e_tbl->entry[k].data;
				}
				break;
			}
		}
	}
	return NULL;
}

/*
 * Macros for linked list based on indexed memory.
 * Example data structure:
 * struct Foo {
 *	ILIST_ENTRY(uint16_t) next;
 *	...
 * }
 *
 */
#define ILIST_ENTRY(type)						\
struct {								\
	type prev; /* Index of previous element. */			\
	type next; /* Index of next element. */				\
}

#define ILIST_INSERT(pool, head, idx, elem, field)			\
	do {								\
		typeof(elem) peer;					\
		MLX5_ASSERT((elem) && (idx));				\
		(elem)->field.next = *(head);				\
		(elem)->field.prev = 0;					\
		if (*(head)) {						\
			(peer) = mlx5_ipool_get(pool, *(head));		\
			if (peer)					\
				(peer)->field.prev = (idx);		\
		}							\
		*(head) = (idx);					\
	} while (0)

#define ILIST_REMOVE(pool, head, idx, elem, field)			\
	do {								\
		typeof(elem) peer;					\
		MLX5_ASSERT(elem);					\
		MLX5_ASSERT(head);					\
		if ((elem)->field.prev) {				\
			(peer) = mlx5_ipool_get				\
				 (pool, (elem)->field.prev);		\
			if (peer)					\
				(peer)->field.next = (elem)->field.next;\
		}							\
		if ((elem)->field.next) {				\
			(peer) = mlx5_ipool_get				\
				 (pool, (elem)->field.next);		\
			if (peer)					\
				(peer)->field.prev = (elem)->field.prev;\
		}							\
		if (*(head) == (idx))					\
			*(head) = (elem)->field.next;			\
	} while (0)

#define ILIST_FOREACH(pool, head, idx, elem, field)			\
	for ((idx) = (head), (elem) =					\
	     (idx) ? mlx5_ipool_get(pool, (idx)) : NULL; (elem);	\
	     idx = (elem)->field.next, (elem) =				\
	     (idx) ? mlx5_ipool_get(pool, idx) : NULL)

/* Single index list. */
#define SILIST_ENTRY(type)						\
struct {								\
	type next; /* Index of next element. */				\
}

#define SILIST_INSERT(head, idx, elem, field)				\
	do {								\
		MLX5_ASSERT((elem) && (idx));				\
		(elem)->field.next = *(head);				\
		*(head) = (idx);					\
	} while (0)

#define SILIST_FOREACH(pool, head, idx, elem, field)			\
	for ((idx) = (head), (elem) =					\
	     (idx) ? mlx5_ipool_get(pool, (idx)) : NULL; (elem);	\
	     idx = (elem)->field.next, (elem) =				\
	     (idx) ? mlx5_ipool_get(pool, idx) : NULL)

#define MLX5_L3T_FOREACH(tbl, idx, entry)				\
	for (idx = 0, (entry) = mlx5_l3t_get_next((tbl), &idx);		\
	     (entry);							\
	     idx++, (entry) = mlx5_l3t_get_next((tbl), &idx))

#define MLX5_IPOOL_FOREACH(ipool, idx, entry)				\
	for ((idx) = 0, mlx5_ipool_flush_cache((ipool)),		\
	    (entry) = mlx5_ipool_get_next((ipool), &idx);		\
	    (entry); idx++, (entry) = mlx5_ipool_get_next((ipool), &idx))

#endif /* RTE_PMD_MLX5_UTILS_H_ */
