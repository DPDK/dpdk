/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 *
 * Copyright 2008-2012 Freescale Semiconductor, Inc.
 *
 */

#ifndef __FSL_BMAN_H
#define __FSL_BMAN_H

#ifdef __cplusplus
extern "C" {
#endif

/* This wrapper represents a bit-array for the depletion state of the 64 Bman
 * buffer pools.
 */
struct bman_depletion {
	u32 state[2];
};

static inline void bman_depletion_init(struct bman_depletion *c)
{
	c->state[0] = c->state[1] = 0;
}

static inline void bman_depletion_fill(struct bman_depletion *c)
{
	c->state[0] = c->state[1] = ~0;
}

/* --- Bman data structures (and associated constants) --- */

/* Represents s/w corenet portal mapped data structures */
struct bm_rcr_entry;	/* RCR (Release Command Ring) entries */
struct bm_mc_command;	/* MC (Management Command) command */
struct bm_mc_result;	/* MC result */

/* Code-reduction, define a wrapper for 48-bit buffers. In cases where a buffer
 * pool id specific to this buffer is needed (BM_RCR_VERB_CMD_BPID_MULTI,
 * BM_MCC_VERB_ACQUIRE), the 'bpid' field is used.
 */
struct bm_buffer {
	union {
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u8 __reserved1;
			u8 bpid;
			u16 hi; /* High 16-bits of 48-bit address */
			u32 lo; /* Low 32-bits of 48-bit address */
#else
			u32 lo;
			u16 hi;
			u8 bpid;
			u8 __reserved;
#endif
		};
		struct {
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
			u64 __notaddress:16;
			u64 addr:48;
#else
			u64 addr:48;
			u64 __notaddress:16;
#endif
		};
		u64 opaque;
	};
} __rte_aligned(8);
static inline u64 bm_buffer_get64(const struct bm_buffer *buf)
{
	return buf->addr;
}

static inline dma_addr_t bm_buf_addr(const struct bm_buffer *buf)
{
	return (dma_addr_t)buf->addr;
}

#define bm_buffer_set64(buf, v) \
	do { \
		struct bm_buffer *__buf931 = (buf); \
		__buf931->hi = upper_32_bits(v); \
		__buf931->lo = lower_32_bits(v); \
	} while (0)

/* See 1.5.3.5.4: "Release Command" */
struct bm_rcr_entry {
	union {
		struct {
			u8 __dont_write_directly__verb;
			u8 bpid; /* used with BM_RCR_VERB_CMD_BPID_SINGLE */
			u8 __reserved1[62];
		};
		struct bm_buffer bufs[8];
	};
} __packed;
#define BM_RCR_VERB_VBIT		0x80
#define BM_RCR_VERB_CMD_MASK		0x70	/* one of two values; */
#define BM_RCR_VERB_CMD_BPID_SINGLE	0x20
#define BM_RCR_VERB_CMD_BPID_MULTI	0x30
#define BM_RCR_VERB_BUFCOUNT_MASK	0x0f	/* values 1..8 */

/* See 1.5.3.1: "Acquire Command" */
/* See 1.5.3.2: "Query Command" */
struct bm_mcc_acquire {
	u8 bpid;
	u8 __reserved1[62];
} __packed;
struct bm_mcc_query {
	u8 __reserved2[63];
} __packed;
struct bm_mc_command {
	u8 __dont_write_directly__verb;
	union {
		struct bm_mcc_acquire acquire;
		struct bm_mcc_query query;
	};
} __packed;
#define BM_MCC_VERB_VBIT		0x80
#define BM_MCC_VERB_CMD_MASK		0x70	/* where the verb contains; */
#define BM_MCC_VERB_CMD_ACQUIRE		0x10
#define BM_MCC_VERB_CMD_QUERY		0x40
#define BM_MCC_VERB_ACQUIRE_BUFCOUNT	0x0f	/* values 1..8 go here */

