/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_L2_API_H__
#define __ECORE_L2_API_H__

#include "ecore_status.h"
#include "ecore_sp_api.h"

#ifndef __EXTRACT__LINUX__
enum ecore_rss_caps {
	ECORE_RSS_IPV4		= 0x1,
	ECORE_RSS_IPV6		= 0x2,
	ECORE_RSS_IPV4_TCP	= 0x4,
	ECORE_RSS_IPV6_TCP	= 0x8,
	ECORE_RSS_IPV4_UDP	= 0x10,
	ECORE_RSS_IPV6_UDP	= 0x20,
};

/* Should be the same as ETH_RSS_IND_TABLE_ENTRIES_NUM */
#define ECORE_RSS_IND_TABLE_SIZE 128
#define ECORE_RSS_KEY_SIZE 10 /* size in 32b chunks */
#endif

struct ecore_queue_start_common_params {
	/* Rx/Tx queue id */
	u8 queue_id;
	u8 vport_id;

	/* stats_id is relative or absolute depends on function */
	u8 stats_id;
	u16 sb;
	u16 sb_idx;
	u16 vf_qid;
};

struct ecore_rss_params {
	u8 update_rss_config;
	u8 rss_enable;
	u8 rss_eng_id;
	u8 update_rss_capabilities;
	u8 update_rss_ind_table;
	u8 update_rss_key;
	u8 rss_caps;
	u8 rss_table_size_log; /* The table size is 2 ^ rss_table_size_log */
	u16 rss_ind_table[ECORE_RSS_IND_TABLE_SIZE];
	u32 rss_key[ECORE_RSS_KEY_SIZE];
};

struct ecore_sge_tpa_params {
	u8 max_buffers_per_cqe;

	u8 update_tpa_en_flg;
	u8 tpa_ipv4_en_flg;
	u8 tpa_ipv6_en_flg;
	u8 tpa_ipv4_tunn_en_flg;
	u8 tpa_ipv6_tunn_en_flg;

	u8 update_tpa_param_flg;
	u8 tpa_pkt_split_flg;
	u8 tpa_hdr_data_split_flg;
	u8 tpa_gro_consistent_flg;
	u8 tpa_max_aggs_num;
	u16 tpa_max_size;
	u16 tpa_min_size_to_start;
	u16 tpa_min_size_to_cont;
};

enum ecore_filter_opcode {
	ECORE_FILTER_ADD,
	ECORE_FILTER_REMOVE,
	ECORE_FILTER_MOVE,
	ECORE_FILTER_REPLACE, /* Delete all MACs and add new one instead */
	ECORE_FILTER_FLUSH, /* Removes all filters */
};

enum ecore_filter_ucast_type {
	ECORE_FILTER_MAC,
	ECORE_FILTER_VLAN,
	ECORE_FILTER_MAC_VLAN,
	ECORE_FILTER_INNER_MAC,
	ECORE_FILTER_INNER_VLAN,
	ECORE_FILTER_INNER_PAIR,
	ECORE_FILTER_INNER_MAC_VNI_PAIR,
	ECORE_FILTER_MAC_VNI_PAIR,
	ECORE_FILTER_VNI,
};

struct ecore_filter_ucast {
	enum ecore_filter_opcode opcode;
	enum ecore_filter_ucast_type type;
	u8 is_rx_filter;
	u8 is_tx_filter;
	u8 vport_to_add_to;
	u8 vport_to_remove_from;
	unsigned char mac[ETH_ALEN];
	u8 assert_on_error;
	u16 vlan;
	u32 vni;
};

struct ecore_filter_mcast {
	/* MOVE is not supported for multicast */
	enum ecore_filter_opcode opcode;
	u8 vport_to_add_to;
	u8 vport_to_remove_from;
	u8	num_mc_addrs;
#define ECORE_MAX_MC_ADDRS	64
	unsigned char mac[ECORE_MAX_MC_ADDRS][ETH_ALEN];
};

struct ecore_filter_accept_flags {
	u8 update_rx_mode_config;
	u8 update_tx_mode_config;
	u8 rx_accept_filter;
	u8 tx_accept_filter;
#define	ECORE_ACCEPT_NONE		0x01
#define ECORE_ACCEPT_UCAST_MATCHED	0x02
#define ECORE_ACCEPT_UCAST_UNMATCHED	0x04
#define ECORE_ACCEPT_MCAST_MATCHED	0x08
#define ECORE_ACCEPT_MCAST_UNMATCHED	0x10
#define ECORE_ACCEPT_BCAST		0x20
};

/* Add / remove / move / remove-all unicast MAC-VLAN filters.
 * FW will assert in the following cases, so driver should take care...:
 * 1. Adding a filter to a full table.
 * 2. Adding a filter which already exists on that vport.
 * 3. Removing a filter which doesn't exist.
 */

