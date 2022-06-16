/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <stdint.h>
#include <stdbool.h>

#include "dlb2_priv.h"
#include "dlb2_iface.h"
#include "dlb2_inline_fns.h"

/*
 * This source file is only used when the compiler on the build machine
 * does not support AVX512VL.
 */

static uint8_t cmd_byte_map[DLB2_NUM_PORT_TYPES][DLB2_NUM_HW_SCHED_TYPES] = {
	{
		/* Load-balanced cmd bytes */
		[RTE_EVENT_OP_NEW] = DLB2_NEW_CMD_BYTE,
		[RTE_EVENT_OP_FORWARD] = DLB2_FWD_CMD_BYTE,
		[RTE_EVENT_OP_RELEASE] = DLB2_COMP_CMD_BYTE,
	},
	{
		/* Directed cmd bytes */
		[RTE_EVENT_OP_NEW] = DLB2_NEW_CMD_BYTE,
		[RTE_EVENT_OP_FORWARD] = DLB2_NEW_CMD_BYTE,
		[RTE_EVENT_OP_RELEASE] = DLB2_NOOP_CMD_BYTE,
	},
};

void
dlb2_event_build_hcws(struct dlb2_port *qm_port,
		      const struct rte_event ev[],
		      int num,
		      uint8_t *sched_type,
		      uint8_t *queue_id)
{
	struct dlb2_enqueue_qe *qe;
	uint16_t sched_word[4];
	__m128i sse_qe[2];
	int i;

	qe = qm_port->qe4;

	sse_qe[0] = _mm_setzero_si128();
	sse_qe[1] = _mm_setzero_si128();

	switch (num) {
	case 4:
		/* Construct the metadata portion of two HCWs in one 128b SSE
		 * register. HCW metadata is constructed in the SSE registers
		 * like so:
		 * sse_qe[0][63:0]:   qe[0]'s metadata
		 * sse_qe[0][127:64]: qe[1]'s metadata
		 * sse_qe[1][63:0]:   qe[2]'s metadata
		 * sse_qe[1][127:64]: qe[3]'s metadata
		 */

		/* Convert the event operation into a command byte and store it
		 * in the metadata:
		 * sse_qe[0][63:56]   = cmd_byte_map[is_directed][ev[0].op]
		 * sse_qe[0][127:120] = cmd_byte_map[is_directed][ev[1].op]
		 * sse_qe[1][63:56]   = cmd_byte_map[is_directed][ev[2].op]
		 * sse_qe[1][127:120] = cmd_byte_map[is_directed][ev[3].op]
		 */
#define DLB2_QE_CMD_BYTE 7
		sse_qe[0] = _mm_insert_epi8(sse_qe[0],
				cmd_byte_map[qm_port->is_directed][ev[0].op],
				DLB2_QE_CMD_BYTE);
		sse_qe[0] = _mm_insert_epi8(sse_qe[0],
				cmd_byte_map[qm_port->is_directed][ev[1].op],
				DLB2_QE_CMD_BYTE + 8);
		sse_qe[1] = _mm_insert_epi8(sse_qe[1],
				cmd_byte_map[qm_port->is_directed][ev[2].op],
				DLB2_QE_CMD_BYTE);
		sse_qe[1] = _mm_insert_epi8(sse_qe[1],
				cmd_byte_map[qm_port->is_directed][ev[3].op],
				DLB2_QE_CMD_BYTE + 8);

		/* Store priority, scheduling type, and queue ID in the sched
		 * word array because these values are re-used when the
		 * destination is a directed queue.
		 */
		sched_word[0] = EV_TO_DLB2_PRIO(ev[0].priority) << 10 |
				sched_type[0] << 8 |
				queue_id[0];
		sched_word[1] = EV_TO_DLB2_PRIO(ev[1].priority) << 10 |
				sched_type[1] << 8 |
				queue_id[1];
		sched_word[2] = EV_TO_DLB2_PRIO(ev[2].priority) << 10 |
				sched_type[2] << 8 |
				queue_id[2];
		sched_word[3] = EV_TO_DLB2_PRIO(ev[3].priority) << 10 |
				sched_type[3] << 8 |
				queue_id[3];

		/* Store the event priority, scheduling type, and queue ID in
		 * the metadata:
		 * sse_qe[0][31:16] = sched_word[0]
		 * sse_qe[0][95:80] = sched_word[1]
		 * sse_qe[1][31:16] = sched_word[2]
		 * sse_qe[1][95:80] = sched_word[3]
		 */
#define DLB2_QE_QID_SCHED_WORD 1
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     sched_word[0],
					     DLB2_QE_QID_SCHED_WORD);
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     sched_word[1],
					     DLB2_QE_QID_SCHED_WORD + 4);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     sched_word[2],
					     DLB2_QE_QID_SCHED_WORD);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     sched_word[3],
					     DLB2_QE_QID_SCHED_WORD + 4);

		/* If the destination is a load-balanced queue, store the lock
		 * ID. If it is a directed queue, DLB places this field in
		 * bytes 10-11 of the received QE, so we format it accordingly:
		 * sse_qe[0][47:32]  = dir queue ? sched_word[0] : flow_id[0]
		 * sse_qe[0][111:96] = dir queue ? sched_word[1] : flow_id[1]
		 * sse_qe[1][47:32]  = dir queue ? sched_word[2] : flow_id[2]
		 * sse_qe[1][111:96] = dir queue ? sched_word[3] : flow_id[3]
		 */
