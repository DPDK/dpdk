/*
 * Copyright (c) 2013-2015 Brocade Communications Systems, Inc.
 *
 * Copyright (c) 2015 QLogic Corporation.
 * All rights reserved.
 * www.qlogic.com
 *
 * See LICENSE.bnx2x_pmd for copyright and licensing details.
 */

#ifndef BNX2X_VFPF_H
#define BNX2X_VFPF_H

#include "ecore_sp.h"

#define VLAN_HLEN 4

struct vf_resource_query {
	uint8_t num_rxqs;
	uint8_t num_txqs;
	uint8_t num_sbs;
	uint8_t num_mac_filters;
	uint8_t num_vlan_filters;
	uint8_t num_mc_filters;
};

#define	BNX2X_VF_STATUS_SUCCESS         1
#define	BNX2X_VF_STATUS_FAILURE         2
#define	BNX2X_VF_STATUS_NO_RESOURCES    4
#define	BNX2X_VF_BULLETIN_TRIES         5

#define	BNX2X_VF_Q_FLAG_CACHE_ALIGN     0x0008
#define	BNX2X_VF_Q_FLAG_STATS           0x0010
#define	BNX2X_VF_Q_FLAG_OV              0x0020
#define	BNX2X_VF_Q_FLAG_VLAN            0x0040
#define	BNX2X_VF_Q_FLAG_COS             0x0080
#define	BNX2X_VF_Q_FLAG_HC              0x0100
#define	BNX2X_VF_Q_FLAG_DHC             0x0200
#define	BNX2X_VF_Q_FLAG_LEADING_RSS     0x0400

#define TLV_BUFFER_SIZE			1024

#define VFPF_RX_MASK_ACCEPT_NONE		0x00000000
#define VFPF_RX_MASK_ACCEPT_MATCHED_UNICAST	0x00000001
#define VFPF_RX_MASK_ACCEPT_MATCHED_MULTICAST	0x00000002
#define VFPF_RX_MASK_ACCEPT_ALL_UNICAST		0x00000004
#define VFPF_RX_MASK_ACCEPT_ALL_MULTICAST	0x00000008
#define VFPF_RX_MASK_ACCEPT_BROADCAST		0x00000010

/* general tlv header (used for both vf->pf request and pf->vf response) */
struct channel_tlv {
	uint16_t type;
	uint16_t length;
};

struct vf_first_tlv {
	struct channel_tlv tl;
	uint32_t reply_offset;
};

struct tlv_buffer_size {
	uint8_t tlv_buffer[TLV_BUFFER_SIZE];
};

/* tlv struct for all PF replies except acquire */
struct vf_common_reply_tlv {
	struct channel_tlv tl;
	uint8_t status;
	uint8_t pad[3];
};

/* used to terminate and pad a tlv list */
struct channel_list_end_tlv {
	struct channel_tlv tl;
	uint32_t pad;
};

/* Acquire */
struct vf_acquire_tlv {
	struct vf_first_tlv first_tlv;

	uint8_t vf_id;
	uint8_t pad[3];

	struct vf_resource_query res_query;

	uint64_t bulletin_addr;
};

/* simple operation request on queue */
struct vf_q_op_tlv {
	struct vf_first_tlv	first_tlv;
	uint8_t vf_qid;
	uint8_t pad[3];
};

/* receive side scaling tlv */
struct vf_rss_tlv {
	struct vf_first_tlv	first_tlv;
	uint32_t		rss_flags;
	uint8_t			rss_result_mask;
	uint8_t			ind_table_size;
	uint8_t			rss_key_size;
	uint8_t			pad;
	uint8_t			ind_table[T_ETH_INDIRECTION_TABLE_SIZE];
	uint32_t		rss_key[T_ETH_RSS_KEY];	/* hash values */
};

struct vf_resc {
#define BNX2X_VF_MAX_QUEUES_PER_VF         16
#define BNX2X_VF_MAX_SBS_PER_VF            16
	uint16_t hw_sbs[BNX2X_VF_MAX_SBS_PER_VF];
	uint8_t hw_qid[BNX2X_VF_MAX_QUEUES_PER_VF];
	uint8_t num_rxqs;
	uint8_t num_txqs;
	uint8_t num_sbs;
	uint8_t num_mac_filters;
	uint8_t num_vlan_filters;
	uint8_t num_mc_filters;
	uint8_t permanent_mac_addr[ETH_ALEN];
	struct ether_addr current_mac_addr;
	uint16_t pf_link_speed;
	uint32_t pf_link_supported;
};