enum _ecore_status_t
ecore_filter_ucast_cmd(struct ecore_dev *p_dev,
		       struct ecore_filter_ucast *p_filter_cmd,
		       enum spq_mode comp_mode,
		       struct ecore_spq_comp_cb *p_comp_data);

/* Add / remove / move multicast MAC filters. */
enum _ecore_status_t
ecore_filter_mcast_cmd(struct ecore_dev *p_dev,
		       struct ecore_filter_mcast *p_filter_cmd,
		       enum spq_mode comp_mode,
		       struct ecore_spq_comp_cb *p_comp_data);

/* Set "accept" filters */
enum _ecore_status_t
ecore_filter_accept_cmd(
	struct ecore_dev		 *p_dev,
	u8				 vport,
	struct ecore_filter_accept_flags accept_flags,
	u8				 update_accept_any_vlan,
	u8				 accept_any_vlan,
	enum spq_mode			 comp_mode,
	struct ecore_spq_comp_cb	 *p_comp_data);

/**
 * @brief ecore_sp_eth_rx_queue_start - RX Queue Start Ramrod
 *
 * This ramrod initializes an RX Queue for a VPort. An Assert is generated if
 * the VPort ID is not currently initialized.
 *
 * @param p_hwfn
 * @param opaque_fid
 * @p_params			[stats_id is relative, packed in p_params]
 * @param bd_max_bytes		Maximum bytes that can be placed on a BD
 * @param bd_chain_phys_addr	Physical address of BDs for receive.
 * @param cqe_pbl_addr		Physical address of the CQE PBL Table.
 * @param cqe_pbl_size		Size of the CQE PBL Table
 * @param pp_prod		Pointer to place producer's
 *                              address for the Rx Q (May be
 *				NULL).
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_eth_rx_queue_start(struct ecore_hwfn *p_hwfn,
			    u16 opaque_fid,
			    struct ecore_queue_start_common_params *p_params,
			    u16 bd_max_bytes,
			    dma_addr_t bd_chain_phys_addr,
			    dma_addr_t cqe_pbl_addr,
			    u16 cqe_pbl_size,
			    void OSAL_IOMEM * *pp_prod);

/**
 * @brief ecore_sp_eth_rx_queue_stop -
 *
 * This ramrod closes an RX queue. It sends RX queue stop ramrod
 * + CFC delete ramrod
 *
 * @param p_hwfn
 * @param rx_queue_id		RX Queue ID
 * @param eq_completion_only	If True completion will be on
 *				EQe, if False completion will be
 *				on EQe if p_hwfn opaque
 *				different from the RXQ opaque
 *				otherwise on CQe.
 * @param cqe_completion	If True completion will be
 *				receive on CQe.
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_eth_rx_queue_stop(struct ecore_hwfn *p_hwfn,
			   u16 rx_queue_id,
			   bool eq_completion_only,
			   bool cqe_completion);

/**
 * @brief ecore_sp_eth_tx_queue_start - TX Queue Start Ramrod
 *
 * This ramrod initializes a TX Queue for a VPort. An Assert is generated if
 * the VPort is not currently initialized.
 *
 * @param p_hwfn
 * @param opaque_fid
 * @p_params
 * @param tc			traffic class to use with this L2 txq
 * @param pbl_addr		address of the pbl array
 * @param pbl_size		number of entries in pbl
 * @param pp_doorbell		Pointer to place doorbell pointer (May be NULL).
 *				This address should be used with the
 *				DIRECT_REG_WR macro.
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_eth_tx_queue_start(struct ecore_hwfn *p_hwfn,
			    u16 opaque_fid,
			    struct ecore_queue_start_common_params *p_params,
			    u8 tc,
			    dma_addr_t pbl_addr,
			    u16 pbl_size,
			    void OSAL_IOMEM * *pp_doorbell);

/**
 * @brief ecore_sp_eth_tx_queue_stop -
 *
 * This ramrod closes a TX queue. It sends TX queue stop ramrod
 * + CFC delete ramrod
 *
 * @param p_hwfn
 * @param tx_queue_id		TX Queue ID
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_sp_eth_tx_queue_stop(struct ecore_hwfn *p_hwfn,
						u16 tx_queue_id);

enum ecore_tpa_mode	{
	ECORE_TPA_MODE_NONE,
	ECORE_TPA_MODE_RSC,
	ECORE_TPA_MODE_GRO,
	ECORE_TPA_MODE_MAX
};

struct ecore_sp_vport_start_params {
	enum ecore_tpa_mode tpa_mode;
	bool remove_inner_vlan;	/* Inner VLAN removal is enabled */
	bool tx_switching;	/* Vport supports tx-switching */
	bool handle_ptp_pkts;	/* Handle PTP packets */
	bool only_untagged;	/* Untagged pkt control */
	bool drop_ttl0;		/* Drop packets with TTL = 0 */
	u8 max_buffers_per_cqe;
	u32 concrete_fid;
	u16 opaque_fid;
	u8 vport_id;		/* VPORT ID */
	u16 mtu;		/* VPORT MTU */
	bool zero_placement_offset;
	bool check_mac;
	bool check_ethtype;
};