#define DLB2_QE_LOCK_ID_WORD 2
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
				(sched_type[0] == DLB2_SCHED_DIRECTED) ?
					sched_word[0] : ev[0].flow_id,
				DLB2_QE_LOCK_ID_WORD);
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
				(sched_type[1] == DLB2_SCHED_DIRECTED) ?
					sched_word[1] : ev[1].flow_id,
				DLB2_QE_LOCK_ID_WORD + 4);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
				(sched_type[2] == DLB2_SCHED_DIRECTED) ?
					sched_word[2] : ev[2].flow_id,
				DLB2_QE_LOCK_ID_WORD);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
				(sched_type[3] == DLB2_SCHED_DIRECTED) ?
					sched_word[3] : ev[3].flow_id,
				DLB2_QE_LOCK_ID_WORD + 4);

		/* Store the event type and sub event type in the metadata:
		 * sse_qe[0][15:0]  = flow_id[0]
		 * sse_qe[0][79:64] = flow_id[1]
		 * sse_qe[1][15:0]  = flow_id[2]
		 * sse_qe[1][79:64] = flow_id[3]
		 */
#define DLB2_QE_EV_TYPE_WORD 0
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     ev[0].sub_event_type << 8 |
						ev[0].event_type,
					     DLB2_QE_EV_TYPE_WORD);
		sse_qe[0] = _mm_insert_epi16(sse_qe[0],
					     ev[1].sub_event_type << 8 |
						ev[1].event_type,
					     DLB2_QE_EV_TYPE_WORD + 4);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     ev[2].sub_event_type << 8 |
						ev[2].event_type,
					     DLB2_QE_EV_TYPE_WORD);
		sse_qe[1] = _mm_insert_epi16(sse_qe[1],
					     ev[3].sub_event_type << 8 |
						ev[3].event_type,
					     DLB2_QE_EV_TYPE_WORD + 4);

		/*
		 * Store the metadata to memory (use the double-precision
		 * _mm_storeh_pd because there is no integer function for
		 * storing the upper 64b):
		 * qe[0] metadata = sse_qe[0][63:0]
		 * qe[1] metadata = sse_qe[0][127:64]
		 * qe[2] metadata = sse_qe[1][63:0]
		 * qe[3] metadata = sse_qe[1][127:64]
		 */
		_mm_storel_epi64((__m128i *)&qe[0].u.opaque_data,
				 sse_qe[0]);
		_mm_storeh_pd((double *)&qe[1].u.opaque_data,
			      (__m128d)sse_qe[0]);
		_mm_storel_epi64((__m128i *)&qe[2].u.opaque_data,
				 sse_qe[1]);
		_mm_storeh_pd((double *)&qe[3].u.opaque_data,
				      (__m128d)sse_qe[1]);

		qe[0].data = ev[0].u64;
		qe[1].data = ev[1].u64;
		qe[2].data = ev[2].u64;
		qe[3].data = ev[3].u64;

		/* will only be set for DLB 2.5 + */
		if (qm_port->cq_weight) {
			qe[0].weight = ev[0].impl_opaque & 3;
			qe[1].weight = ev[1].impl_opaque & 3;
			qe[2].weight = ev[2].impl_opaque & 3;
			qe[3].weight = ev[3].impl_opaque & 3;
		}

		break;
	case 3:
	case 2:
	case 1:
		for (i = 0; i < num; i++) {
			qe[i].cmd_byte =
				cmd_byte_map[qm_port->is_directed][ev[i].op];
			qe[i].sched_type = sched_type[i];
			qe[i].data = ev[i].u64;
			qe[i].qid = queue_id[i];
			qe[i].priority = EV_TO_DLB2_PRIO(ev[i].priority);
			qe[i].lock_id = ev[i].flow_id;
			if (sched_type[i] == DLB2_SCHED_DIRECTED) {
				struct dlb2_msg_info *info =
					(struct dlb2_msg_info *)&qe[i].lock_id;

				info->qid = queue_id[i];
				info->sched_type = DLB2_SCHED_DIRECTED;
				info->priority = qe[i].priority;
			}
			qe[i].u.event_type.major = ev[i].event_type;
			qe[i].u.event_type.sub = ev[i].sub_event_type;
		}
		break;
	case 0:
		break;
	}
}