/* See 1.5.3.3: "Acquire Response" */
/* See 1.5.3.4: "Query Response" */
struct bm_pool_state {
	u8 __reserved1[32];
	/* "availability state" and "depletion state" */
	struct {
		u8 __reserved1[8];
		/* Access using bman_depletion_***() */
		struct bman_depletion state;
	} as, ds;
};

struct bm_mc_result {
	union {
		struct {
			u8 verb;
			u8 __reserved1[63];
		};
		union {
			struct {
				u8 __reserved1;
				u8 bpid;
				u8 __reserved2[62];
			};
			struct bm_buffer bufs[8];
		} acquire;
		struct bm_pool_state query;
	};
} __packed;
#define BM_MCR_VERB_VBIT		0x80
#define BM_MCR_VERB_CMD_MASK		BM_MCC_VERB_CMD_MASK
#define BM_MCR_VERB_CMD_ACQUIRE		BM_MCC_VERB_CMD_ACQUIRE
#define BM_MCR_VERB_CMD_QUERY		BM_MCC_VERB_CMD_QUERY
#define BM_MCR_VERB_CMD_ERR_INVALID	0x60
#define BM_MCR_VERB_CMD_ERR_ECC		0x70
#define BM_MCR_VERB_ACQUIRE_BUFCOUNT	BM_MCC_VERB_ACQUIRE_BUFCOUNT /* 0..8 */

/* Portal and Buffer Pools */
/* Represents a managed portal */
struct bman_portal;

/* This object type represents Bman buffer pools. */
struct bman_pool;

/* This struct specifies parameters for a bman_pool object. */
struct bman_pool_params {
	/* index of the buffer pool to encapsulate (0-63), ignored if
	 * BMAN_POOL_FLAG_DYNAMIC_BPID is set.
	 */
	u32 bpid;
	/* bit-mask of BMAN_POOL_FLAG_*** options */
	u32 flags;
	/* depletion-entry/exit thresholds, if BMAN_POOL_FLAG_THRESH is set. NB:
	 * this is only allowed if BMAN_POOL_FLAG_DYNAMIC_BPID is used *and*
	 * when run in the control plane (which controls Bman CCSR). This array
	 * matches the definition of bm_pool_set().
	 */
	u32 thresholds[4];
};

/* Flags to bman_new_pool() */
#define BMAN_POOL_FLAG_NO_RELEASE    0x00000001 /* can't release to pool */
#define BMAN_POOL_FLAG_ONLY_RELEASE  0x00000002 /* can only release to pool */
#define BMAN_POOL_FLAG_DYNAMIC_BPID  0x00000008 /* (de)allocate bpid */
#define BMAN_POOL_FLAG_THRESH        0x00000010 /* set depletion thresholds */

/* Flags to bman_release() */
#define BMAN_RELEASE_FLAG_NOW        0x00000008 /* issue immediate release */


/**
 * bman_get_portal_index - get portal configuration index
 */
int bman_get_portal_index(void);

/**
 * bman_rcr_is_empty - Determine if portal's RCR is empty
 *
 * For use in situations where a cpu-affine caller needs to determine when all
 * releases for the local portal have been processed by Bman but can't use the
 * BMAN_RELEASE_FLAG_WAIT_SYNC flag to do this from the final bman_release().
 * The function forces tracking of RCR consumption (which normally doesn't
 * happen until release processing needs to find space to put new release
 * commands), and returns zero if the ring still has unprocessed entries,
 * non-zero if it is empty.
 */
int bman_rcr_is_empty(void);

/**
 * bman_alloc_bpid_range - Allocate a contiguous range of BPIDs
 * @result: is set by the API to the base BPID of the allocated range
 * @count: the number of BPIDs required
 * @align: required alignment of the allocated range
 * @partial: non-zero if the API can return fewer than @count BPIDs
 *
 * Returns the number of buffer pools allocated, or a negative error code. If
 * @partial is non zero, the allocation request may return a smaller range of
 * BPs than requested (though alignment will be as requested). If @partial is
 * zero, the return value will either be 'count' or negative.
 */
int bman_alloc_bpid_range(u32 *result, u32 count, u32 align, int partial);
static inline int bman_alloc_bpid(u32 *result)
{
	int ret = bman_alloc_bpid_range(result, 1, 0, 0);

	return (ret > 0) ? 0 : ret;
}