/**
 * @brief ecore_sp_vport_start -
 *
 * This ramrod initializes a VPort. An Assert if generated if the Function ID
 * of the VPort is not enabled.
 *
 * @param p_hwfn
 * @param p_params		VPORT start params
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_vport_start(struct ecore_hwfn *p_hwfn,
		     struct ecore_sp_vport_start_params *p_params);

struct ecore_sp_vport_update_params {
	u16			opaque_fid;
	u8			vport_id;
	u8			update_vport_active_rx_flg;
	u8			vport_active_rx_flg;
	u8			update_vport_active_tx_flg;
	u8			vport_active_tx_flg;
	u8			update_inner_vlan_removal_flg;
	u8			inner_vlan_removal_flg;
	u8			silent_vlan_removal_flg;
	u8			update_default_vlan_enable_flg;
	u8			default_vlan_enable_flg;
	u8			update_default_vlan_flg;
	u16			default_vlan;
	u8			update_tx_switching_flg;
	u8			tx_switching_flg;
	u8			update_approx_mcast_flg;
	u8			update_anti_spoofing_en_flg;
	u8			anti_spoofing_en;
	u8			update_accept_any_vlan_flg;
	u8			accept_any_vlan;
	unsigned long		bins[8];
	struct ecore_rss_params	*rss_params;
	struct ecore_filter_accept_flags accept_flags;
	struct ecore_sge_tpa_params *sge_tpa_params;
	/* MTU change - notice this requires the vport to be disabled.
	 * If non-zero, value would be used.
	 */
	u16 mtu;
};

/**
 * @brief ecore_sp_vport_update -
 *
 * This ramrod updates the parameters of the VPort. Every field can be updated
 * independently, according to flags.
 *
 * This ramrod is also used to set the VPort state to active after creation.
 * An Assert is generated if the VPort does not contain an RX queue.
 *
 * @param p_hwfn
 * @param p_params
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t
ecore_sp_vport_update(struct ecore_hwfn *p_hwfn,
		      struct ecore_sp_vport_update_params *p_params,
		      enum spq_mode comp_mode,
		      struct ecore_spq_comp_cb *p_comp_data);
/**
 * @brief ecore_sp_vport_stop -
 *
 * This ramrod closes a VPort after all its RX and TX queues are terminated.
 * An Assert is generated if any queues are left open.
 *
 * @param p_hwfn
 * @param opaque_fid
 * @param vport_id VPort ID
 *
 * @return enum _ecore_status_t
 */
enum _ecore_status_t ecore_sp_vport_stop(struct ecore_hwfn *p_hwfn,
					 u16 opaque_fid,
					 u8 vport_id);

enum _ecore_status_t
ecore_sp_eth_filter_ucast(struct ecore_hwfn *p_hwfn,
			  u16 opaque_fid,
			  struct ecore_filter_ucast *p_filter_cmd,
			  enum spq_mode comp_mode,
			  struct ecore_spq_comp_cb *p_comp_data);

/**
 * @brief ecore_sp_rx_eth_queues_update -
 *
 * This ramrod updates an RX queue. It is used for setting the active state
 * of the queue and updating the TPA and SGE parameters.
 *
 * @note Final phase API.
 *
 * @param p_hwfn
 * @param rx_queue_id		RX Queue ID
 * @param num_rxqs              Allow to update multiple rx
 *				queues, from rx_queue_id to
 *				(rx_queue_id + num_rxqs)
 * @param complete_cqe_flg	Post completion to the CQE Ring if set
 * @param complete_event_flg	Post completion to the Event Ring if set
 *
 * @return enum _ecore_status_t
 */

enum _ecore_status_t
ecore_sp_eth_rx_queues_update(struct ecore_hwfn *p_hwfn,
			      u16 rx_queue_id,
			      u8 num_rxqs,
			      u8 complete_cqe_flg,
			      u8 complete_event_flg,
			      enum spq_mode comp_mode,
			      struct ecore_spq_comp_cb *p_comp_data);

void __ecore_get_vport_stats(struct ecore_hwfn *p_hwfn,
			     struct ecore_ptt *p_ptt,
			     struct ecore_eth_stats *stats,
			     u16 statistics_bin, bool b_get_port_stats);

void ecore_get_vport_stats(struct ecore_dev *p_dev,
			   struct ecore_eth_stats *stats);

void ecore_reset_vport_stats(struct ecore_dev *p_dev);

#endif
