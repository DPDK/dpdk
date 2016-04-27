/*
 * Copyright (c) 2016 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.qede_pmd for copyright and licensing details.
 */

#ifndef __ECORE_VF_PF_IF_H__
#define __ECORE_VF_PF_IF_H__

#define T_ETH_INDIRECTION_TABLE_SIZE 128
#define T_ETH_RSS_KEY_SIZE 10
#ifndef aligned_u64
#define aligned_u64 u64
#endif

/***********************************************
 *
 * Common definitions for all HVs
 *
 **/
struct vf_pf_resc_request {
	u8 num_rxqs;
	u8 num_txqs;
	u8 num_sbs;
	u8 num_mac_filters;
	u8 num_vlan_filters;
	u8 num_mc_filters;	/* No limit  so superfluous */
	u16 padding;
};

struct hw_sb_info {
	u16 hw_sb_id;		/* aka absolute igu id, used to ack the sb */
	u8 sb_qid;		/* used to update DHC for sb */
	u8 padding[5];
};

/***********************************************
 *
 * HW VF-PF channel definitions
 *
 * A.K.A VF-PF mailbox
 *
 **/
#define TLV_BUFFER_SIZE		1024
#define TLV_ALIGN		sizeof(u64)
#define PF_VF_BULLETIN_SIZE	512

#define VFPF_RX_MASK_ACCEPT_NONE		0x00000000
#define VFPF_RX_MASK_ACCEPT_MATCHED_UNICAST     0x00000001
#define VFPF_RX_MASK_ACCEPT_MATCHED_MULTICAST   0x00000002
#define VFPF_RX_MASK_ACCEPT_ALL_UNICAST	0x00000004
#define VFPF_RX_MASK_ACCEPT_ALL_MULTICAST       0x00000008
#define VFPF_RX_MASK_ACCEPT_BROADCAST	0x00000010
/* TODO: #define VFPF_RX_MASK_ACCEPT_ANY_VLAN   0x00000020 */

#define BULLETIN_CONTENT_SIZE	(sizeof(struct pf_vf_bulletin_content))
#define BULLETIN_ATTEMPTS       5	/* crc failures before throwing towel */
#define BULLETIN_CRC_SEED       0

enum {
	PFVF_STATUS_WAITING = 0,
	PFVF_STATUS_SUCCESS,
	PFVF_STATUS_FAILURE,
	PFVF_STATUS_NOT_SUPPORTED,
	PFVF_STATUS_NO_RESOURCE,
	PFVF_STATUS_FORCED,
};

/* vf pf channel tlvs */
/* general tlv header (used for both vf->pf request and pf->vf response) */
struct channel_tlv {
	u16 type;
	u16 length;
};

/* header of first vf->pf tlv carries the offset used to calculate response
 * buffer address
 */
struct vfpf_first_tlv {
	struct channel_tlv tl;
	u32 padding;
	aligned_u64 reply_address;
};

/* header of pf->vf tlvs, carries the status of handling the request */
struct pfvf_tlv {
	struct channel_tlv tl;
	u8 status;
	u8 padding[3];
};

/* response tlv used for most tlvs */
struct pfvf_def_resp_tlv {
	struct pfvf_tlv hdr;
};

/* used to terminate and pad a tlv list */
struct channel_list_end_tlv {
	struct channel_tlv tl;
	u8 padding[4];
};

/* Acquire */
struct vfpf_acquire_tlv {
	struct vfpf_first_tlv first_tlv;

	struct vf_pf_vfdev_info {
#define VFPF_ACQUIRE_CAP_OVERRIDE_FW_VER		(1 << 0)
		aligned_u64 capabilties;
		u8 fw_major;
		u8 fw_minor;
		u8 fw_revision;
		u8 fw_engineering;
		u32 driver_version;
		u16 opaque_fid;	/* ME register value */
		u8 os_type;	/* VFPF_ACQUIRE_OS_* value */
		u8 padding[5];
	} vfdev_info;

	struct vf_pf_resc_request resc_request;

