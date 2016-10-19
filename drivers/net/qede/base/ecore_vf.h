/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_VF_H__
#define __ECORE_VF_H__

#include "ecore_status.h"
#include "ecore_vf_api.h"
#include "ecore_l2_api.h"
#include "ecore_vfpf_if.h"

/* This data is held in the ecore_hwfn structure for VFs only. */
struct ecore_vf_iov {
	union vfpf_tlvs			*vf2pf_request;
	dma_addr_t			vf2pf_request_phys;
	union pfvf_tlvs			*pf2vf_reply;
	dma_addr_t			pf2vf_reply_phys;

	/* Should be taken whenever the mailbox buffers are accessed */
	osal_mutex_t			mutex;
	u8				*offset;

	/* Bulletin Board */
	struct ecore_bulletin		bulletin;
	struct ecore_bulletin_content	bulletin_shadow;

	/* we set aside a copy of the acquire response */
	struct pfvf_acquire_resp_tlv	acquire_resp;

	/* In case PF originates prior to the fp-hsi version comparison,
	 * this has to be propagated as it affects the fastpath.
	 */
	bool b_pre_fp_hsi;
};

#ifdef CONFIG_ECORE_SRIOV
/**
 * @brief hw preparation for VF
 * sends ACQUIRE message
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_vf_hw_prepare(struct ecore_hwfn *p_hwfn);

/**
 * @brief VF - start the RX Queue by sending a message to the PF
 *
 * @param p_hwfn
 * @param cid			- zero based within the VF
 * @param rx_queue_id		- zero based within the VF
 * @param sb			- VF status block for this queue
 * @param sb_index		- Index within the status block
 * @param bd_max_bytes		- maximum number of bytes per bd
 * @param bd_chain_phys_addr	- physical address of bd chain
 * @param cqe_pbl_addr		- physical address of pbl
 * @param cqe_pbl_size		- pbl size
 * @param pp_prod		- pointer to the producer to be
 *				  used in fasthpath
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_vf_pf_rxq_start(struct ecore_hwfn *p_hwfn,
					   u8 rx_queue_id,
					   u16 sb,
					   u8 sb_index,
					   u16 bd_max_bytes,
					   dma_addr_t bd_chain_phys_addr,
					   dma_addr_t cqe_pbl_addr,
					   u16 cqe_pbl_size,
					   void OSAL_IOMEM **pp_prod);

/**
 * @brief VF - start the TX queue by sending a message to the
 *        PF.
 *
 * @param p_hwfn
 * @param tx_queue_id		- zero based within the VF
 * @param sb			- status block for this queue
 * @param sb_index		- index within the status block
 * @param bd_chain_phys_addr	- physical address of tx chain
 * @param pp_doorbell		- pointer to address to which to
 *				write the doorbell too..
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_vf_pf_txq_start(struct ecore_hwfn *p_hwfn,
					   u16 tx_queue_id,
					   u16 sb,
					   u8 sb_index,
					   dma_addr_t pbl_addr,
					   u16 pbl_size,
					   void OSAL_IOMEM **pp_doorbell);

/**
 * @brief VF - stop the RX queue by sending a message to the PF
 *
 * @param p_hwfn
 * @param rx_qid
 * @param cqe_completion
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_vf_pf_rxq_stop(struct ecore_hwfn	*p_hwfn,
					  u16			rx_qid,
					  bool			cqe_completion);

/**
 * @brief VF - stop the TX queue by sending a message to the PF
 *
 * @param p_hwfn
 * @param tx_qid
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_vf_pf_txq_stop(struct ecore_hwfn	*p_hwfn,
					  u16			tx_qid);

#ifndef LINUX_REMOVE
/**
 * @brief VF - update the RX queue by sending a message to the
 *        PF
 *
 * @param p_hwfn
 * @param rx_queue_id
 * @param num_rxqs
 * @param init_sge_ring
 * @param comp_cqe_flg
 * @param comp_event_flg
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_vf_pf_rxqs_update(
			struct ecore_hwfn	*p_hwfn,
			u16			rx_queue_id,
			u8			num_rxqs,
			u8			comp_cqe_flg,
			u8			comp_event_flg);
#endif

/**
 * @brief VF - send a vport update command
 *
 * @param p_hwfn
 * @param params
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_vf_pf_vport_update(struct ecore_hwfn *p_hwfn,
			 struct ecore_sp_vport_update_params *p_params);

/**
 * @brief VF - send a close message to PF
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status
 */
