/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2020 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_MPC_H_
#define _BNXT_MPC_H_

#include <inttypes.h>
#include <stdbool.h>
#include <rte_malloc.h>

/* MPC Batch support */
extern bool bnxt_tfc_mpc_batch;
extern uint8_t bnxt_mpc_batch_count;

#define BNXT_MPC_RX_RETRY    100000

#define BNXT_MPC_NB_DESC		128
#define BNXT_MPC_DESC_THRESH		3
#define BNXT_MPC_CHNL_SHIFT		16
#define BNXT_MPC_QIDX_MSK		0xFFFF
#define BNXT_MPC_CHNL(x)		((x) >> BNXT_MPC_CHNL_SHIFT)
#define BNXT_MPC_QIDX(x)		((x) & BNXT_MPC_QIDX_MSK)
#define BNXT_MPC_MAP_INDEX(x, y)	(((x) << BNXT_MPC_CHNL_SHIFT) | (y))

#define BNXT_MPC_CHNLS_SUPPORTED	2 /* Limit to MPC TE_CFA and RE_CFA */

/* BNXT_MPC_RINGS_SUPPORTED set to 1 TE_CFA and 1 1 RE_CFA types.
 * Can be set up to tx_nr_rings * BNXT_MPC_CHNLS_SUPPORTED if needed.
 */
#define BNXT_MPC_RINGS_SUPPORTED	(1 * BNXT_MPC_CHNLS_SUPPORTED)

/* Defines the number of msgs there are in an MPC msg completion event.
 * Used to pass an opaque value into the MPC msg xmit function. The
 * completion processing uses this value to ring the doorbell correctly to
 * signal "completion event processing complete" to the hardware.
 */
#define BNXT_MPC_COMP_MSG_COUNT		1

/* Defines the uS delay prior to processing an MPC completion */
#define BNXT_MPC_RX_US_DELAY 1

enum bnxt_mpc_chnl {
	BNXT_MPC_CHNL_TCE = 0,
	BNXT_MPC_CHNL_RCE = 1,
	BNXT_MPC_CHNL_TE_CFA = 2,
	BNXT_MPC_CHNL_RE_CFA = 3,
	BNXT_MPC_CHNL_PRIMATE = 4,
	BNXT_MPC_CHNL_MAX = 5,
};

struct bnxt_sw_mpc_bd {
	struct bnxt_mpc_mbuf	*mpc_mbuf; /* mpc mbuf associated with mpc bd */
	unsigned short		nr_bds;
};

struct bnxt_mpc_ring_info {
	uint16_t		raw_prod;
	uint16_t		raw_cons;
	struct bnxt_db_info     db;

	struct tx_bd_mp_cmd	*mpc_desc_ring;
	struct bnxt_sw_mpc_bd	*mpc_buf_ring;

	rte_iova_t		mpc_desc_mapping;

	uint32_t		dev_state;

	struct bnxt_ring	*mpc_ring_struct;
	uint32_t                epoch;
};

struct bnxt_mpc_mbuf {
	enum bnxt_mpc_chnl	chnl_id;
	uint8_t                 cmp_type;
	uint8_t			*msg_data;
	/* MPC msg size in bytes, must be multiple of 16Bytes */
	uint16_t		msg_size;
};

struct bnxt_mpc_txq {
	enum		bnxt_mpc_chnl chnl_id;
	uint32_t	queue_idx;
	uint16_t	nb_mpc_desc; /* number of MPC descriptors */
	uint16_t	free_thresh;/* minimum mpc cmds before freeing */
	int		wake_thresh;
	uint8_t		started; /* MPC queue is started */

	struct bnxt	*bp;
	struct bnxt_mpc_ring_info	*mpc_ring;
	unsigned int	cp_nr_rings;
	struct bnxt_cp_ring_info	*cp_ring;
	const struct rte_memzone *mz;
	struct bnxt_mpc_mbuf **free;
	uint16_t	seq_num;

	void (*cmpl_handler_cb)(struct bnxt_mpc_txq *mpc_queue,
				uint32_t nb_mpc_cmds);
};

struct bnxt_mpc {
	uint8_t			mpc_chnls_cap;
	uint8_t			mpc_chnls_en;
	struct bnxt_mpc_txq	*mpc_txq[BNXT_MPC_CHNL_MAX];
};

int bnxt_mpc_open(struct bnxt *bp);
int bnxt_mpc_close(struct bnxt *bp);
int bnxt_mpc_send(struct bnxt *bp,
		  struct bnxt_mpc_mbuf *in_msg,
		  struct bnxt_mpc_mbuf *out_msg,
		  uint16_t *opaque,
		  bool batch);
int bnxt_mpc_cmd_cmpl(struct bnxt_mpc_txq *mpc_queue,
		      struct bnxt_mpc_mbuf *out_msg,
		      uint16_t *opaque);
int bnxt_mpc_poll_cmd_cmpls(struct bnxt_mpc_txq *mpc_queue);

#endif
