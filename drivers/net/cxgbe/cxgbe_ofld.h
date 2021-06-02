/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Chelsio Communications.
 * All rights reserved.
 */

#ifndef _CXGBE_OFLD_H_
#define _CXGBE_OFLD_H_

#include <rte_bitmap.h>

#include "cxgbe_filter.h"

#define INIT_TP_WR(w, tid) do { \
	(w)->wr.wr_hi = cpu_to_be32(V_FW_WR_OP(FW_TP_WR) | \
				V_FW_WR_IMMDLEN(sizeof(*w) - sizeof(w->wr))); \
	(w)->wr.wr_mid = cpu_to_be32( \
				V_FW_WR_LEN16(DIV_ROUND_UP(sizeof(*w), 16)) | \
				V_FW_WR_FLOWID(tid)); \
	(w)->wr.wr_lo = cpu_to_be64(0); \
} while (0)

#define INIT_TP_WR_MIT_CPL(w, cpl, tid) do { \
	INIT_TP_WR(w, tid); \
	OPCODE_TID(w) = cpu_to_be32(MK_OPCODE_TID(cpl, tid)); \
} while (0)

#define INIT_ULPTX_WR(w, wrlen, atomic, tid) do { \
	(w)->wr.wr_hi = cpu_to_be32(V_FW_WR_OP(FW_ULPTX_WR) | \
				    V_FW_WR_ATOMIC(atomic)); \
	(w)->wr.wr_mid = cpu_to_be32(V_FW_WR_LEN16(DIV_ROUND_UP(wrlen, 16)) | \
				     V_FW_WR_FLOWID(tid)); \
	(w)->wr.wr_lo = cpu_to_be64(0); \
} while (0)

/*
 * Max # of ATIDs.  The absolute HW max is 16K but we keep it lower.
 */
#define MAX_ATIDS 8192U

union aopen_entry {
	void *data;
	union aopen_entry *next;
};

/*
 * Holds the size, base address, free list start, etc of filter TID.
 * The tables themselves are allocated dynamically.
 */
struct tid_info {
	void **tid_tab;
	unsigned int ntids;
	struct filter_entry *ftid_tab;	/* Normal filters */
	union aopen_entry *atid_tab;
	struct rte_bitmap *ftid_bmap;
	uint8_t *ftid_bmap_array;
	unsigned int nftids, natids;
	unsigned int ftid_base, hash_base;

	union aopen_entry *afree;
	unsigned int atids_in_use;

	/* TIDs in the TCAM */
	u32 tids_in_use;
	/* TIDs in the HASH */
	u32 hash_tids_in_use;
	u32 conns_in_use;

	rte_spinlock_t atid_lock __rte_cache_aligned;
	rte_spinlock_t ftid_lock;
};

static inline void *lookup_tid(const struct tid_info *t, unsigned int tid)
{
	return tid < t->ntids ? t->tid_tab[tid] : NULL;
}

static inline void *lookup_atid(const struct tid_info *t, unsigned int atid)
{
	return atid < t->natids ? t->atid_tab[atid].data : NULL;
}

int cxgbe_alloc_atid(struct tid_info *t, void *data);
void cxgbe_free_atid(struct tid_info *t, unsigned int atid);
void cxgbe_remove_tid(struct tid_info *t, unsigned int qid, unsigned int tid,
		      unsigned short family);
void cxgbe_insert_tid(struct tid_info *t, void *data, unsigned int tid,
		      unsigned short family);

#endif /* _CXGBE_OFLD_H_ */
