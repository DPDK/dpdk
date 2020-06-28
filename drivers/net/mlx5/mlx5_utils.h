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
#include <rte_memory.h>
#include <rte_bitmap.h>

#include <mlx5_common.h>

#include "mlx5_defs.h"


/* Convert a bit number to the corresponding 64-bit mask */
#define MLX5_BITSHIFT(v) (UINT64_C(1) << (v))

/* Save and restore errno around argument evaluation. */
#define ERRNO_SAFE(x) ((errno = (int []){ errno, ((x), 0) }[0]))

extern int mlx5_logtype;

/* Generic printf()-like logging macro with automatic line feed. */
#define DRV_LOG(level, ...) \
	PMD_DRV_LOG_(level, mlx5_logtype, MLX5_DRIVER_NAME, \
		__VA_ARGS__ PMD_DRV_LOG_STRIP PMD_DRV_LOG_OPAREN, \
		PMD_DRV_LOG_CPAREN)

#define INFO(...) DRV_LOG(INFO, __VA_ARGS__)
#define WARN(...) DRV_LOG(WARNING, __VA_ARGS__)
#define ERROR(...) DRV_LOG(ERR, __VA_ARGS__)

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
 * array table will be more efficiect than hash table once need to serarch
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
	uint16_t entry[]; /* Entry array. */
};

/* L3 double word entry table data structure. */
struct mlx5_l3t_entry_dword {
	uint32_t idx; /* Table index. */
	uint64_t ref_cnt; /* Table ref_cnt. */
	uint32_t entry[]; /* Entry array. */
};

/* L3 quad word entry table data structure. */
struct mlx5_l3t_entry_qword {
	uint32_t idx; /* Table index. */
	uint64_t ref_cnt; /* Table ref_cnt. */
	uint64_t entry[]; /* Entry array. */
};

/* L3 pointer entry table data structure. */
struct mlx5_l3t_entry_ptr {
	uint32_t idx; /* Table index. */
	uint64_t ref_cnt; /* Table ref_cnt. */
	void *entry[]; /* Entry array. */
};

/* L3 table data structure. */
struct mlx5_l3t_tbl {
	enum mlx5_l3t_type type; /* Table type. */
	struct mlx5_indexed_pool *eip;
	/* Table index pool handles. */
	struct mlx5_l3t_level_tbl *tbl; /* Global table index. */
};

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

struct mlx5_indexed_pool {
	struct mlx5_indexed_pool_config cfg; /* Indexed pool configuration. */
	rte_spinlock_t lock; /* Pool lock for multiple thread usage. */
	uint32_t n_trunk_valid; /* Trunks allocated. */
	uint32_t n_trunk; /* Trunk pointer array size. */
	/* Dim of trunk pointer array. */
	struct mlx5_indexed_trunk **trunks;
	uint32_t free_list; /* Index to first free trunk. */
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

/** Maximum size of string for naming the hlist table. */
#define MLX5_HLIST_NAMESIZE			32

/**
 * Structure of the entry in the hash list, user should define its own struct
 * that contains this in order to store the data. The 'key' is 64-bits right
 * now and its user's responsibility to guarantee there is no collision.
 */
struct mlx5_hlist_entry {
	LIST_ENTRY(mlx5_hlist_entry) next; /* entry pointers in the list. */
	uint64_t key; /* user defined 'key', could be the hash signature. */
};

/** Structure for hash head. */
LIST_HEAD(mlx5_hlist_head, mlx5_hlist_entry);

/** Type of function that is used to handle the data before freeing. */
typedef void (*mlx5_hlist_destroy_callback_fn)(void *p, void *ctx);

/** hash list table structure */
struct mlx5_hlist {
	char name[MLX5_HLIST_NAMESIZE]; /**< Name of the hash list. */
	/**< number of heads, need to be power of 2. */
	uint32_t table_sz;
	/**< mask to get the index of the list heads. */
	uint32_t mask;
	struct mlx5_hlist_head heads[];	/**< list head arrays. */
};

/**
 * Create a hash list table, the user can specify the list heads array size
 * of the table, now the size should be a power of 2 in order to get better
 * distribution for the entries. Each entry is a part of the whole data element
 * and the caller should be responsible for the data element's allocation and
 * cleanup / free. Key of each entry will be calculated with CRC in order to
 * generate a little fairer distribution.
 *
 * @param name
 *   Name of the hash list(optional).
 * @param size
 *   Heads array size of the hash list.
 *
 * @return
 *   Pointer of the hash list table created, NULL on failure.
 */
struct mlx5_hlist *mlx5_hlist_create(const char *name, uint32_t size);

/**
 * Search an entry matching the key.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param key
 *   Key for the searching entry.
 *
 * @return
 *   Pointer of the hlist entry if found, NULL otherwise.
 */
struct mlx5_hlist_entry *mlx5_hlist_lookup(struct mlx5_hlist *h, uint64_t key);

/**
 * Insert an entry to the hash list table, the entry is only part of whole data
 * element and a 64B key is used for matching. User should construct the key or
 * give a calculated hash signature and guarantee there is no collision.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param entry
 *   Entry to be inserted into the hash list table.
 *
 * @return
 *   - zero for success.
 *   - -EEXIST if the entry is already inserted.
 */
int mlx5_hlist_insert(struct mlx5_hlist *h, struct mlx5_hlist_entry *entry);

/**
 * Remove an entry from the hash list table. User should guarantee the validity
 * of the entry.
 *
 * @param h
 *   Pointer to the hast list table. (not used)
 * @param entry
 *   Entry to be removed from the hash list table.
 */
void mlx5_hlist_remove(struct mlx5_hlist *h __rte_unused,
		       struct mlx5_hlist_entry *entry);

/**
 * Destroy the hash list table, all the entries already inserted into the lists
 * will be handled by the callback function provided by the user (including
 * free if needed) before the table is freed.
 *
 * @param h
 *   Pointer to the hast list table.
 * @param cb
 *   Callback function for each inserted entry when destroying the hash list.
 * @param ctx
 *   Common context parameter used by callback function for each entry.
 */
void mlx5_hlist_destroy(struct mlx5_hlist *h,
			mlx5_hlist_destroy_callback_fn cb, void *ctx);

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

uint32_t mlx5_l3t_get_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
			    union mlx5_l3t_data *data);
/**
 * This function clears the index entry from Three-level table.
 *
 * @param tbl
 *   Pointer to the l3t.
 * @param idx
 *   Index to the entry.
 */
void mlx5_l3t_clear_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx);

/**
 * This function gets the index entry from Three-level table.
 *
 * @param tbl
 *   Pointer to the l3t.
 * @param idx
 *   Index to the entry.
 * @param data
 *   Pointer to the memory which contains the entry data save to l3t.
 *
 * @return
 *   0 if success, -1 on error.
 */
uint32_t mlx5_l3t_set_entry(struct mlx5_l3t_tbl *tbl, uint32_t idx,
			    union mlx5_l3t_data *data);

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

#endif /* RTE_PMD_MLX5_UTILS_H_ */