/* tlv struct holding reply for acquire */
struct vf_acquire_resp_tlv {
	uint16_t type;
	uint16_t length;
	uint8_t status;
	uint8_t pad1[3];
	uint32_t chip_num;
	uint8_t pad2[4];
	char fw_ver[32];
	uint16_t db_size;
	uint8_t pad3[2];
	struct vf_resc resc;
};

/* Init VF */
struct vf_init_tlv {
	struct vf_first_tlv first_tlv;
	uint64_t sb_addr[BNX2X_VF_MAX_SBS_PER_VF];
	uint64_t spq_addr;
	uint64_t stats_addr;
	uint16_t stats_step;
	uint32_t flags;
	uint32_t pad[2];
};

struct vf_rxq_params {
	/* physical addresses */
	uint64_t rcq_addr;
	uint64_t rcq_np_addr;
	uint64_t rxq_addr;
	uint64_t pad1;

	/* sb + hc info */
	uint8_t  vf_sb_id;
	uint8_t  sb_cq_index;
	uint16_t hc_rate;	/* desired interrupts per sec. */
	/* rx buffer info */
	uint16_t mtu;
	uint16_t buf_sz;
	uint16_t flags;         /* for BNX2X_VF_Q_FLAG_X flags */
	uint16_t stat_id;	/* valid if BNX2X_VF_Q_FLAG_STATS */

	uint8_t pad2[5];

	uint8_t drop_flags;
	uint8_t cache_line_log;	/* BNX2X_VF_Q_FLAG_CACHE_ALIGN */
	uint8_t pad3;
};

struct vf_txq_params {
	/* physical addresses */
	uint64_t txq_addr;

	/* sb + hc info */
	uint8_t  vf_sb_id;	/* index in hw_sbs[] */
	uint8_t  sb_index;	/* Index in the SB */
	uint16_t hc_rate;	/* desired interrupts per sec. */
	uint32_t flags;		/* for BNX2X_VF_Q_FLAG_X flags */
	uint16_t stat_id;	/* valid if BNX2X_VF_Q_FLAG_STATS */
	uint8_t  traffic_type;	/* see in setup_context() */
	uint8_t  pad;
};

/* Setup Queue */
struct vf_setup_q_tlv {
	struct vf_first_tlv first_tlv;

	struct vf_rxq_params rxq;
	struct vf_txq_params txq;

	uint8_t vf_qid;			/* index in hw_qid[] */
	uint8_t param_valid;
	#define VF_RXQ_VALID		0x01
	#define VF_TXQ_VALID		0x02
	uint8_t pad[2];
};

/* Set Queue Filters */
struct vf_q_mac_vlan_filter {
	uint32_t flags;
	#define BNX2X_VF_Q_FILTER_DEST_MAC_VALID	0x01
	#define BNX2X_VF_Q_FILTER_VLAN_TAG_VALID	0x02
	#define BNX2X_VF_Q_FILTER_SET_MAC		0x100	/* set/clear */
	uint8_t  mac[ETH_ALEN];
	uint16_t vlan_tag;
};


#define _UP_ETH_ALEN	(6)

/* configure queue filters */
struct vf_set_q_filters_tlv {
	struct vf_first_tlv first_tlv;

	uint32_t flags;
	#define BNX2X_VF_MAC_VLAN_CHANGED 	0x01
	#define BNX2X_VF_MULTICAST_CHANGED	0x02
	#define BNX2X_VF_RX_MASK_CHANGED  	0x04

	uint8_t vf_qid;			/* index in hw_qid[] */
	uint8_t mac_filters_cnt;
	uint8_t multicast_cnt;
	uint8_t pad;

