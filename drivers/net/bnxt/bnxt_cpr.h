/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_CPR_H_
#define _BNXT_CPR_H_
#include <stdbool.h>

#include <rte_io.h>

#define CMP_VALID(cmp, raw_cons, ring)					\
	(!!(((struct cmpl_base *)(cmp))->info3_v & CMPL_BASE_V) ==	\
	 !((raw_cons) & ((ring)->ring_size)))

#define CMPL_VALID(cmp, v)						\
	(!!(((struct cmpl_base *)(cmp))->info3_v & CMPL_BASE_V) == !(v))

#define CMP_TYPE(cmp)						\
	(((struct cmpl_base *)cmp)->type & CMPL_BASE_TYPE_MASK)

#define ADV_RAW_CMP(idx, n)	((idx) + (n))
#define NEXT_RAW_CMP(idx)	ADV_RAW_CMP(idx, 1)
#define RING_CMP(ring, idx)	((idx) & (ring)->ring_mask)
#define RING_CMPL(ring_mask, idx)	((idx) & (ring_mask))
#define NEXT_CMP(idx)		RING_CMP(ADV_RAW_CMP(idx, 1))
#define FLIP_VALID(cons, mask, val)	((cons) >= (mask) ? !(val) : (val))

#define DB_CP_REARM_FLAGS	(DB_KEY_CP | DB_IDX_VALID)
#define DB_CP_FLAGS		(DB_KEY_CP | DB_IDX_VALID | DB_IRQ_DIS)

#define NEXT_CMPL(cpr, idx, v, inc)	do { \
	(idx) += (inc); \
	if (unlikely((idx) == (cpr)->cp_ring_struct->ring_size)) { \
		(v) = !(v); \
		(idx) = 0; \
	} \
} while (0)
#define B_CP_DB_REARM(cpr, raw_cons)					\
	rte_write32((DB_CP_REARM_FLAGS |				\
		    RING_CMP(((cpr)->cp_ring_struct), raw_cons)),	\
		    ((cpr)->cp_doorbell))

#define B_CP_DB_ARM(cpr)	rte_write32((DB_KEY_CP), ((cpr)->cp_doorbell))
#define B_CP_DB_DISARM(cpr)	(*(uint32_t *)((cpr)->cp_doorbell) = \
				 DB_KEY_CP | DB_IRQ_DIS)

#define B_CP_DB_IDX_ARM(cpr, cons)					\
		(*(uint32_t *)((cpr)->cp_doorbell) = (DB_CP_REARM_FLAGS | \
				(cons)))

#define B_CP_DB_IDX_DISARM(cpr, cons)	do {				\
		rte_smp_wmb();						\
		(*(uint32_t *)((cpr)->cp_doorbell) = (DB_CP_FLAGS |	\
				(cons));				\
} while (0)
#define B_CP_DIS_DB(cpr, raw_cons)					\
	rte_write32((DB_CP_FLAGS |					\
		    RING_CMP(((cpr)->cp_ring_struct), raw_cons)),	\
		    ((cpr)->cp_doorbell))
#define B_CP_DB(cpr, raw_cons, ring_mask)				\
	rte_write32((DB_CP_FLAGS |					\
		    RING_CMPL((ring_mask), raw_cons)),	\
		    ((cpr)->cp_doorbell))

struct bnxt_ring;
struct bnxt_cp_ring_info {
	uint32_t		cp_raw_cons;
	void			*cp_doorbell;

	struct cmpl_base	*cp_desc_ring;

	rte_iova_t		cp_desc_mapping;

	struct ctx_hw_stats	*hw_stats;
	rte_iova_t		hw_stats_map;
	uint32_t		hw_stats_ctx_id;

	struct bnxt_ring	*cp_ring_struct;
	uint16_t		cp_cons;
	bool			valid;
};

#define RX_CMP_L2_ERRORS						\
	(RX_PKT_CMPL_ERRORS_BUFFER_ERROR_MASK | RX_PKT_CMPL_ERRORS_CRC_ERROR)

struct bnxt;
void bnxt_handle_async_event(struct bnxt *bp, struct cmpl_base *cmp);
void bnxt_handle_fwd_req(struct bnxt *bp, struct cmpl_base *cmp);
int bnxt_event_hwrm_resp_handler(struct bnxt *bp, struct cmpl_base *cmp);

#endif
