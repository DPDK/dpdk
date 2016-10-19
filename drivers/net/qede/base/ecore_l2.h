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

/**
 * @brief ecore_sp_eth_tx_queue_update -
 *
 * This ramrod updates a TX queue. It is used for setting the active
 * state of the queue.
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_sp_eth_tx_queue_update(struct ecore_hwfn *p_hwfn);

enum _ecore_status_t
ecore_sp_eth_vport_start(struct ecore_hwfn *p_hwfn,
			 struct ecore_sp_vport_start_params *p_params);

/**
 * @brief - Starts an Rx queue; Should be used where contexts are handled
 * outside of the ramrod area [specifically iov scenarios]
 *
 * @param p_hwfn
 * @param opaque_fid
 * @param cid
 * @param p_params [queue_id, vport_id, stats_id, sb, sb_idx, vf_qid]
	  stats_id is absolute packed in p_params.
 * @param bd_max_bytes
 * @param bd_chain_phys_addr
 * @param cqe_pbl_addr
 * @param cqe_pbl_size
 * @param b_use_zone_a_prod - support legacy VF producers
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_eth_rxq_start_ramrod(struct ecore_hwfn	*p_hwfn,
			      u16 opaque_fid,
			      u32 cid,
			      struct ecore_queue_start_common_params *p_params,
			      u16 bd_max_bytes,
			      dma_addr_t bd_chain_phys_addr,
			      dma_addr_t cqe_pbl_addr,
			      u16 cqe_pbl_size, bool b_use_zone_a_prod);

/**
 * @brief - Starts a Tx queue; Should be used where contexts are handled
 * outside of the ramrod area [specifically iov scenarios]
 *
 * @param p_hwfn
 * @param opaque_fid
 * @param cid
 * @param p_params [queue_id, vport_id,stats_id, sb, sb_idx, vf_qid]
 * @param pbl_addr
 * @param pbl_size
 * @param p_pq_params - parameters for choosing the PQ for this Tx queue
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_eth_txq_start_ramrod(struct ecore_hwfn	*p_hwfn,
			      u16 opaque_fid,
			      u32 cid,
			      struct ecore_queue_start_common_params *p_params,
			      dma_addr_t pbl_addr,
			      u16 pbl_size,
			      union ecore_qm_pq_params *p_pq_params);

u8 ecore_mcast_bin_from_mac(u8 *mac);

#endif