	aligned_u64 bulletin_addr;
	u32 bulletin_size;
	u32 padding;
};

/* receive side scaling tlv */
struct vfpf_vport_update_rss_tlv {
	struct channel_tlv tl;

	u8 update_rss_flags;
#define VFPF_UPDATE_RSS_CONFIG_FLAG	  (1 << 0)
#define VFPF_UPDATE_RSS_CAPS_FLAG	  (1 << 1)
#define VFPF_UPDATE_RSS_IND_TABLE_FLAG	  (1 << 2)
#define VFPF_UPDATE_RSS_KEY_FLAG	  (1 << 3)

	u8 rss_enable;
	u8 rss_caps;
	u8 rss_table_size_log;	/* The table size is 2 ^ rss_table_size_log */
	u16 rss_ind_table[T_ETH_INDIRECTION_TABLE_SIZE];
	u32 rss_key[T_ETH_RSS_KEY_SIZE];
};

struct pfvf_storm_stats {
	u32 address;
	u32 len;
};

struct pfvf_stats_info {
	struct pfvf_storm_stats mstats;
	struct pfvf_storm_stats pstats;
	struct pfvf_storm_stats tstats;
	struct pfvf_storm_stats ustats;
};

/* acquire response tlv - carries the allocated resources */
struct pfvf_acquire_resp_tlv {
	struct pfvf_tlv hdr;

	struct pf_vf_pfdev_info {
		u32 chip_num;
		u32 mfw_ver;

		u16 fw_major;
		u16 fw_minor;
		u16 fw_rev;
		u16 fw_eng;

		aligned_u64 capabilities;
#define PFVF_ACQUIRE_CAP_DEFAULT_UNTAGGED	(1 << 0)

		u16 db_size;
		u8 indices_per_sb;
		u8 os_type;

		/* Thesee should match the PF's ecore_dev values */
		u16 chip_rev;
		u8 dev_type;

		u8 padding;

		struct pfvf_stats_info stats_info;

		u8 port_mac[ETH_ALEN];
		u8 padding2[2];
	} pfdev_info;

	struct pf_vf_resc {
		/* in case of status NO_RESOURCE in message hdr, pf will fill
		 * this struct with suggested amount of resources for next
		 * acquire request
		 */
#define PFVF_MAX_QUEUES_PER_VF         16
#define PFVF_MAX_SBS_PER_VF            16
		struct hw_sb_info hw_sbs[PFVF_MAX_SBS_PER_VF];
		u8 hw_qid[PFVF_MAX_QUEUES_PER_VF];
		u8 cid[PFVF_MAX_QUEUES_PER_VF];

		u8 num_rxqs;
		u8 num_txqs;
		u8 num_sbs;
		u8 num_mac_filters;
		u8 num_vlan_filters;
		u8 num_mc_filters;
		u8 padding[2];
	} resc;

	u32 bulletin_size;
	u32 padding;
};

/* Init VF */
struct vfpf_init_tlv {
	struct vfpf_first_tlv first_tlv;
	aligned_u64 stats_addr;

	u16 rx_mask;
	u16 tx_mask;
	u8 drop_ttl0_flg;
	u8 padding[3];

};

/* Setup Queue */
struct vfpf_start_rxq_tlv {
	struct vfpf_first_tlv first_tlv;

	/* physical addresses */
	aligned_u64 rxq_addr;
	aligned_u64 deprecated_sge_addr;
	aligned_u64 cqe_pbl_addr;

	u16 cqe_pbl_size;
	u16 hw_sb;
	u16 rx_qid;
	u16 hc_rate;		/* desired interrupts per sec. */

	u16 bd_max_bytes;
	u16 stat_id;
	u8 sb_index;
	u8 padding[3];

};

struct vfpf_start_txq_tlv {
	struct vfpf_first_tlv first_tlv;

	/* physical addresses */
	aligned_u64 pbl_addr;
	u16 pbl_size;
	u16 stat_id;
	u16 tx_qid;
	u16 hw_sb;

