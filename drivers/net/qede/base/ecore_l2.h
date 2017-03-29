/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_L2_H__
#define __ECORE_L2_H__


#include "ecore.h"
#include "ecore_hw.h"
#include "ecore_spq.h"
#include "ecore_l2_api.h"

struct ecore_queue_cid {
	/* 'Relative' is a relative term ;-). Usually the indices [not counting
	 * SBs] would be PF-relative, but there are some cases where that isn't
	 * the case - specifically for a PF configuring its VF indices it's
	 * possible some fields [E.g., stats-id] in 'rel' would already be abs.
	 */
	struct ecore_queue_start_common_params rel;
	struct ecore_queue_start_common_params abs;
	u32 cid;
	u16 opaque_fid;

	/* VFs queues are mapped differently, so we need to know the
	 * relative queue associated with them [0-based].
	 * Notice this is relevant on the *PF* queue-cid of its VF's queues,
	 * and not on the VF itself.
	 */
	bool is_vf;
	u8 vf_qid;

	/* Legacy VFs might have Rx producer located elsewhere */
	bool b_legacy_vf;

	struct ecore_hwfn *p_owner;
};

void ecore_eth_queue_cid_release(struct ecore_hwfn *p_hwfn,
				 struct ecore_queue_cid *p_cid);

struct ecore_queue_cid *
_ecore_eth_queue_to_cid(struct ecore_hwfn *p_hwfn,
			u16 opaque_fid, u32 cid, u8 vf_qid,
			struct ecore_queue_start_common_params *p_params);

enum _ecore_status_t
ecore_sp_eth_vport_start(struct ecore_hwfn *p_hwfn,
			 struct ecore_sp_vport_start_params *p_params);

/**
 * @brief - Starts an Rx queue, when queue_cid is already prepared
 *
 * @param p_hwfn
 * @param p_cid
 * @param bd_max_bytes
 * @param bd_chain_phys_addr
 * @param cqe_pbl_addr
 * @param cqe_pbl_size
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_eth_rxq_start_ramrod(struct ecore_hwfn *p_hwfn,
			   struct ecore_queue_cid *p_cid,
			   u16 bd_max_bytes,
			   dma_addr_t bd_chain_phys_addr,
			   dma_addr_t cqe_pbl_addr,
			   u16 cqe_pbl_size);

/**
 * @brief - Starts a Tx queue, where queue_cid is already prepared
 *
 * @param p_hwfn
 * @param p_cid
 * @param pbl_addr
 * @param pbl_size
 * @param p_pq_params - parameters for choosing the PQ for this Tx queue
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_eth_txq_start_ramrod(struct ecore_hwfn *p_hwfn,
			   struct ecore_queue_cid *p_cid,
			   dma_addr_t pbl_addr, u16 pbl_size,
			   u16 pq_id);

u8 ecore_mcast_bin_from_mac(u8 *mac);

#endif
