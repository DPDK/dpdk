/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_CPR_H_
#define _BNXT_CPR_H_
#include <stdbool.h>

#include <rte_io.h>
#include "hsi_struct_def_dpdk.h"

struct bnxt_db_info;

#define CMP_TYPE(cmp)						\
	(((struct cmpl_base *)cmp)->type & CMPL_BASE_TYPE_MASK)

/* Get completion length from completion type, in 16-byte units. */
#define CMP_LEN(cmp_type) (((cmp_type) & 1) + 1)


#define ADV_RAW_CMP(idx, n)	((idx) + (n))
#define NEXT_RAW_CMP(idx)	ADV_RAW_CMP(idx, 1)
#define RING_CMP(ring, idx)	((idx) & (ring)->ring_mask)
#define RING_CMPL(ring_mask, idx)	((idx) & (ring_mask))
#define NEXT_CMP(idx)		RING_CMP(ADV_RAW_CMP(idx, 1))

#define DB_CP_REARM_FLAGS	(DB_KEY_CP | DB_IDX_VALID)
#define DB_CP_FLAGS		(DB_KEY_CP | DB_IDX_VALID | DB_IRQ_DIS)

#define B_CP_DB_REARM(cpr, raw_cons)					\
	rte_write32((DB_CP_REARM_FLAGS |				\
		    DB_RING_IDX(&((cpr)->cp_db), raw_cons)),		\
		    ((cpr)->cp_db.doorbell))

#define B_CP_DB_ARM(cpr)	rte_write32((DB_KEY_CP),		\
					    ((cpr)->cp_db.doorbell))

#define B_CP_DB_DISARM(cpr)	(*(uint32_t *)((cpr)->cp_db.doorbell) = \
				 DB_KEY_CP | DB_IRQ_DIS)

#define B_CP_DB_IDX_ARM(cpr, cons)					\
		(*(uint32_t *)((cpr)->cp_db.doorbell) = (DB_CP_REARM_FLAGS | \
				(cons)))

#define B_CP_DB_IDX_DISARM(cpr, cons)	do {				\
		rte_smp_wmb();						\
		(*(uint32_t *)((cpr)->cp_db.doorbell) = (DB_CP_FLAGS |	\
				(cons));				\
} while (0)
#define B_CP_DIS_DB(cpr, raw_cons)					\
	rte_write32_relaxed((DB_CP_FLAGS |				\
		    DB_RING_IDX(&((cpr)->cp_db), raw_cons)),		\
		    ((cpr)->cp_db.doorbell))

#define B_CP_DB(cpr, raw_cons, ring_mask)				\
	rte_write32((DB_CP_FLAGS |					\
		    RING_CMPL((ring_mask), raw_cons)),	\
		    ((cpr)->cp_db.doorbell))

struct bnxt_db_info {
	void                    *doorbell;
	union {
		uint64_t        db_key64;
		uint32_t        db_key32;
	};
	bool                    db_64;
	uint32_t		db_ring_mask;
	uint32_t		db_epoch_mask;
	uint32_t		db_epoch_shift;
};

#define DB_EPOCH(db, idx)	(((idx) & (db)->db_epoch_mask) <<	\
				 ((db)->db_epoch_shift))
#define DB_RING_IDX(db, idx)	(((idx) & (db)->db_ring_mask) |		\
				 DB_EPOCH(db, idx))

struct bnxt_ring;
struct bnxt_cp_ring_info {
	uint32_t		cp_raw_cons;

	struct cmpl_base	*cp_desc_ring;
	struct bnxt_db_info     cp_db;
	rte_iova_t		cp_desc_mapping;

	struct ctx_hw_stats	*hw_stats;
	rte_iova_t		hw_stats_map;
	uint32_t		hw_stats_ctx_id;

	struct bnxt_ring	*cp_ring_struct;
};

#define RX_CMP_L2_ERRORS						\
	(RX_PKT_CMPL_ERRORS_BUFFER_ERROR_MASK | RX_PKT_CMPL_ERRORS_CRC_ERROR)

struct bnxt;
void bnxt_handle_async_event(struct bnxt *bp, struct cmpl_base *cmp);
void bnxt_handle_fwd_req(struct bnxt *bp, struct cmpl_base *cmp);
int bnxt_event_hwrm_resp_handler(struct bnxt *bp, struct cmpl_base *cmp);
void bnxt_dev_reset_and_resume(void *arg);
void bnxt_wait_for_device_shutdown(struct bnxt *bp);

#define EVENT_DATA1_REASON_CODE_FW_EXCEPTION_FATAL     \
	HWRM_ASYNC_EVENT_CMPL_RESET_NOTIFY_EVENT_DATA1_REASON_CODE_FW_EXCEPTION_FATAL
#define EVENT_DATA1_REASON_CODE_MASK                   \
	HWRM_ASYNC_EVENT_CMPL_RESET_NOTIFY_EVENT_DATA1_REASON_CODE_MASK

#define EVENT_DATA1_FLAGS_MASK                         \
	HWRM_ASYNC_EVENT_CMPL_ERROR_RECOVERY_EVENT_DATA1_FLAGS_MASK

#define EVENT_DATA1_FLAGS_MASTER_FUNC                  \
	HWRM_ASYNC_EVENT_CMPL_ERROR_RECOVERY_EVENT_DATA1_FLAGS_MASTER_FUNC

#define EVENT_DATA1_FLAGS_RECOVERY_ENABLED             \
	HWRM_ASYNC_EVENT_CMPL_ERROR_RECOVERY_EVENT_DATA1_FLAGS_RECOVERY_ENABLED

bool bnxt_is_recovery_enabled(struct bnxt *bp);
bool bnxt_is_primary_func(struct bnxt *bp);

void bnxt_stop_rxtx(struct bnxt *bp);

/**
 * Check validity of a completion ring entry. If the entry is valid, include a
 * C11 __ATOMIC_ACQUIRE fence to ensure that subsequent loads of fields in the
 * completion are not hoisted by the compiler or by the CPU to come before the
 * loading of the "valid" field.
 *
 * Note: the caller must not access any fields in the specified completion
 * entry prior to calling this function.
 *
 * @param cmpl
 *   Pointer to an entry in the completion ring.
 * @param raw_cons
 *   Raw consumer index of entry in completion ring.
 * @param ring_size
 *   Size of completion ring.
 */
static __rte_always_inline bool
bnxt_cpr_cmp_valid(const void *cmpl, uint32_t raw_cons, uint32_t ring_size)
{
	const struct cmpl_base *c = cmpl;
	bool expected, valid;

	expected = !(raw_cons & ring_size);
	valid = !!(rte_le_to_cpu_32(c->info3_v) & CMPL_BASE_V);
	if (valid == expected) {
		rte_atomic_thread_fence(__ATOMIC_ACQUIRE);
		return true;
	}
	return false;
}
#endif