	u32 flags;		/* VFPF_QUEUE_FLG_X flags */
	u16 hc_rate;		/* desired interrupts per sec. */
	u8 sb_index;
	u8 padding[3];
};

/* Stop RX Queue */
struct vfpf_stop_rxqs_tlv {
	struct vfpf_first_tlv first_tlv;

	u16 rx_qid;
	u8 num_rxqs;
	u8 cqe_completion;
	u8 padding[4];
};

/* Stop TX Queues */
struct vfpf_stop_txqs_tlv {
	struct vfpf_first_tlv first_tlv;

	u16 tx_qid;
	u8 num_txqs;
	u8 padding[5];
};

struct vfpf_update_rxq_tlv {
	struct vfpf_first_tlv first_tlv;

	aligned_u64 deprecated_sge_addr[PFVF_MAX_QUEUES_PER_VF];

	u16 rx_qid;
	u8 num_rxqs;
	u8 flags;
#define VFPF_RXQ_UPD_INIT_SGE_DEPRECATE_FLAG	(1 << 0)
#define VFPF_RXQ_UPD_COMPLETE_CQE_FLAG		(1 << 1)
#define VFPF_RXQ_UPD_COMPLETE_EVENT_FLAG	(1 << 2)

	u8 padding[4];
};

/* Set Queue Filters */
struct vfpf_q_mac_vlan_filter {
	u32 flags;
#define VFPF_Q_FILTER_DEST_MAC_VALID    0x01
#define VFPF_Q_FILTER_VLAN_TAG_VALID    0x02
#define VFPF_Q_FILTER_SET_MAC	0x100	/* set/clear */

	u8 mac[ETH_ALEN];
	u16 vlan_tag;

	u8 padding[4];
};

/* Start a vport */
struct vfpf_vport_start_tlv {
	struct vfpf_first_tlv first_tlv;

	aligned_u64 sb_addr[PFVF_MAX_SBS_PER_VF];

	u32 tpa_mode;
	u16 dep1;
	u16 mtu;

	u8 vport_id;
	u8 inner_vlan_removal;

	u8 only_untagged;
	u8 max_buffers_per_cqe;

	u8 padding[4];
};

/* Extended tlvs - need to add rss, mcast, accept mode tlvs */
struct vfpf_vport_update_activate_tlv {
	struct channel_tlv tl;
	u8 update_rx;
	u8 update_tx;
	u8 active_rx;
	u8 active_tx;
};

struct vfpf_vport_update_tx_switch_tlv {
	struct channel_tlv tl;
	u8 tx_switching;
	u8 padding[3];
};

struct vfpf_vport_update_vlan_strip_tlv {
	struct channel_tlv tl;
	u8 remove_vlan;
	u8 padding[3];
};

struct vfpf_vport_update_mcast_bin_tlv {
	struct channel_tlv tl;
	u8 padding[4];

	aligned_u64 bins[8];
};

struct vfpf_vport_update_accept_param_tlv {
	struct channel_tlv tl;
	u8 update_rx_mode;
	u8 update_tx_mode;
	u8 rx_accept_filter;
	u8 tx_accept_filter;
};

struct vfpf_vport_update_accept_any_vlan_tlv {
	struct channel_tlv tl;
	u8 update_accept_any_vlan_flg;
	u8 accept_any_vlan;

	u8 padding[2];
};

struct vfpf_vport_update_sge_tpa_tlv {
	struct channel_tlv tl;

	u16 sge_tpa_flags;
#define VFPF_TPA_IPV4_EN_FLAG	     (1 << 0)
#define VFPF_TPA_IPV6_EN_FLAG        (1 << 1)
#define VFPF_TPA_PKT_SPLIT_FLAG      (1 << 2)
#define VFPF_TPA_HDR_DATA_SPLIT_FLAG (1 << 3)
#define VFPF_TPA_GRO_CONSIST_FLAG    (1 << 4)