	#define VF_MAX_MAC_FILTERS			16
	#define VF_MAX_VLAN_FILTERS       		16
	#define VF_MAX_FILTERS 			(VF_MAX_MAC_FILTERS +\
							VF_MAX_VLAN_FILTERS)
	struct vf_q_mac_vlan_filter filters[VF_MAX_FILTERS];

	#define VF_MAX_MULTICAST_PER_VF   		32
	uint8_t  multicast[VF_MAX_MULTICAST_PER_VF][_UP_ETH_ALEN];
	unsigned long rx_mask;
};


/* close VF (disable VF) */
struct vf_close_tlv {
	struct vf_first_tlv	first_tlv;
	uint16_t		vf_id;  /* for debug */
	uint8_t pad[2];
};

/* rlease the VF's acquired resources */
struct vf_release_tlv {
	struct vf_first_tlv   first_tlv;
	uint16_t		vf_id;  /* for debug */
	uint8_t pad[2];
};

union query_tlvs {
	struct vf_first_tlv		first_tlv;
	struct vf_acquire_tlv		acquire;
	struct vf_init_tlv		init;
	struct vf_close_tlv		close;
	struct vf_q_op_tlv		q_op;
	struct vf_setup_q_tlv		setup_q;
	struct vf_set_q_filters_tlv	set_q_filters;
	struct vf_release_tlv		release;
	struct vf_rss_tlv		update_rss;
	struct channel_list_end_tlv     list_end;
	struct tlv_buffer_size		tlv_buf_size;
};

union resp_tlvs {
	struct vf_common_reply_tlv	common_reply;
	struct vf_acquire_resp_tlv	acquire_resp;
	struct channel_list_end_tlv	list_end;
	struct tlv_buffer_size		tlv_buf_size;
};

/* struct allocated by VF driver, PF sends updates to VF via bulletin */
struct bnx2x_vf_bulletin {
	uint32_t crc;			/* crc of structure to ensure is not in
					 * mid-update
					 */
	uint16_t version;
	uint16_t length;

	uint64_t valid_bitmap;	/* bitmap indicating which fields
					 * hold valid values
					 */

#define MAC_ADDR_VALID		0	/* alert the vf that a new mac address
					 * is available for it
					 */
#define VLAN_VALID		1	/* when set, the vf should no access the
					 * vf channel
					 */
#define CHANNEL_DOWN		2	/* vf channel is disabled. VFs are not
					 * to attempt to send messages on the
					 * channel after this bit is set
					 */
	uint8_t mac[ETH_ALEN];
	uint8_t mac_pad[2];

	uint16_t vlan;
	uint8_t vlan_pad[6];
};

#define MAX_TLVS_IN_LIST 50
enum channel_tlvs {
	BNX2X_VF_TLV_NONE, /* ends tlv sequence */
	BNX2X_VF_TLV_ACQUIRE,
	BNX2X_VF_TLV_INIT,
	BNX2X_VF_TLV_SETUP_Q,
	BNX2X_VF_TLV_SET_Q_FILTERS,
	BNX2X_VF_TLV_ACTIVATE_Q,
	BNX2X_VF_TLV_DEACTIVATE_Q,
	BNX2X_VF_TLV_TEARDOWN_Q,
	BNX2X_VF_TLV_CLOSE,
	BNX2X_VF_TLV_RELEASE,
	BNX2X_VF_TLV_UPDATE_RSS_OLD,
	BNX2X_VF_TLV_PF_RELEASE_VF,
	BNX2X_VF_TLV_LIST_END,
	BNX2X_VF_TLV_FLR,
	BNX2X_VF_TLV_PF_SET_MAC,
	BNX2X_VF_TLV_PF_SET_VLAN,
	BNX2X_VF_TLV_UPDATE_RSS,
	BNX2X_VF_TLV_PHYS_PORT_ID,
	BNX2X_VF_TLV_MAX
};

struct bnx2x_vf_mbx_msg {
	union query_tlvs query[BNX2X_VF_MAX_QUEUES_PER_VF];
	union resp_tlvs resp;
};

int bnx2x_vf_set_mac(struct bnx2x_softc *sc, int set);
int bnx2x_vf_config_rss(struct bnx2x_softc *sc, struct ecore_config_rss_params *params);

#endif /* BNX2X_VFPF_H */