enum _ecore_status_t ecore_vf_pf_reset(struct ecore_hwfn *p_hwfn);

/**
 * @brief VF - free vf`s memories
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status
 */
enum _ecore_status_t ecore_vf_pf_release(struct ecore_hwfn *p_hwfn);

/**
 * @brief ecore_vf_get_igu_sb_id - Get the IGU SB ID for a given
 *        sb_id. For VFs igu sbs don't have to be contiguous
 *
 * @param p_hwfn
 * @param sb_id
 *
 * @return INLINE u16
 */
u16 ecore_vf_get_igu_sb_id(struct ecore_hwfn *p_hwfn,
			   u16               sb_id);


/**
 * @brief ecore_vf_pf_vport_start - perform vport start for VF.
 *
 * @param p_hwfn
 * @param vport_id
 * @param mtu
 * @param inner_vlan_removal
 * @param tpa_mode
 * @param max_buffers_per_cqe,
 * @param only_untagged - default behavior regarding vlan acceptance
 *
 * @return enum _ecore_status
 */
enum _ecore_status_t ecore_vf_pf_vport_start(
			struct ecore_hwfn *p_hwfn,
			u8 vport_id,
			u16 mtu,
			u8 inner_vlan_removal,
			enum ecore_tpa_mode tpa_mode,
			u8 max_buffers_per_cqe,
			u8 only_untagged);

/**
 * @brief ecore_vf_pf_vport_stop - stop the VF's vport
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status
 */
enum _ecore_status_t ecore_vf_pf_vport_stop(struct ecore_hwfn *p_hwfn);

enum _ecore_status_t ecore_vf_pf_filter_ucast(
			struct ecore_hwfn *p_hwfn,
			struct ecore_filter_ucast *p_param);

void ecore_vf_pf_filter_mcast(struct ecore_hwfn *p_hwfn,
			      struct ecore_filter_mcast *p_filter_cmd);

/**
 * @brief ecore_vf_pf_int_cleanup - clean the SB of the VF
 *
 * @param p_hwfn
 *
 * @return enum _ecore_status
 */
enum _ecore_status_t ecore_vf_pf_int_cleanup(struct ecore_hwfn *p_hwfn);

/**
 * @brief - return the link params in a given bulletin board
 *
 * @param p_hwfn
 * @param p_params - pointer to a struct to fill with link params
 * @param p_bulletin
 */
void __ecore_vf_get_link_params(struct ecore_hwfn *p_hwfn,
				struct ecore_mcp_link_params *p_params,
				struct ecore_bulletin_content *p_bulletin);

/**
 * @brief - return the link state in a given bulletin board
 *
 * @param p_hwfn
 * @param p_link - pointer to a struct to fill with link state
 * @param p_bulletin
 */
void __ecore_vf_get_link_state(struct ecore_hwfn *p_hwfn,
			       struct ecore_mcp_link_state *p_link,
			       struct ecore_bulletin_content *p_bulletin);

/**
 * @brief - return the link capabilities in a given bulletin board
 *
 * @param p_hwfn
 * @param p_link - pointer to a struct to fill with link capabilities
 * @param p_bulletin
 */
void __ecore_vf_get_link_caps(struct ecore_hwfn *p_hwfn,
			      struct ecore_mcp_link_capabilities *p_link_caps,
			      struct ecore_bulletin_content *p_bulletin);

#endif
#endif /* __ECORE_VF_H__ */