/**
 * bman_release_bpid_range - Release the specified range of buffer pool IDs
 * @bpid: the base BPID of the range to deallocate
 * @count: the number of BPIDs in the range
 *
 * This function can also be used to seed the allocator with ranges of BPIDs
 * that it can subsequently allocate from.
 */
void bman_release_bpid_range(u32 bpid, unsigned int count);
static inline void bman_release_bpid(u32 bpid)
{
	bman_release_bpid_range(bpid, 1);
}

int bman_reserve_bpid_range(u32 bpid, unsigned int count);
static inline int bman_reserve_bpid(u32 bpid)
{
	return bman_reserve_bpid_range(bpid, 1);
}

void bman_seed_bpid_range(u32 bpid, unsigned int count);

int bman_shutdown_pool(u32 bpid);

/**
 * bman_new_pool - Allocates a Buffer Pool object
 * @params: parameters specifying the buffer pool ID and behaviour
 *
 * Creates a pool object for the given @params. A portal and the depletion
 * callback field of @params are only used if the BMAN_POOL_FLAG_DEPLETION flag
 * is set. NB, the fields from @params are copied into the new pool object, so
 * the structure provided by the caller can be released or reused after the
 * function returns.
 */
__rte_internal
struct bman_pool *bman_new_pool(const struct bman_pool_params *params);

/**
 * bman_free_pool - Deallocates a Buffer Pool object
 * @pool: the pool object to release
 */
__rte_internal
void bman_free_pool(struct bman_pool *pool);

/**
 * bman_get_params - Returns a pool object's parameters.
 * @pool: the pool object
 *
 * The returned pointer refers to state within the pool object so must not be
 * modified and can no longer be read once the pool object is destroyed.
 */
__rte_internal
const struct bman_pool_params *bman_get_params(const struct bman_pool *pool);

/**
 * bman_release - Release buffer(s) to the buffer pool
 * @pool: the buffer pool object to release to
 * @bufs: an array of buffers to release
 * @num: the number of buffers in @bufs (1-8)
 * @flags: bit-mask of BMAN_RELEASE_FLAG_*** options
 *
 */
__rte_internal
int bman_release(struct bman_pool *pool, const struct bm_buffer *bufs, u8 num,
		 u32 flags);

/**
 * bman_acquire - Acquire buffer(s) from a buffer pool
 * @pool: the buffer pool object to acquire from
 * @bufs: array for storing the acquired buffers
 * @num: the number of buffers desired (@bufs is at least this big)
 *
 * Issues an "Acquire" command via the portal's management command interface.
 * The return value will be the number of buffers obtained from the pool, or a
 * negative error code if a h/w error or pool starvation was encountered.
 */
__rte_internal
int bman_acquire(struct bman_pool *pool, struct bm_buffer *bufs, u8 num,
		 u32 flags);

/**
 * bman_query_pools - Query all buffer pool states
 * @state: storage for the queried availability and depletion states
 */
int bman_query_pools(struct bm_pool_state *state);

/**
 * bman_query_free_buffers - Query how many free buffers are in buffer pool
 * @pool: the buffer pool object to query
 *
 * Return the number of the free buffers
 */
__rte_internal
u32 bman_query_free_buffers(struct bman_pool *pool);

/**
 * bman_update_pool_thresholds - Change the buffer pool's depletion thresholds
 * @pool: the buffer pool object to which the thresholds will be set
 * @thresholds: the new thresholds
 */
int bman_update_pool_thresholds(struct bman_pool *pool, const u32 *thresholds);

/**
 * bm_pool_set_hw_threshold - Change the buffer pool's thresholds
 * @pool: Pool id
 * @low_thresh: low threshold
 * @high_thresh: high threshold
 */
int bm_pool_set_hw_threshold(u32 bpid, const u32 low_thresh,
			     const u32 high_thresh);

#ifdef __cplusplus
}
#endif

#endif /* __FSL_BMAN_H */