	u8 update_sge_tpa_flags;
#define VFPF_UPDATE_SGE_DEPRECATED_FLAG	   (1 << 0)
#define VFPF_UPDATE_TPA_EN_FLAG    (1 << 1)
#define VFPF_UPDATE_TPA_PARAM_FLAG (1 << 2)

	u8 max_buffers_per_cqe;

	u16 deprecated_sge_buff_size;
	u16 tpa_max_size;
	u16 tpa_min_size_to_start;
	u16 tpa_min_size_to_cont;

	u8 tpa_max_aggs_num;
	u8 padding[7];

};

/* Primary tlv as a header for various extended tlvs for
 * various functionalities in vport update ramrod.
 */
struct vfpf_vport_update_tlv {
	struct vfpf_first_tlv first_tlv;
};

struct vfpf_ucast_filter_tlv {
	struct vfpf_first_tlv first_tlv;

	u8 opcode;
	u8 type;

	u8 mac[ETH_ALEN];

	u16 vlan;
	u16 padding[3];
};

struct tlv_buffer_size {
	u8 tlv_buffer[TLV_BUFFER_SIZE];
};

union vfpf_tlvs {
	struct vfpf_first_tlv first_tlv;
	struct vfpf_acquire_tlv acquire;
	struct vfpf_init_tlv init;
	struct vfpf_start_rxq_tlv start_rxq;
	struct vfpf_start_txq_tlv start_txq;
	struct vfpf_stop_rxqs_tlv stop_rxqs;
	struct vfpf_stop_txqs_tlv stop_txqs;
	struct vfpf_update_rxq_tlv update_rxq;
	struct vfpf_vport_start_tlv start_vport;
	struct vfpf_vport_update_tlv vport_update;
	struct vfpf_ucast_filter_tlv ucast_filter;
	struct channel_list_end_tlv list_end;
	struct tlv_buffer_size tlv_buf_size;
};

union pfvf_tlvs {
	struct pfvf_def_resp_tlv default_resp;
	struct pfvf_acquire_resp_tlv acquire_resp;
	struct channel_list_end_tlv list_end;
	struct tlv_buffer_size tlv_buf_size;
};

/* This is a structure which is allocated in the VF, which the PF may update
 * when it deems it necessary to do so. The bulletin board is sampled
 * periodically by the VF. A copy per VF is maintained in the PF (to prevent
 * loss of data upon multiple updates (or the need for read modify write)).
 */
enum ecore_bulletin_bit {
	/* Alert the VF that a forced MAC was set by the PF */
	MAC_ADDR_FORCED = 0,

	/* The VF should not access the vfpf channel */
	VFPF_CHANNEL_INVALID = 1,

	/* Alert the VF that a forced VLAN was set by the PF */
	VLAN_ADDR_FORCED = 2,

	/* Indicate that `default_only_untagged' contains actual data */
	VFPF_BULLETIN_UNTAGGED_DEFAULT = 3,
	VFPF_BULLETIN_UNTAGGED_DEFAULT_FORCED = 4,

	/* Alert the VF that suggested mac was sent by the PF.
	 * MAC_ADDR will be disabled in case MAC_ADDR_FORCED is set
	 */
	VFPF_BULLETIN_MAC_ADDR = 5
};

struct ecore_bulletin_content {
	u32 crc;		/* crc of structure to ensure is not in
				 * mid-update
				 */
	u32 version;

	aligned_u64 valid_bitmap;	/* bitmap indicating wich fields
					 * hold valid values
					 */

	u8 mac[ETH_ALEN];	/* used for MAC_ADDR or MAC_ADDR_FORCED */

	u8 default_only_untagged;	/* If valid, 1 => only untagged Rx
					 * if no vlan filter is configured.
					 */
	u8 padding;

	/* The following is a 'copy' of ecore_mcp_link_state,
	 * ecore_mcp_link_params and ecore_mcp_link_capabilities. Since it's
	 * possible the structs will increase further along the road we cannot
	 * have it here; Instead we need to have all of its fields.
	 */
	u8 req_autoneg;
	u8 req_autoneg_pause;
	u8 req_forced_rx;
	u8 req_forced_tx;
	u8 padding2[4];

