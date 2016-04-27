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
 * @brief ecore_sp_vf_start -  VF Function Start
 *
 * This ramrod is sent to initialize a virtual function (VF) is loaded.
 * It will configure the function related parameters.
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 * @param concrete_vfid				VF ID
 * @param opaque_vfid
 *
 * @return enum _ecore_status_t
 */

enum _ecore_status_t ecore_sp_vf_start(struct ecore_hwfn *p_hwfn,
				       u32 concrete_vfid, u16 opaque_vfid);

/**
 * @brief ecore_sp_vf_update - VF Function Update Ramrod
 *
 * This ramrod performs updates of a virtual function (VF).
 * It currently contains no functionality.
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */

enum _ecore_status_t ecore_sp_vf_update(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_sp_vf_stop - VF Function Stop Ramrod
 *
 * This ramrod is sent to unload a virtual function (VF).
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 * @param concrete_vfid
 * @param opaque_vfid
 *
 * @return enum _ecore_status_t
 */

enum _ecore_status_t ecore_sp_vf_stop(struct ecore_hwfn *p_hwfn,
				      u32 concrete_vfid, u16 opaque_vfid);

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
 * @param rx_queue_id
 * @param vport_id
 * @param stats_id
 * @param sb
 * @param sb_index
 * @param bd_max_bytes
 * @param bd_chain_phys_addr
 * @param cqe_pbl_addr
 * @param cqe_pbl_size
 * @param leading
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_eth_rxq_start_ramrod(struct ecore_hwfn *p_hwfn,
			      u16 opaque_fid,
			      u32 cid,
			      u16 rx_queue_id,
			      u8 vport_id,
			      u8 stats_id,
			      u16 sb,
			      u8 sb_index,
			      u16 bd_max_bytes,
			      dma_addr_t bd_chain_phys_addr,
			      dma_addr_t cqe_pbl_addr, u16 cqe_pbl_size);

/**
 * @brief - Starts a Tx queue; Should be used where contexts are handled
 * outside of the ramrod area [specifically iov scenarios]
 *
 * @param p_hwfn
 * @param opaque_fid
 * @param tx_queue_id
 * @param cid
 * @param vport_id
 * @param stats_id
 * @param sb
 * @param sb_index
 * @param pbl_addr
 * @param pbl_size
 * @param p_pq_params - parameters for choosing the PQ for this Tx queue
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_eth_txq_start_ramrod(struct ecore_hwfn *p_hwfn,
			      u16 opaque_fid,
			      u16 tx_queue_id,
			      u32 cid,
			      u8 vport_id,
			      u8 stats_id,
			      u16 sb,
			      u8 sb_index,
			      dma_addr_t pbl_addr,
			      u16 pbl_size,
			      union ecore_qm_pq_params *p_pq_params);

u8 ecore_mcast_bin_from_mac(u8 *mac);

#endif
