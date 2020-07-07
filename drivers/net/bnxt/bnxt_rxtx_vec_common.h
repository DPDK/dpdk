/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_RXTX_VEC_COMMON_H_
#define _BNXT_RXTX_VEC_COMMON_H_

#define RTE_BNXT_MAX_RX_BURST		32
#define RTE_BNXT_MAX_TX_BURST		32
#define RTE_BNXT_RXQ_REARM_THRESH	32
#define RTE_BNXT_DESCS_PER_LOOP		4

#define TX_BD_FLAGS_CMPL ((1 << TX_BD_LONG_FLAGS_BD_CNT_SFT) | \
			  TX_BD_SHORT_FLAGS_COAL_NOW | \
			  TX_BD_SHORT_TYPE_TX_BD_SHORT | \
			  TX_BD_LONG_FLAGS_PACKET_END)

#define TX_BD_FLAGS_NOCMPL (TX_BD_FLAGS_CMPL | TX_BD_LONG_FLAGS_NO_CMPL)

static inline uint32_t
bnxt_xmit_flags_len(uint16_t len, uint16_t flags)
{
	switch (len >> 9) {
	case 0:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT512;
	case 1:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT1K;
	case 2:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT2K;
	case 3:
		return flags | TX_BD_LONG_FLAGS_LHINT_LT2K;
	default:
		return flags | TX_BD_LONG_FLAGS_LHINT_GTE2K;
	}
}

static inline int
bnxt_rxq_vec_setup_common(struct bnxt_rx_queue *rxq)
{
	uintptr_t p;
	struct rte_mbuf mb_def = { .buf_addr = 0 }; /* zeroed mbuf */

	mb_def.nb_segs = 1;
	mb_def.data_off = RTE_PKTMBUF_HEADROOM;
	mb_def.port = rxq->port_id;
	rte_mbuf_refcnt_set(&mb_def, 1);

	/* prevent compiler reordering: rearm_data covers previous fields */
	rte_compiler_barrier();
	p = (uintptr_t)&mb_def.rearm_data;
	rxq->mbuf_initializer = *(uint64_t *)p;
	rxq->rxrearm_nb = 0;
	rxq->rxrearm_start = 0;
	return 0;
}
#endif /* _BNXT_RXTX_VEC_COMMON_H_ */