	u32 req_adv_speed;
	u32 req_forced_speed;
	u32 req_loopback;
	u32 padding3;

	u8 link_up;
	u8 full_duplex;
	u8 autoneg;
	u8 autoneg_complete;
	u8 parallel_detection;
	u8 pfc_enabled;
	u8 partner_tx_flow_ctrl_en;
	u8 partner_rx_flow_ctrl_en;
	u8 partner_adv_pause;
	u8 sfp_tx_fault;
	u8 padding4[6];

	u32 speed;
	u32 partner_adv_speed;

	u32 capability_speed;

	/* Forced vlan */
	u16 pvid;
	u16 padding5;
};

struct ecore_bulletin {
	dma_addr_t phys;
	struct ecore_bulletin_content *p_virt;
	u32 size;
};

#ifndef print_enum
enum {
/*!!!!! Make sure to update STRINGS structure accordingly !!!!!*/

	CHANNEL_TLV_NONE,	/* ends tlv sequence */
	CHANNEL_TLV_ACQUIRE,
	CHANNEL_TLV_VPORT_START,
	CHANNEL_TLV_VPORT_UPDATE,
	CHANNEL_TLV_VPORT_TEARDOWN,
	CHANNEL_TLV_START_RXQ,
	CHANNEL_TLV_START_TXQ,
	CHANNEL_TLV_STOP_RXQS,
	CHANNEL_TLV_STOP_TXQS,
	CHANNEL_TLV_UPDATE_RXQ,
	CHANNEL_TLV_INT_CLEANUP,
	CHANNEL_TLV_CLOSE,
	CHANNEL_TLV_RELEASE,
	CHANNEL_TLV_LIST_END,
	CHANNEL_TLV_UCAST_FILTER,
	CHANNEL_TLV_VPORT_UPDATE_ACTIVATE,
	CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH,
	CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP,
	CHANNEL_TLV_VPORT_UPDATE_MCAST,
	CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM,
	CHANNEL_TLV_VPORT_UPDATE_RSS,
	CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN,
	CHANNEL_TLV_VPORT_UPDATE_SGE_TPA,
	CHANNEL_TLV_MAX
/*!!!!! Make sure to update STRINGS structure accordingly !!!!!*/
};
extern const char *ecore_channel_tlvs_string[];

#else
print_enum(channel_tlvs, CHANNEL_TLV_NONE,	/* ends tlv sequence */
	   CHANNEL_TLV_ACQUIRE,
	   CHANNEL_TLV_VPORT_START,
	   CHANNEL_TLV_VPORT_UPDATE,
	   CHANNEL_TLV_VPORT_TEARDOWN,
	   CHANNEL_TLV_SETUP_RXQ,
	   CHANNEL_TLV_SETUP_TXQ,
	   CHANNEL_TLV_STOP_RXQS,
	   CHANNEL_TLV_STOP_TXQS,
	   CHANNEL_TLV_UPDATE_RXQ,
	   CHANNEL_TLV_INT_CLEANUP,
	   CHANNEL_TLV_CLOSE,
	   CHANNEL_TLV_RELEASE,
	   CHANNEL_TLV_LIST_END,
	   CHANNEL_TLV_UCAST_FILTER,
	   CHANNEL_TLV_VPORT_UPDATE_ACTIVATE,
	   CHANNEL_TLV_VPORT_UPDATE_TX_SWITCH,
	   CHANNEL_TLV_VPORT_UPDATE_VLAN_STRIP,
	   CHANNEL_TLV_VPORT_UPDATE_MCAST,
	   CHANNEL_TLV_VPORT_UPDATE_ACCEPT_PARAM,
	   CHANNEL_TLV_VPORT_UPDATE_RSS,
	   CHANNEL_TLV_VPORT_UPDATE_ACCEPT_ANY_VLAN,
	   CHANNEL_TLV_VPORT_UPDATE_SGE_TPA, CHANNEL_TLV_MAX);
#endif

#endif /* __ECORE_VF_PF_IF_H__ */
