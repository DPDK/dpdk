/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef __OTX2_MBOX_H__
#define __OTX2_MBOX_H__

#include <errno.h>
#include <stdbool.h>

#include <rte_ether.h>
#include <rte_spinlock.h>

#include <otx2_common.h>

#define SZ_64K			(64ULL * 1024ULL)
#define SZ_1K			(1ULL * 1024ULL)
#define MBOX_SIZE		SZ_64K

/* AF/PF: PF initiated, PF/VF VF initiated */
#define MBOX_DOWN_RX_START	0
#define MBOX_DOWN_RX_SIZE	(46 * SZ_1K)
#define MBOX_DOWN_TX_START	(MBOX_DOWN_RX_START + MBOX_DOWN_RX_SIZE)
#define MBOX_DOWN_TX_SIZE	(16 * SZ_1K)
/* AF/PF: AF initiated, PF/VF PF initiated */
#define MBOX_UP_RX_START	(MBOX_DOWN_TX_START + MBOX_DOWN_TX_SIZE)
#define MBOX_UP_RX_SIZE		SZ_1K
#define MBOX_UP_TX_START	(MBOX_UP_RX_START + MBOX_UP_RX_SIZE)
#define MBOX_UP_TX_SIZE		SZ_1K

#if MBOX_UP_TX_SIZE + MBOX_UP_TX_START != MBOX_SIZE
# error "Incorrect mailbox area sizes"
#endif

#define INTR_MASK(pfvfs) ((pfvfs < 64) ? (BIT_ULL(pfvfs) - 1) : (~0ull))

#define MBOX_RSP_TIMEOUT	3000 /* Time to wait for mbox response in ms */

#define MBOX_MSG_ALIGN		16  /* Align mbox msg start to 16bytes */

/* Mailbox directions */
#define MBOX_DIR_AFPF		0  /* AF replies to PF */
#define MBOX_DIR_PFAF		1  /* PF sends messages to AF */
#define MBOX_DIR_PFVF		2  /* PF replies to VF */
#define MBOX_DIR_VFPF		3  /* VF sends messages to PF */
#define MBOX_DIR_AFPF_UP	4  /* AF sends messages to PF */
#define MBOX_DIR_PFAF_UP	5  /* PF replies to AF */
#define MBOX_DIR_PFVF_UP	6  /* PF sends messages to VF */
#define MBOX_DIR_VFPF_UP	7  /* VF replies to PF */

/* Device memory does not support unaligned access, instruct compiler to
 * not optimize the memory access when working with mailbox memory.
 */
#define __otx2_io volatile

struct otx2_mbox_dev {
	void	    *mbase;   /* This dev's mbox region */
	rte_spinlock_t  mbox_lock;
	uint16_t     msg_size; /* Total msg size to be sent */
	uint16_t     rsp_size; /* Total rsp size to be sure the reply is ok */
	uint16_t     num_msgs; /* No of msgs sent or waiting for response */
	uint16_t     msgs_acked; /* No of msgs for which response is received */
};

struct otx2_mbox {
	uintptr_t hwbase;  /* Mbox region advertised by HW */
	uintptr_t reg_base;/* CSR base for this dev */
	uint64_t trigger;  /* Trigger mbox notification */
	uint16_t tr_shift; /* Mbox trigger shift */
	uint64_t rx_start; /* Offset of Rx region in mbox memory */
	uint64_t tx_start; /* Offset of Tx region in mbox memory */
	uint16_t rx_size;  /* Size of Rx region */
	uint16_t tx_size;  /* Size of Tx region */
	uint16_t ndevs;    /* The number of peers */
	struct otx2_mbox_dev *dev;
	uint64_t intr_offset; /* Offset to interrupt register */
};

/* Header which precedes all mbox messages */
struct mbox_hdr {
	uint64_t __otx2_io msg_size;   /* Total msgs size embedded */
	uint16_t __otx2_io num_msgs;   /* No of msgs embedded */
};

/* Header which precedes every msg and is also part of it */
struct mbox_msghdr {
	uint16_t __otx2_io pcifunc; /* Who's sending this msg */
	uint16_t __otx2_io id;      /* Mbox message ID */
#define OTX2_MBOX_REQ_SIG (0xdead)
#define OTX2_MBOX_RSP_SIG (0xbeef)
	/* Signature, for validating corrupted msgs */
	uint16_t __otx2_io sig;
#define OTX2_MBOX_VERSION (0x000a)
	/* Version of msg's structure for this ID */
	uint16_t __otx2_io ver;
	/* Offset of next msg within mailbox region */
	uint16_t __otx2_io next_msgoff;
	int __otx2_io rc; /* Msg processed response code */
};

/* Mailbox message types */
#define MBOX_MSG_MASK				0xFFFF
#define MBOX_MSG_INVALID			0xFFFE
#define MBOX_MSG_MAX				0xFFFF

#define MBOX_MESSAGES							\
/* Generic mbox IDs (range 0x000 - 0x1FF) */				\
M(READY,		0x001, ready, msg_req, ready_msg_rsp)		\
M(ATTACH_RESOURCES,	0x002, attach_resources, rsrc_attach_req, msg_rsp)\
M(DETACH_RESOURCES,	0x003, detach_resources, rsrc_detach_req, msg_rsp)\
M(FREE_RSRC_CNT,	0x004, free_rsrc_cnt, msg_req, free_rsrcs_rsp)	\
M(MSIX_OFFSET,		0x005, msix_offset, msg_req, msix_offset_rsp)	\
M(VF_FLR,		0x006, vf_flr, msg_req, msg_rsp)		\
M(PTP_OP,		0x007, ptp_op, ptp_req, ptp_rsp)		\
M(GET_HW_CAP,		0x008, get_hw_cap, msg_req, get_hw_cap_rsp)	\
M(NDC_SYNC_OP,		0x009, ndc_sync_op, ndc_sync_op, msg_rsp)	\
/* CGX mbox IDs (range 0x200 - 0x3FF) */				\
M(CGX_START_RXTX,	0x200, cgx_start_rxtx, msg_req, msg_rsp)	\
M(CGX_STOP_RXTX,	0x201, cgx_stop_rxtx, msg_req, msg_rsp)		\
M(CGX_STATS,		0x202, cgx_stats, msg_req, cgx_stats_rsp)	\
M(CGX_MAC_ADDR_SET,	0x203, cgx_mac_addr_set, cgx_mac_addr_set_or_get,\
				cgx_mac_addr_set_or_get)		\
M(CGX_MAC_ADDR_GET,	0x204, cgx_mac_addr_get, cgx_mac_addr_set_or_get,\
				cgx_mac_addr_set_or_get)		\
M(CGX_PROMISC_ENABLE,	0x205, cgx_promisc_enable, msg_req, msg_rsp)	\
M(CGX_PROMISC_DISABLE,	0x206, cgx_promisc_disable, msg_req, msg_rsp)	\
M(CGX_START_LINKEVENTS, 0x207, cgx_start_linkevents, msg_req, msg_rsp)	\
M(CGX_STOP_LINKEVENTS,	0x208, cgx_stop_linkevents, msg_req, msg_rsp)	\
M(CGX_GET_LINKINFO,	0x209, cgx_get_linkinfo, msg_req, cgx_link_info_msg)\
M(CGX_INTLBK_ENABLE,	0x20A, cgx_intlbk_enable, msg_req, msg_rsp)	\
M(CGX_INTLBK_DISABLE,	0x20B, cgx_intlbk_disable, msg_req, msg_rsp)	\
M(CGX_PTP_RX_ENABLE,	0x20C, cgx_ptp_rx_enable, msg_req, msg_rsp)	\
M(CGX_PTP_RX_DISABLE,	0x20D, cgx_ptp_rx_disable, msg_req, msg_rsp)	\
M(CGX_CFG_PAUSE_FRM,	0x20E, cgx_cfg_pause_frm, cgx_pause_frm_cfg,	\
				cgx_pause_frm_cfg)			\
M(CGX_FW_DATA_GET,	0x20F, cgx_get_aux_link_info, msg_req, cgx_fw_data) \
M(CGX_FEC_SET,		0x210, cgx_set_fec_param, fec_mode, fec_mode) \
M(CGX_MAC_ADDR_ADD,     0x211, cgx_mac_addr_add, cgx_mac_addr_add_req,	\
				cgx_mac_addr_add_rsp)			\
M(CGX_MAC_ADDR_DEL,     0x212, cgx_mac_addr_del, cgx_mac_addr_del_req,	\
				msg_rsp)				\
M(CGX_MAC_MAX_ENTRIES_GET, 0x213, cgx_mac_max_entries_get, msg_req,	\
				 cgx_max_dmac_entries_get_rsp)		\
M(CGX_SET_LINK_STATE,	0x214, cgx_set_link_state,		\
			cgx_set_link_state_msg, msg_rsp)		\
M(CGX_GET_PHY_MOD_TYPE, 0x215, cgx_get_phy_mod_type, msg_req,		\
				cgx_phy_mod_type)			\
M(CGX_SET_PHY_MOD_TYPE, 0x216, cgx_set_phy_mod_type, cgx_phy_mod_type,	\
				msg_rsp)				\
M(CGX_FEC_STATS,	0x217, cgx_fec_stats, msg_req, cgx_fec_stats_rsp) \
M(CGX_SET_LINK_MODE,	0x218, cgx_set_link_mode, cgx_set_link_mode_req,\
			       cgx_set_link_mode_rsp)			\
M(CGX_GET_PHY_FEC_STATS, 0x219, cgx_get_phy_fec_stats, msg_req, msg_rsp) \
M(CGX_STATS_RST,	0x21A, cgx_stats_rst, msg_req, msg_rsp)		\
/* NPA mbox IDs (range 0x400 - 0x5FF) */				\
M(NPA_LF_ALLOC,		0x400, npa_lf_alloc, npa_lf_alloc_req,		\
				npa_lf_alloc_rsp)			\
M(NPA_LF_FREE,		0x401, npa_lf_free, msg_req, msg_rsp)		\
M(NPA_AQ_ENQ,		0x402, npa_aq_enq, npa_aq_enq_req, npa_aq_enq_rsp)\
M(NPA_HWCTX_DISABLE,	0x403, npa_hwctx_disable, hwctx_disable_req, msg_rsp)\
/* SSO/SSOW mbox IDs (range 0x600 - 0x7FF) */				\
M(SSO_LF_ALLOC,		0x600, sso_lf_alloc, sso_lf_alloc_req,		\
				sso_lf_alloc_rsp)			\
M(SSO_LF_FREE,		0x601, sso_lf_free, sso_lf_free_req, msg_rsp)	\
M(SSOW_LF_ALLOC,	0x602, ssow_lf_alloc, ssow_lf_alloc_req, msg_rsp)\
M(SSOW_LF_FREE,		0x603, ssow_lf_free, ssow_lf_free_req, msg_rsp)	\
M(SSO_HW_SETCONFIG,	0x604, sso_hw_setconfig, sso_hw_setconfig,	\
				msg_rsp)				\
M(SSO_GRP_SET_PRIORITY,	0x605, sso_grp_set_priority, sso_grp_priority,	\
				msg_rsp)				\
M(SSO_GRP_GET_PRIORITY,	0x606, sso_grp_get_priority, sso_info_req,	\
				sso_grp_priority)			\
M(SSO_WS_CACHE_INV,	0x607, sso_ws_cache_inv, msg_req, msg_rsp)	\
M(SSO_GRP_QOS_CONFIG,	0x608, sso_grp_qos_config, sso_grp_qos_cfg,	\
				msg_rsp)				\
M(SSO_GRP_GET_STATS,	0x609, sso_grp_get_stats, sso_info_req,		\
				sso_grp_stats)				\
M(SSO_HWS_GET_STATS,	0x610, sso_hws_get_stats, sso_info_req,		\
				sso_hws_stats)				\
M(SSO_HW_RELEASE_XAQ,	0x611, sso_hw_release_xaq_aura,			\
				sso_release_xaq, msg_rsp)		\
/* TIM mbox IDs (range 0x800 - 0x9FF) */				\
M(TIM_LF_ALLOC,		0x800, tim_lf_alloc, tim_lf_alloc_req,		\
				tim_lf_alloc_rsp)			\
M(TIM_LF_FREE,		0x801, tim_lf_free, tim_ring_req, msg_rsp)	\
M(TIM_CONFIG_RING,	0x802, tim_config_ring, tim_config_req, msg_rsp)\
M(TIM_ENABLE_RING,	0x803, tim_enable_ring, tim_ring_req,		\
				tim_enable_rsp)				\
M(TIM_DISABLE_RING,	0x804, tim_disable_ring, tim_ring_req, msg_rsp)	\
/* CPT mbox IDs (range 0xA00 - 0xBFF) */				\
M(CPT_LF_ALLOC,		0xA00, cpt_lf_alloc, cpt_lf_alloc_req_msg,	\
			       cpt_lf_alloc_rsp_msg)			\
M(CPT_LF_FREE,		0xA01, cpt_lf_free, msg_req, msg_rsp)		\
M(CPT_RD_WR_REGISTER,	0xA02, cpt_rd_wr_register, cpt_rd_wr_reg_msg,	\
			       cpt_rd_wr_reg_msg)			\
M(CPT_SET_CRYPTO_GRP,	0xA03, cpt_set_crypto_grp,			\
			       cpt_set_crypto_grp_req_msg,		\
			       msg_rsp)					\
M(CPT_INLINE_IPSEC_CFG, 0xA04, cpt_inline_ipsec_cfg,			\
			       cpt_inline_ipsec_cfg_msg, msg_rsp)	\
M(CPT_RX_INLINE_LF_CFG, 0xBFE, cpt_rx_inline_lf_cfg,			\
			       cpt_rx_inline_lf_cfg_msg, msg_rsp)	\
M(CPT_GET_CAPS,		0xBFD, cpt_caps_get, msg_req, cpt_caps_rsp_msg)	\
/* REE mbox IDs (range 0xE00 - 0xFFF) */				\
M(REE_CONFIG_LF,	0xE01, ree_config_lf, ree_lf_req_msg,		\
				msg_rsp)				\
M(REE_RD_WR_REGISTER,	0xE02, ree_rd_wr_register, ree_rd_wr_reg_msg,	\
				ree_rd_wr_reg_msg)			\
M(REE_RULE_DB_PROG,	0xE03, ree_rule_db_prog,			\
				ree_rule_db_prog_req_msg,		\
				msg_rsp)				\
M(REE_RULE_DB_LEN_GET,	0xE04, ree_rule_db_len_get, ree_req_msg,	\
				ree_rule_db_len_rsp_msg)		\
M(REE_RULE_DB_GET,	0xE05, ree_rule_db_get,				\
				ree_rule_db_get_req_msg,		\
				ree_rule_db_get_rsp_msg)		\
/* NPC mbox IDs (range 0x6000 - 0x7FFF) */				\
M(NPC_MCAM_ALLOC_ENTRY,	0x6000, npc_mcam_alloc_entry,			\
				npc_mcam_alloc_entry_req,		\
				npc_mcam_alloc_entry_rsp)		\
M(NPC_MCAM_FREE_ENTRY,	0x6001, npc_mcam_free_entry,			\
				npc_mcam_free_entry_req, msg_rsp)	\
M(NPC_MCAM_WRITE_ENTRY,	0x6002, npc_mcam_write_entry,			\
				npc_mcam_write_entry_req, msg_rsp)	\
M(NPC_MCAM_ENA_ENTRY,	0x6003, npc_mcam_ena_entry,			\
				npc_mcam_ena_dis_entry_req, msg_rsp)	\
M(NPC_MCAM_DIS_ENTRY,	0x6004, npc_mcam_dis_entry,			\
				npc_mcam_ena_dis_entry_req, msg_rsp)	\
M(NPC_MCAM_SHIFT_ENTRY,	0x6005, npc_mcam_shift_entry,			\
				npc_mcam_shift_entry_req,		\
				npc_mcam_shift_entry_rsp)		\
M(NPC_MCAM_ALLOC_COUNTER,	0x6006, npc_mcam_alloc_counter,		\
				npc_mcam_alloc_counter_req,		\
				npc_mcam_alloc_counter_rsp)		\
M(NPC_MCAM_FREE_COUNTER,	0x6007, npc_mcam_free_counter,		\
				npc_mcam_oper_counter_req,		\
				msg_rsp)				\
M(NPC_MCAM_UNMAP_COUNTER,	0x6008, npc_mcam_unmap_counter,		\
				npc_mcam_unmap_counter_req,		\
				msg_rsp)				\
M(NPC_MCAM_CLEAR_COUNTER,	0x6009, npc_mcam_clear_counter,		\
				npc_mcam_oper_counter_req,		\
				msg_rsp)				\
M(NPC_MCAM_COUNTER_STATS,	0x600a, npc_mcam_counter_stats,		\
				npc_mcam_oper_counter_req,		\
				npc_mcam_oper_counter_rsp)		\
M(NPC_MCAM_ALLOC_AND_WRITE_ENTRY, 0x600b, npc_mcam_alloc_and_write_entry,\
				npc_mcam_alloc_and_write_entry_req,	\
				npc_mcam_alloc_and_write_entry_rsp)	\
M(NPC_GET_KEX_CFG,	  0x600c, npc_get_kex_cfg, msg_req,		\
				npc_get_kex_cfg_rsp)			\
M(NPC_INSTALL_FLOW,	  0x600d, npc_install_flow,			\
				  npc_install_flow_req,			\
				  npc_install_flow_rsp)			\
M(NPC_DELETE_FLOW,	  0x600e, npc_delete_flow,			\
				  npc_delete_flow_req, msg_rsp)		\
M(NPC_MCAM_READ_ENTRY,	  0x600f, npc_mcam_read_entry,			\
				  npc_mcam_read_entry_req,		\
				  npc_mcam_read_entry_rsp)		\
M(NPC_SET_PKIND,          0x6010, npc_set_pkind,                        \
				  npc_set_pkind,                        \
				  msg_rsp)                              \
M(NPC_MCAM_READ_BASE_RULE, 0x6011, npc_read_base_steer_rule, msg_req,   \
				   npc_mcam_read_base_rule_rsp)         \
/* NIX mbox IDs (range 0x8000 - 0xFFFF) */				\
M(NIX_LF_ALLOC,		0x8000, nix_lf_alloc, nix_lf_alloc_req,		\
				nix_lf_alloc_rsp)			\
M(NIX_LF_FREE,		0x8001, nix_lf_free, nix_lf_free_req, msg_rsp)	\
M(NIX_AQ_ENQ,		0x8002, nix_aq_enq, nix_aq_enq_req,		\
				nix_aq_enq_rsp)				\
M(NIX_HWCTX_DISABLE,	0x8003, nix_hwctx_disable, hwctx_disable_req,	\
				msg_rsp)				\
M(NIX_TXSCH_ALLOC,	0x8004, nix_txsch_alloc, nix_txsch_alloc_req,	\
				nix_txsch_alloc_rsp)			\
M(NIX_TXSCH_FREE,	0x8005, nix_txsch_free,	nix_txsch_free_req,	\
				msg_rsp)				\
M(NIX_TXSCHQ_CFG,	0x8006, nix_txschq_cfg, nix_txschq_config,	\
				nix_txschq_config)			\
M(NIX_STATS_RST,	0x8007, nix_stats_rst, msg_req, msg_rsp)	\
M(NIX_VTAG_CFG,		0x8008, nix_vtag_cfg, nix_vtag_config, msg_rsp)	\
M(NIX_RSS_FLOWKEY_CFG,	0x8009, nix_rss_flowkey_cfg,			\
				nix_rss_flowkey_cfg,			\
				nix_rss_flowkey_cfg_rsp)		\
M(NIX_SET_MAC_ADDR,	0x800a, nix_set_mac_addr, nix_set_mac_addr,	\
				msg_rsp)				\
M(NIX_SET_RX_MODE,	0x800b, nix_set_rx_mode, nix_rx_mode, msg_rsp)	\
M(NIX_SET_HW_FRS,	0x800c, nix_set_hw_frs,	nix_frs_cfg, msg_rsp)	\
M(NIX_LF_START_RX,	0x800d, nix_lf_start_rx, msg_req, msg_rsp)	\
M(NIX_LF_STOP_RX,	0x800e, nix_lf_stop_rx,	msg_req, msg_rsp)	\
M(NIX_MARK_FORMAT_CFG,	0x800f, nix_mark_format_cfg,			\
				nix_mark_format_cfg,			\
				nix_mark_format_cfg_rsp)		\
M(NIX_SET_RX_CFG,	0x8010, nix_set_rx_cfg, nix_rx_cfg, msg_rsp)	\
M(NIX_LSO_FORMAT_CFG,	0x8011, nix_lso_format_cfg, nix_lso_format_cfg,	\
				nix_lso_format_cfg_rsp)			\
M(NIX_LF_PTP_TX_ENABLE,	0x8013, nix_lf_ptp_tx_enable, msg_req,		\
				msg_rsp)				\
M(NIX_LF_PTP_TX_DISABLE,	0x8014, nix_lf_ptp_tx_disable, msg_req,	\
				msg_rsp)				\
M(NIX_SET_VLAN_TPID,	0x8015, nix_set_vlan_tpid, nix_set_vlan_tpid,	\
				msg_rsp)				\
M(NIX_BP_ENABLE,	0x8016, nix_bp_enable, nix_bp_cfg_req,		\
				nix_bp_cfg_rsp)				\
M(NIX_BP_DISABLE,	0x8017, nix_bp_disable,	nix_bp_cfg_req, msg_rsp)\
M(NIX_GET_MAC_ADDR,	0x8018, nix_get_mac_addr, msg_req,		\
				nix_get_mac_addr_rsp)			\
M(NIX_INLINE_IPSEC_CFG,	0x8019, nix_inline_ipsec_cfg,			\
				nix_inline_ipsec_cfg, msg_rsp)		\
M(NIX_INLINE_IPSEC_LF_CFG,						\
			0x801a, nix_inline_ipsec_lf_cfg,		\
				nix_inline_ipsec_lf_cfg, msg_rsp)

/* Messages initiated by AF (range 0xC00 - 0xDFF) */
#define MBOX_UP_CGX_MESSAGES						\
M(CGX_LINK_EVENT,	0xC00, cgx_link_event, cgx_link_info_msg,	\
				msg_rsp)				\
M(CGX_PTP_RX_INFO,	0xC01, cgx_ptp_rx_info, cgx_ptp_rx_info_msg,	\
				msg_rsp)

enum {
#define M(_name, _id, _1, _2, _3) MBOX_MSG_ ## _name = _id,
MBOX_MESSAGES
MBOX_UP_CGX_MESSAGES
#undef M
};

/* Mailbox message formats */

#define RVU_DEFAULT_PF_FUNC     0xFFFF

/* Generic request msg used for those mbox messages which
 * don't send any data in the request.
 */
struct msg_req {
	struct mbox_msghdr hdr;
};

/* Generic response msg used a ack or response for those mbox
 * messages which doesn't have a specific rsp msg format.
 */
struct msg_rsp {
	struct mbox_msghdr hdr;
};

/* RVU mailbox error codes
 * Range 256 - 300.
 */
enum rvu_af_status {
	RVU_INVALID_VF_ID           = -256,
};

struct ready_msg_rsp {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io sclk_feq; /* SCLK frequency */
	uint16_t __otx2_io rclk_freq; /* RCLK frequency */
};

/* Struct to set pkind */
struct npc_set_pkind {
	struct mbox_msghdr hdr;
#define OTX2_PRIV_FLAGS_DEFAULT  BIT_ULL(0)
#define OTX2_PRIV_FLAGS_EDSA     BIT_ULL(1)
#define OTX2_PRIV_FLAGS_HIGIG    BIT_ULL(2)
#define OTX2_PRIV_FLAGS_LEN_90B  BIT_ULL(3)
#define OTX2_PRIV_FLAGS_CUSTOM   BIT_ULL(63)
	uint64_t __otx2_io mode;
#define PKIND_TX		BIT_ULL(0)
#define PKIND_RX		BIT_ULL(1)
	uint8_t __otx2_io dir;
	uint8_t __otx2_io pkind; /* valid only in case custom flag */
};

/* Structure for requesting resource provisioning.
 * 'modify' flag to be used when either requesting more
 * or to detach partial of a certain resource type.
 * Rest of the fields specify how many of what type to
 * be attached.
 * To request LFs from two blocks of same type this mailbox
 * can be sent twice as below:
 *      struct rsrc_attach *attach;
 *       .. Allocate memory for message ..
 *       attach->cptlfs = 3; <3 LFs from CPT0>
 *       .. Send message ..
 *       .. Allocate memory for message ..
 *       attach->modify = 1;
 *       attach->cpt_blkaddr = BLKADDR_CPT1;
 *       attach->cptlfs = 2; <2 LFs from CPT1>
 *       .. Send message ..
 */
struct rsrc_attach_req {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io modify:1;
	uint8_t __otx2_io npalf:1;
	uint8_t __otx2_io nixlf:1;
	uint16_t __otx2_io sso;
	uint16_t __otx2_io ssow;
	uint16_t __otx2_io timlfs;
	uint16_t __otx2_io cptlfs;
	uint16_t __otx2_io reelfs;
	/* BLKADDR_CPT0/BLKADDR_CPT1 or 0 for BLKADDR_CPT0 */
	int __otx2_io cpt_blkaddr;
	/* BLKADDR_REE0/BLKADDR_REE1 or 0 for BLKADDR_REE0 */
	int __otx2_io ree_blkaddr;
};

/* Structure for relinquishing resources.
 * 'partial' flag to be used when relinquishing all resources
 * but only of a certain type. If not set, all resources of all
 * types provisioned to the RVU function will be detached.
 */
struct rsrc_detach_req {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io partial:1;
	uint8_t __otx2_io npalf:1;
	uint8_t __otx2_io nixlf:1;
	uint8_t __otx2_io sso:1;
	uint8_t __otx2_io ssow:1;
	uint8_t __otx2_io timlfs:1;
	uint8_t __otx2_io cptlfs:1;
	uint8_t __otx2_io reelfs:1;
};

/* NIX Transmit schedulers */
#define	NIX_TXSCH_LVL_SMQ 0x0
#define	NIX_TXSCH_LVL_MDQ 0x0
#define	NIX_TXSCH_LVL_TL4 0x1
#define	NIX_TXSCH_LVL_TL3 0x2
#define	NIX_TXSCH_LVL_TL2 0x3
#define	NIX_TXSCH_LVL_TL1 0x4
#define	NIX_TXSCH_LVL_CNT 0x5

/*
 * Number of resources available to the caller.
 * In reply to MBOX_MSG_FREE_RSRC_CNT.
 */
struct free_rsrcs_rsp {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io schq[NIX_TXSCH_LVL_CNT];
	uint16_t __otx2_io sso;
	uint16_t __otx2_io tim;
	uint16_t __otx2_io ssow;
	uint16_t __otx2_io cpt;
	uint8_t __otx2_io npa;
	uint8_t __otx2_io nix;
	uint16_t  __otx2_io schq_nix1[NIX_TXSCH_LVL_CNT];
	uint8_t  __otx2_io nix1;
	uint8_t  __otx2_io cpt1;
	uint8_t  __otx2_io ree0;
	uint8_t  __otx2_io ree1;
};

#define MSIX_VECTOR_INVALID	0xFFFF
#define MAX_RVU_BLKLF_CNT	256

struct msix_offset_rsp {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io npa_msixoff;
	uint16_t __otx2_io nix_msixoff;
	uint16_t __otx2_io sso;
	uint16_t __otx2_io ssow;
	uint16_t __otx2_io timlfs;
	uint16_t __otx2_io cptlfs;
	uint16_t __otx2_io sso_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __otx2_io ssow_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __otx2_io timlf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __otx2_io cptlf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __otx2_io cpt1_lfs;
	uint16_t __otx2_io ree0_lfs;
	uint16_t __otx2_io ree1_lfs;
	uint16_t __otx2_io cpt1_lf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __otx2_io ree0_lf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __otx2_io ree1_lf_msixoff[MAX_RVU_BLKLF_CNT];

};

/* CGX mbox message formats */

struct cgx_stats_rsp {
	struct mbox_msghdr hdr;
#define CGX_RX_STATS_COUNT	13
#define CGX_TX_STATS_COUNT	18
	uint64_t __otx2_io rx_stats[CGX_RX_STATS_COUNT];
	uint64_t __otx2_io tx_stats[CGX_TX_STATS_COUNT];
};

struct cgx_fec_stats_rsp {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io fec_corr_blks;
	uint64_t __otx2_io fec_uncorr_blks;
};
/* Structure for requesting the operation for
 * setting/getting mac address in the CGX interface
 */
struct cgx_mac_addr_set_or_get {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io mac_addr[RTE_ETHER_ADDR_LEN];
};

/* Structure for requesting the operation to
 * add DMAC filter entry into CGX interface
 */
struct cgx_mac_addr_add_req {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io mac_addr[RTE_ETHER_ADDR_LEN];
};

/* Structure for response against the operation to
 * add DMAC filter entry into CGX interface
 */
struct cgx_mac_addr_add_rsp {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io index;
};

/* Structure for requesting the operation to
 * delete DMAC filter entry from CGX interface
 */
struct cgx_mac_addr_del_req {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io index;
};

/* Structure for response against the operation to
 * get maximum supported DMAC filter entries
 */
struct cgx_max_dmac_entries_get_rsp {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io max_dmac_filters;
};

struct cgx_link_user_info {
	uint64_t __otx2_io link_up:1;
	uint64_t __otx2_io full_duplex:1;
	uint64_t __otx2_io lmac_type_id:4;
	uint64_t __otx2_io speed:20; /* speed in Mbps */
	uint64_t __otx2_io an:1; /* AN supported or not */
	uint64_t __otx2_io fec:2; /* FEC type if enabled else 0 */
	uint64_t __otx2_io port:8;
#define LMACTYPE_STR_LEN 16
	char lmac_type[LMACTYPE_STR_LEN];
};

struct cgx_link_info_msg {
	struct mbox_msghdr hdr;
	struct cgx_link_user_info link_info;
};

struct cgx_ptp_rx_info_msg {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io ptp_en;
};

struct cgx_pause_frm_cfg {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io set;
	/* set = 1 if the request is to config pause frames */
	/* set = 0 if the request is to fetch pause frames config */
	uint8_t __otx2_io rx_pause;
	uint8_t __otx2_io tx_pause;
};

struct sfp_eeprom_s {
#define SFP_EEPROM_SIZE 256
	uint16_t __otx2_io sff_id;
	uint8_t __otx2_io buf[SFP_EEPROM_SIZE];
	uint64_t __otx2_io reserved;
};

enum fec_type {
	OTX2_FEC_NONE,
	OTX2_FEC_BASER,
	OTX2_FEC_RS,
};

struct phy_s {
	uint64_t __otx2_io can_change_mod_type : 1;
	uint64_t __otx2_io mod_type            : 1;
};

struct cgx_lmac_fwdata_s {
	uint16_t __otx2_io rw_valid;
	uint64_t __otx2_io supported_fec;
	uint64_t __otx2_io supported_an;
	uint64_t __otx2_io supported_link_modes;
	/* Only applicable if AN is supported */
	uint64_t __otx2_io advertised_fec;
	uint64_t __otx2_io advertised_link_modes;
	/* Only applicable if SFP/QSFP slot is present */
	struct sfp_eeprom_s sfp_eeprom;
	struct phy_s phy;
#define LMAC_FWDATA_RESERVED_MEM 1023
	uint64_t __otx2_io reserved[LMAC_FWDATA_RESERVED_MEM];
};

struct cgx_fw_data {
	struct mbox_msghdr hdr;
	struct cgx_lmac_fwdata_s fwdata;
};

struct fec_mode {
	struct mbox_msghdr hdr;
	int __otx2_io fec;
};

struct cgx_set_link_state_msg {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io enable;
};

struct cgx_phy_mod_type {
	struct mbox_msghdr hdr;
	int __otx2_io mod;
};

struct cgx_set_link_mode_args {
	uint32_t __otx2_io speed;
	uint8_t __otx2_io duplex;
	uint8_t __otx2_io an;
	uint8_t __otx2_io ports;
	uint64_t __otx2_io mode;
};

struct cgx_set_link_mode_req {
	struct mbox_msghdr hdr;
	struct cgx_set_link_mode_args args;
};

struct cgx_set_link_mode_rsp {
	struct mbox_msghdr hdr;
	int __otx2_io status;
};
/* NPA mbox message formats */

/* NPA mailbox error codes
 * Range 301 - 400.
 */
enum npa_af_status {
	NPA_AF_ERR_PARAM            = -301,
	NPA_AF_ERR_AQ_FULL          = -302,
	NPA_AF_ERR_AQ_ENQUEUE       = -303,
	NPA_AF_ERR_AF_LF_INVALID    = -304,
	NPA_AF_ERR_AF_LF_ALLOC      = -305,
	NPA_AF_ERR_LF_RESET         = -306,
};

#define NPA_AURA_SZ_0		0
#define NPA_AURA_SZ_128		1
#define	NPA_AURA_SZ_256		2
#define	NPA_AURA_SZ_512		3
#define	NPA_AURA_SZ_1K		4
#define	NPA_AURA_SZ_2K		5
#define	NPA_AURA_SZ_4K		6
#define	NPA_AURA_SZ_8K		7
#define	NPA_AURA_SZ_16K		8
#define	NPA_AURA_SZ_32K		9
#define	NPA_AURA_SZ_64K		10
#define	NPA_AURA_SZ_128K	11
#define	NPA_AURA_SZ_256K	12
#define	NPA_AURA_SZ_512K	13
#define	NPA_AURA_SZ_1M		14
#define	NPA_AURA_SZ_MAX		15

/* For NPA LF context alloc and init */
struct npa_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __otx2_io node;
	int __otx2_io aura_sz; /* No of auras. See NPA_AURA_SZ_* */
	uint32_t __otx2_io nr_pools; /* No of pools */
	uint64_t __otx2_io way_mask;
};

struct npa_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io stack_pg_ptrs;  /* No of ptrs per stack page */
	uint32_t __otx2_io stack_pg_bytes; /* Size of stack page */
	uint16_t __otx2_io qints; /* NPA_AF_CONST::QINTS */
};

/* NPA AQ enqueue msg */
struct npa_aq_enq_req {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io aura_id;
	uint8_t __otx2_io ctype;
	uint8_t __otx2_io op;
	union {
		/* Valid when op == WRITE/INIT and ctype == AURA.
		 * LF fills the pool_id in aura.pool_addr. AF will translate
		 * the pool_id to pool context pointer.
		 */
		__otx2_io struct npa_aura_s aura;
		/* Valid when op == WRITE/INIT and ctype == POOL */
		__otx2_io struct npa_pool_s pool;
	};
	/* Mask data when op == WRITE (1=write, 0=don't write) */
	union {
		/* Valid when op == WRITE and ctype == AURA */
		__otx2_io struct npa_aura_s aura_mask;
		/* Valid when op == WRITE and ctype == POOL */
		__otx2_io struct npa_pool_s pool_mask;
	};
};

struct npa_aq_enq_rsp {
	struct mbox_msghdr hdr;
	union {
		/* Valid when op == READ and ctype == AURA */
		__otx2_io struct npa_aura_s aura;
		/* Valid when op == READ and ctype == POOL */
		__otx2_io struct npa_pool_s pool;
	};
};

/* Disable all contexts of type 'ctype' */
struct hwctx_disable_req {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io ctype;
};

/* NIX mbox message formats */

/* NIX mailbox error codes
 * Range 401 - 500.
 */
enum nix_af_status {
	NIX_AF_ERR_PARAM            = -401,
	NIX_AF_ERR_AQ_FULL          = -402,
	NIX_AF_ERR_AQ_ENQUEUE       = -403,
	NIX_AF_ERR_AF_LF_INVALID    = -404,
	NIX_AF_ERR_AF_LF_ALLOC      = -405,
	NIX_AF_ERR_TLX_ALLOC_FAIL   = -406,
	NIX_AF_ERR_TLX_INVALID      = -407,
	NIX_AF_ERR_RSS_SIZE_INVALID = -408,
	NIX_AF_ERR_RSS_GRPS_INVALID = -409,
	NIX_AF_ERR_FRS_INVALID      = -410,
	NIX_AF_ERR_RX_LINK_INVALID  = -411,
	NIX_AF_INVAL_TXSCHQ_CFG     = -412,
	NIX_AF_SMQ_FLUSH_FAILED     = -413,
	NIX_AF_ERR_LF_RESET         = -414,
	NIX_AF_ERR_RSS_NOSPC_FIELD  = -415,
	NIX_AF_ERR_RSS_NOSPC_ALGO   = -416,
	NIX_AF_ERR_MARK_CFG_FAIL    = -417,
	NIX_AF_ERR_LSO_CFG_FAIL     = -418,
	NIX_AF_INVAL_NPA_PF_FUNC    = -419,
	NIX_AF_INVAL_SSO_PF_FUNC    = -420,
	NIX_AF_ERR_TX_VTAG_NOSPC    = -421,
	NIX_AF_ERR_RX_VTAG_INUSE    = -422,
	NIX_AF_ERR_PTP_CONFIG_FAIL  = -423,
};

/* For NIX LF context alloc and init */
struct nix_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __otx2_io node;
	uint32_t __otx2_io rq_cnt;   /* No of receive queues */
	uint32_t __otx2_io sq_cnt;   /* No of send queues */
	uint32_t __otx2_io cq_cnt;   /* No of completion queues */
	uint8_t __otx2_io xqe_sz;
	uint16_t __otx2_io rss_sz;
	uint8_t __otx2_io rss_grps;
	uint16_t __otx2_io npa_func;
	/* RVU_DEFAULT_PF_FUNC == default pf_func associated with lf */
	uint16_t __otx2_io sso_func;
	uint64_t __otx2_io rx_cfg;   /* See NIX_AF_LF(0..127)_RX_CFG */
	uint64_t __otx2_io way_mask;
#define NIX_LF_RSS_TAG_LSB_AS_ADDER BIT_ULL(0)
	uint64_t flags;
};

struct nix_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io sqb_size;
	uint16_t __otx2_io rx_chan_base;
	uint16_t __otx2_io tx_chan_base;
	uint8_t __otx2_io rx_chan_cnt; /* Total number of RX channels */
	uint8_t __otx2_io tx_chan_cnt; /* Total number of TX channels */
	uint8_t __otx2_io lso_tsov4_idx;
	uint8_t __otx2_io lso_tsov6_idx;
	uint8_t __otx2_io mac_addr[RTE_ETHER_ADDR_LEN];
	uint8_t __otx2_io lf_rx_stats; /* NIX_AF_CONST1::LF_RX_STATS */
	uint8_t __otx2_io lf_tx_stats; /* NIX_AF_CONST1::LF_TX_STATS */
	uint16_t __otx2_io cints; /* NIX_AF_CONST2::CINTS */
	uint16_t __otx2_io qints; /* NIX_AF_CONST2::QINTS */
	uint8_t __otx2_io hw_rx_tstamp_en; /*set if rx timestamping enabled */
	uint8_t __otx2_io cgx_links;  /* No. of CGX links present in HW */
	uint8_t __otx2_io lbk_links;  /* No. of LBK links present in HW */
	uint8_t __otx2_io sdp_links;  /* No. of SDP links present in HW */
	uint8_t __otx2_io tx_link;    /* Transmit channel link number */
};

struct nix_lf_free_req {
	struct mbox_msghdr hdr;
#define NIX_LF_DISABLE_FLOWS		BIT_ULL(0)
#define NIX_LF_DONT_FREE_TX_VTAG	BIT_ULL(1)
	uint64_t __otx2_io flags;
};

/* NIX AQ enqueue msg */
struct nix_aq_enq_req {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io qidx;
	uint8_t __otx2_io ctype;
	uint8_t __otx2_io op;
	union {
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RQ */
		__otx2_io struct nix_rq_ctx_s rq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_SQ */
		__otx2_io struct nix_sq_ctx_s sq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_CQ */
		__otx2_io struct nix_cq_ctx_s cq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RSS */
		__otx2_io struct nix_rsse_s rss;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_MCE */
		__otx2_io struct nix_rx_mce_s mce;
	};
	/* Mask data when op == WRITE (1=write, 0=don't write) */
	union {
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RQ */
		__otx2_io struct nix_rq_ctx_s rq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_SQ */
		__otx2_io struct nix_sq_ctx_s sq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_CQ */
		__otx2_io struct nix_cq_ctx_s cq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RSS */
		__otx2_io struct nix_rsse_s rss_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_MCE */
		__otx2_io struct nix_rx_mce_s mce_mask;
	};
};

struct nix_aq_enq_rsp {
	struct mbox_msghdr hdr;
	union {
		__otx2_io struct nix_rq_ctx_s rq;
		__otx2_io struct nix_sq_ctx_s sq;
		__otx2_io struct nix_cq_ctx_s cq;
		__otx2_io struct nix_rsse_s   rss;
		__otx2_io struct nix_rx_mce_s mce;
	};
};

/* Tx scheduler/shaper mailbox messages */

#define MAX_TXSCHQ_PER_FUNC	128

struct nix_txsch_alloc_req {
	struct mbox_msghdr hdr;
	/* Scheduler queue count request at each level */
	uint16_t __otx2_io schq_contig[NIX_TXSCH_LVL_CNT]; /* Contig. queues */
	uint16_t __otx2_io schq[NIX_TXSCH_LVL_CNT]; /* Non-Contig. queues */
};

struct nix_txsch_alloc_rsp {
	struct mbox_msghdr hdr;
	/* Scheduler queue count allocated at each level */
	uint16_t __otx2_io schq_contig[NIX_TXSCH_LVL_CNT]; /* Contig. queues */
	uint16_t __otx2_io schq[NIX_TXSCH_LVL_CNT]; /* Non-Contig. queues */
	/* Scheduler queue list allocated at each level */
	uint16_t __otx2_io
		schq_contig_list[NIX_TXSCH_LVL_CNT][MAX_TXSCHQ_PER_FUNC];
	uint16_t __otx2_io schq_list[NIX_TXSCH_LVL_CNT][MAX_TXSCHQ_PER_FUNC];
	/* Traffic aggregation scheduler level */
	uint8_t  __otx2_io aggr_level;
	/* Aggregation lvl's RR_PRIO config */
	uint8_t  __otx2_io aggr_lvl_rr_prio;
	/* LINKX_CFG CSRs mapped to TL3 or TL2's index ? */
	uint8_t  __otx2_io link_cfg_lvl;
};

struct nix_txsch_free_req {
	struct mbox_msghdr hdr;
#define TXSCHQ_FREE_ALL BIT_ULL(0)
	uint16_t __otx2_io flags;
	/* Scheduler queue level to be freed */
	uint16_t __otx2_io schq_lvl;
	/* List of scheduler queues to be freed */
	uint16_t __otx2_io schq;
};

struct nix_txschq_config {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io lvl; /* SMQ/MDQ/TL4/TL3/TL2/TL1 */
	uint8_t __otx2_io read;
#define TXSCHQ_IDX_SHIFT 16
#define TXSCHQ_IDX_MASK (BIT_ULL(10) - 1)
#define TXSCHQ_IDX(reg, shift) (((reg) >> (shift)) & TXSCHQ_IDX_MASK)
	uint8_t __otx2_io num_regs;
#define MAX_REGS_PER_MBOX_MSG 20
	uint64_t __otx2_io reg[MAX_REGS_PER_MBOX_MSG];
	uint64_t __otx2_io regval[MAX_REGS_PER_MBOX_MSG];
	/* All 0's => overwrite with new value */
	uint64_t __otx2_io regval_mask[MAX_REGS_PER_MBOX_MSG];
};

struct nix_vtag_config {
	struct mbox_msghdr hdr;
	/* '0' for 4 octet VTAG, '1' for 8 octet VTAG */
	uint8_t __otx2_io vtag_size;
	/* cfg_type is '0' for tx vlan cfg
	 * cfg_type is '1' for rx vlan cfg
	 */
	uint8_t __otx2_io cfg_type;
	union {
		/* Valid when cfg_type is '0' */
		struct {
			uint64_t __otx2_io vtag0;
			uint64_t __otx2_io vtag1;

			/* cfg_vtag0 & cfg_vtag1 fields are valid
			 * when free_vtag0 & free_vtag1 are '0's.
			 */
			/* cfg_vtag0 = 1 to configure vtag0 */
			uint8_t __otx2_io cfg_vtag0 :1;
			/* cfg_vtag1 = 1 to configure vtag1 */
			uint8_t __otx2_io cfg_vtag1 :1;

			/* vtag0_idx & vtag1_idx are only valid when
			 * both cfg_vtag0 & cfg_vtag1 are '0's,
			 * these fields are used along with free_vtag0
			 * & free_vtag1 to free the nix lf's tx_vlan
			 * configuration.
			 *
			 * Denotes the indices of tx_vtag def registers
			 * that needs to be cleared and freed.
			 */
			int __otx2_io vtag0_idx;
			int __otx2_io vtag1_idx;

			/* Free_vtag0 & free_vtag1 fields are valid
			 * when cfg_vtag0 & cfg_vtag1 are '0's.
			 */
			/* Free_vtag0 = 1 clears vtag0 configuration
			 * vtag0_idx denotes the index to be cleared.
			 */
			uint8_t __otx2_io free_vtag0 :1;
			/* Free_vtag1 = 1 clears vtag1 configuration
			 * vtag1_idx denotes the index to be cleared.
			 */
			uint8_t __otx2_io free_vtag1 :1;
		} tx;

		/* Valid when cfg_type is '1' */
		struct {
			/* Rx vtag type index, valid values are in 0..7 range */
			uint8_t __otx2_io vtag_type;
			/* Rx vtag strip */
			uint8_t __otx2_io strip_vtag :1;
			/* Rx vtag capture */
			uint8_t __otx2_io capture_vtag :1;
		} rx;
	};
};

struct nix_vtag_config_rsp {
	struct mbox_msghdr hdr;
	/* Indices of tx_vtag def registers used to configure
	 * tx vtag0 & vtag1 headers, these indices are valid
	 * when nix_vtag_config mbox requested for vtag0 and/
	 * or vtag1 configuration.
	 */
	int __otx2_io vtag0_idx;
	int __otx2_io vtag1_idx;
};

struct nix_rss_flowkey_cfg {
	struct mbox_msghdr hdr;
	int __otx2_io mcam_index;  /* MCAM entry index to modify */
	uint32_t __otx2_io flowkey_cfg; /* Flowkey types selected */
#define FLOW_KEY_TYPE_PORT     BIT(0)
#define FLOW_KEY_TYPE_IPV4     BIT(1)
#define FLOW_KEY_TYPE_IPV6     BIT(2)
#define FLOW_KEY_TYPE_TCP      BIT(3)
#define FLOW_KEY_TYPE_UDP      BIT(4)
#define FLOW_KEY_TYPE_SCTP     BIT(5)
#define FLOW_KEY_TYPE_NVGRE    BIT(6)
#define FLOW_KEY_TYPE_VXLAN    BIT(7)
#define FLOW_KEY_TYPE_GENEVE   BIT(8)
#define FLOW_KEY_TYPE_ETH_DMAC BIT(9)
#define FLOW_KEY_TYPE_IPV6_EXT BIT(10)
#define FLOW_KEY_TYPE_GTPU       BIT(11)
#define FLOW_KEY_TYPE_INNR_IPV4     BIT(12)
#define FLOW_KEY_TYPE_INNR_IPV6     BIT(13)
#define FLOW_KEY_TYPE_INNR_TCP      BIT(14)
#define FLOW_KEY_TYPE_INNR_UDP      BIT(15)
#define FLOW_KEY_TYPE_INNR_SCTP     BIT(16)
#define FLOW_KEY_TYPE_INNR_ETH_DMAC BIT(17)
#define FLOW_KEY_TYPE_CH_LEN_90B	BIT(18)
#define FLOW_KEY_TYPE_CUSTOM0		BIT(19)
#define FLOW_KEY_TYPE_VLAN		BIT(20)
#define FLOW_KEY_TYPE_L4_DST BIT(28)
#define FLOW_KEY_TYPE_L4_SRC BIT(29)
#define FLOW_KEY_TYPE_L3_DST BIT(30)
#define FLOW_KEY_TYPE_L3_SRC BIT(31)
	uint8_t	__otx2_io group;       /* RSS context or group */
};

struct nix_rss_flowkey_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io alg_idx; /* Selected algo index */
};

struct nix_set_mac_addr {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io mac_addr[RTE_ETHER_ADDR_LEN];
};

struct nix_get_mac_addr_rsp {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io mac_addr[RTE_ETHER_ADDR_LEN];
};

struct nix_mark_format_cfg {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io offset;
	uint8_t __otx2_io y_mask;
	uint8_t __otx2_io y_val;
	uint8_t __otx2_io r_mask;
	uint8_t __otx2_io r_val;
};

struct nix_mark_format_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io mark_format_idx;
};

struct nix_lso_format_cfg {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io field_mask;
	uint64_t __otx2_io fields[NIX_LSO_FIELD_MAX];
};

struct nix_lso_format_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io lso_format_idx;
};

struct nix_rx_mode {
	struct mbox_msghdr hdr;
#define NIX_RX_MODE_UCAST    BIT(0)
#define NIX_RX_MODE_PROMISC  BIT(1)
#define NIX_RX_MODE_ALLMULTI BIT(2)
	uint16_t __otx2_io mode;
};

struct nix_rx_cfg {
	struct mbox_msghdr hdr;
#define NIX_RX_OL3_VERIFY   BIT(0)
#define NIX_RX_OL4_VERIFY   BIT(1)
	uint8_t __otx2_io len_verify; /* Outer L3/L4 len check */
#define NIX_RX_CSUM_OL4_VERIFY  BIT(0)
	uint8_t __otx2_io csum_verify; /* Outer L4 checksum verification */
};

struct nix_frs_cfg {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io update_smq;    /* Update SMQ's min/max lens */
	uint8_t __otx2_io update_minlen; /* Set minlen also */
	uint8_t __otx2_io sdp_link;      /* Set SDP RX link */
	uint16_t __otx2_io maxlen;
	uint16_t __otx2_io minlen;
};

struct nix_set_vlan_tpid {
	struct mbox_msghdr hdr;
#define NIX_VLAN_TYPE_INNER 0
#define NIX_VLAN_TYPE_OUTER 1
	uint8_t __otx2_io vlan_type;
	uint16_t __otx2_io tpid;
};

struct nix_bp_cfg_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io chan_base; /* Starting channel number */
	uint8_t __otx2_io chan_cnt; /* Number of channels */
	uint8_t __otx2_io bpid_per_chan;
	/* bpid_per_chan = 0  assigns single bp id for range of channels */
	/* bpid_per_chan = 1 assigns separate bp id for each channel */
};

/* PF can be mapped to either CGX or LBK interface,
 * so maximum 64 channels are possible.
 */
#define NIX_MAX_CHAN	64
struct nix_bp_cfg_rsp {
	struct mbox_msghdr hdr;
	/* Channel and bpid mapping */
	uint16_t __otx2_io chan_bpid[NIX_MAX_CHAN];
	/* Number of channel for which bpids are assigned */
	uint8_t __otx2_io chan_cnt;
};

/* Global NIX inline IPSec configuration */
struct nix_inline_ipsec_cfg {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io cpt_credit;
	struct {
		uint8_t __otx2_io egrp;
		uint8_t __otx2_io opcode;
	} gen_cfg;
	struct {
		uint16_t __otx2_io cpt_pf_func;
		uint8_t __otx2_io cpt_slot;
	} inst_qsel;
	uint8_t __otx2_io enable;
};

/* Per NIX LF inline IPSec configuration */
struct nix_inline_ipsec_lf_cfg {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io sa_base_addr;
	struct {
		uint32_t __otx2_io tag_const;
		uint16_t __otx2_io lenm1_max;
		uint8_t __otx2_io sa_pow2_size;
		uint8_t __otx2_io tt;
	} ipsec_cfg0;
	struct {
		uint32_t __otx2_io sa_idx_max;
		uint8_t __otx2_io sa_idx_w;
	} ipsec_cfg1;
	uint8_t __otx2_io enable;
};

/* SSO mailbox error codes
 * Range 501 - 600.
 */
enum sso_af_status {
	SSO_AF_ERR_PARAM	= -501,
	SSO_AF_ERR_LF_INVALID	= -502,
	SSO_AF_ERR_AF_LF_ALLOC	= -503,
	SSO_AF_ERR_GRP_EBUSY	= -504,
	SSO_AF_INVAL_NPA_PF_FUNC = -505,
};

struct sso_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __otx2_io node;
	uint16_t __otx2_io hwgrps;
};

struct sso_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io xaq_buf_size;
	uint32_t __otx2_io xaq_wq_entries;
	uint32_t __otx2_io in_unit_entries;
	uint16_t __otx2_io hwgrps;
};

struct sso_lf_free_req {
	struct mbox_msghdr hdr;
	int __otx2_io node;
	uint16_t __otx2_io hwgrps;
};

/* SSOW mailbox error codes
 * Range 601 - 700.
 */
enum ssow_af_status {
	SSOW_AF_ERR_PARAM	= -601,
	SSOW_AF_ERR_LF_INVALID	= -602,
	SSOW_AF_ERR_AF_LF_ALLOC	= -603,
};

struct ssow_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __otx2_io node;
	uint16_t __otx2_io hws;
};

struct ssow_lf_free_req {
	struct mbox_msghdr hdr;
	int __otx2_io node;
	uint16_t __otx2_io hws;
};

struct sso_hw_setconfig {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io npa_aura_id;
	uint16_t __otx2_io npa_pf_func;
	uint16_t __otx2_io hwgrps;
};

struct sso_release_xaq {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io hwgrps;
};

struct sso_info_req {
	struct mbox_msghdr hdr;
	union {
		uint16_t __otx2_io grp;
		uint16_t __otx2_io hws;
	};
};

struct sso_grp_priority {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io grp;
	uint8_t __otx2_io priority;
	uint8_t __otx2_io affinity;
	uint8_t __otx2_io weight;
};

struct sso_grp_qos_cfg {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io grp;
	uint32_t __otx2_io xaq_limit;
	uint16_t __otx2_io taq_thr;
	uint16_t __otx2_io iaq_thr;
};

struct sso_grp_stats {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io grp;
	uint64_t __otx2_io ws_pc;
	uint64_t __otx2_io ext_pc;
	uint64_t __otx2_io wa_pc;
	uint64_t __otx2_io ts_pc;
	uint64_t __otx2_io ds_pc;
	uint64_t __otx2_io dq_pc;
	uint64_t __otx2_io aw_status;
	uint64_t __otx2_io page_cnt;
};

struct sso_hws_stats {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io hws;
	uint64_t __otx2_io arbitration;
};

/* CPT mailbox error codes
 * Range 901 - 1000.
 */
enum cpt_af_status {
	CPT_AF_ERR_PARAM		= -901,
	CPT_AF_ERR_GRP_INVALID		= -902,
	CPT_AF_ERR_LF_INVALID		= -903,
	CPT_AF_ERR_ACCESS_DENIED	= -904,
	CPT_AF_ERR_SSO_PF_FUNC_INVALID	= -905,
	CPT_AF_ERR_NIX_PF_FUNC_INVALID	= -906,
	CPT_AF_ERR_INLINE_IPSEC_INB_ENA	= -907,
	CPT_AF_ERR_INLINE_IPSEC_OUT_ENA	= -908
};

/* CPT mbox message formats */

struct cpt_rd_wr_reg_msg {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io reg_offset;
	uint64_t __otx2_io *ret_val;
	uint64_t __otx2_io val;
	uint8_t __otx2_io is_write;
	/* BLKADDR_CPT0/BLKADDR_CPT1 or 0 for BLKADDR_CPT0 */
	uint8_t __otx2_io blkaddr;
};

struct cpt_set_crypto_grp_req_msg {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io crypto_eng_grp;
};

struct cpt_lf_alloc_req_msg {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io nix_pf_func;
	uint16_t __otx2_io sso_pf_func;
	uint16_t __otx2_io eng_grpmask;
	/* BLKADDR_CPT0/BLKADDR_CPT1 or 0 for BLKADDR_CPT0 */
	uint8_t __otx2_io blkaddr;
};

struct cpt_lf_alloc_rsp_msg {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io eng_grpmsk;
};

#define CPT_INLINE_INBOUND	0
#define CPT_INLINE_OUTBOUND	1

struct cpt_inline_ipsec_cfg_msg {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io enable;
	uint8_t __otx2_io slot;
	uint8_t __otx2_io dir;
	uint16_t __otx2_io sso_pf_func; /* Inbound path SSO_PF_FUNC */
	uint16_t __otx2_io nix_pf_func; /* Outbound path NIX_PF_FUNC */
};

struct cpt_rx_inline_lf_cfg_msg {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io sso_pf_func;
};

enum cpt_eng_type {
	CPT_ENG_TYPE_AE = 1,
	CPT_ENG_TYPE_SE = 2,
	CPT_ENG_TYPE_IE = 3,
	CPT_MAX_ENG_TYPES,
};

/* CPT HW capabilities */
union cpt_eng_caps {
	uint64_t __otx2_io u;
	struct {
		uint64_t __otx2_io reserved_0_4:5;
		uint64_t __otx2_io mul:1;
		uint64_t __otx2_io sha1_sha2:1;
		uint64_t __otx2_io chacha20:1;
		uint64_t __otx2_io zuc_snow3g:1;
		uint64_t __otx2_io sha3:1;
		uint64_t __otx2_io aes:1;
		uint64_t __otx2_io kasumi:1;
		uint64_t __otx2_io des:1;
		uint64_t __otx2_io crc:1;
		uint64_t __otx2_io reserved_14_63:50;
	};
};

struct cpt_caps_rsp_msg {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io cpt_pf_drv_version;
	uint8_t __otx2_io cpt_revision;
	union cpt_eng_caps eng_caps[CPT_MAX_ENG_TYPES];
};

/* NPC mbox message structs */

#define NPC_MCAM_ENTRY_INVALID	0xFFFF
#define NPC_MCAM_INVALID_MAP	0xFFFF

/* NPC mailbox error codes
 * Range 701 - 800.
 */
enum npc_af_status {
	NPC_MCAM_INVALID_REQ	= -701,
	NPC_MCAM_ALLOC_DENIED	= -702,
	NPC_MCAM_ALLOC_FAILED	= -703,
	NPC_MCAM_PERM_DENIED	= -704,
	NPC_AF_ERR_HIGIG_CONFIG_FAIL	= -705,
};

struct npc_mcam_alloc_entry_req {
	struct mbox_msghdr hdr;
#define NPC_MAX_NONCONTIG_ENTRIES	256
	uint8_t __otx2_io contig;   /* Contiguous entries ? */
#define NPC_MCAM_ANY_PRIO		0
#define NPC_MCAM_LOWER_PRIO		1
#define NPC_MCAM_HIGHER_PRIO		2
	uint8_t __otx2_io priority; /* Lower or higher w.r.t ref_entry */
	uint16_t __otx2_io ref_entry;
	uint16_t __otx2_io count;    /* Number of entries requested */
};

struct npc_mcam_alloc_entry_rsp {
	struct mbox_msghdr hdr;
	/* Entry alloc'ed or start index if contiguous.
	 * Invalid in case of non-contiguous.
	 */
	uint16_t __otx2_io entry;
	uint16_t __otx2_io count; /* Number of entries allocated */
	uint16_t __otx2_io free_count; /* Number of entries available */
	uint16_t __otx2_io entry_list[NPC_MAX_NONCONTIG_ENTRIES];
};

struct npc_mcam_free_entry_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io entry; /* Entry index to be freed */
	uint8_t __otx2_io all;   /* Free all entries alloc'ed to this PFVF */
};

struct mcam_entry {
#define NPC_MAX_KWS_IN_KEY	7 /* Number of keywords in max key width */
	uint64_t __otx2_io kw[NPC_MAX_KWS_IN_KEY];
	uint64_t __otx2_io kw_mask[NPC_MAX_KWS_IN_KEY];
	uint64_t __otx2_io action;
	uint64_t __otx2_io vtag_action;
};

struct npc_mcam_write_entry_req {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
	uint16_t __otx2_io entry; /* MCAM entry to write this match key */
	uint16_t __otx2_io cntr;	 /* Counter for this MCAM entry */
	uint8_t __otx2_io intf;	 /* Rx or Tx interface */
	uint8_t __otx2_io enable_entry;/* Enable this MCAM entry ? */
	uint8_t __otx2_io set_cntr;    /* Set counter for this entry ? */
};

/* Enable/Disable a given entry */
struct npc_mcam_ena_dis_entry_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io entry;
};

struct npc_mcam_shift_entry_req {
	struct mbox_msghdr hdr;
#define NPC_MCAM_MAX_SHIFTS	64
	uint16_t __otx2_io curr_entry[NPC_MCAM_MAX_SHIFTS];
	uint16_t __otx2_io new_entry[NPC_MCAM_MAX_SHIFTS];
	uint16_t __otx2_io shift_count; /* Number of entries to shift */
};

struct npc_mcam_shift_entry_rsp {
	struct mbox_msghdr hdr;
	/* Index in 'curr_entry', not entry itself */
	uint16_t __otx2_io failed_entry_idx;
};

struct npc_mcam_alloc_counter_req {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io contig;	/* Contiguous counters ? */
#define NPC_MAX_NONCONTIG_COUNTERS 64
	uint16_t __otx2_io count;	/* Number of counters requested */
};

struct npc_mcam_alloc_counter_rsp {
	struct mbox_msghdr hdr;
	/* Counter alloc'ed or start idx if contiguous.
	 * Invalid incase of non-contiguous.
	 */
	uint16_t __otx2_io cntr;
	uint16_t __otx2_io count; /* Number of counters allocated */
	uint16_t __otx2_io cntr_list[NPC_MAX_NONCONTIG_COUNTERS];
};

struct npc_mcam_oper_counter_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io cntr; /* Free a counter or clear/fetch it's stats */
};

struct npc_mcam_oper_counter_rsp {
	struct mbox_msghdr hdr;
	/* valid only while fetching counter's stats */
	uint64_t __otx2_io stat;
};

struct npc_mcam_unmap_counter_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io cntr;
	uint16_t __otx2_io entry; /* Entry and counter to be unmapped */
	uint8_t __otx2_io all;   /* Unmap all entries using this counter ? */
};

struct npc_mcam_alloc_and_write_entry_req {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
	uint16_t __otx2_io ref_entry;
	uint8_t __otx2_io priority;    /* Lower or higher w.r.t ref_entry */
	uint8_t __otx2_io intf;	 /* Rx or Tx interface */
	uint8_t __otx2_io enable_entry;/* Enable this MCAM entry ? */
	uint8_t __otx2_io alloc_cntr;  /* Allocate counter and map ? */
};

struct npc_mcam_alloc_and_write_entry_rsp {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io entry;
	uint16_t __otx2_io cntr;
};

struct npc_get_kex_cfg_rsp {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io rx_keyx_cfg;   /* NPC_AF_INTF(0)_KEX_CFG */
	uint64_t __otx2_io tx_keyx_cfg;   /* NPC_AF_INTF(1)_KEX_CFG */
#define NPC_MAX_INTF	2
#define NPC_MAX_LID	8
#define NPC_MAX_LT	16
#define NPC_MAX_LD	2
#define NPC_MAX_LFL	16
	/* NPC_AF_KEX_LDATA(0..1)_FLAGS_CFG */
	uint64_t __otx2_io kex_ld_flags[NPC_MAX_LD];
	/* NPC_AF_INTF(0..1)_LID(0..7)_LT(0..15)_LD(0..1)_CFG */
	uint64_t __otx2_io
	intf_lid_lt_ld[NPC_MAX_INTF][NPC_MAX_LID][NPC_MAX_LT][NPC_MAX_LD];
	/* NPC_AF_INTF(0..1)_LDATA(0..1)_FLAGS(0..15)_CFG */
	uint64_t __otx2_io
	intf_ld_flags[NPC_MAX_INTF][NPC_MAX_LD][NPC_MAX_LFL];
#define MKEX_NAME_LEN 128
	uint8_t __otx2_io mkex_pfl_name[MKEX_NAME_LEN];
};

enum header_fields {
	NPC_DMAC,
	NPC_SMAC,
	NPC_ETYPE,
	NPC_OUTER_VID,
	NPC_TOS,
	NPC_SIP_IPV4,
	NPC_DIP_IPV4,
	NPC_SIP_IPV6,
	NPC_DIP_IPV6,
	NPC_SPORT_TCP,
	NPC_DPORT_TCP,
	NPC_SPORT_UDP,
	NPC_DPORT_UDP,
	NPC_FDSA_VAL,
	NPC_HEADER_FIELDS_MAX,
};

struct flow_msg {
	unsigned char __otx2_io dmac[6];
	unsigned char __otx2_io smac[6];
	uint16_t __otx2_io etype;
	uint16_t __otx2_io vlan_etype;
	uint16_t __otx2_io vlan_tci;
	union {
		uint32_t __otx2_io ip4src;
		uint32_t __otx2_io ip6src[4];
	};
	union {
		uint32_t __otx2_io ip4dst;
		uint32_t __otx2_io ip6dst[4];
	};
	uint8_t __otx2_io tos;
	uint8_t __otx2_io ip_ver;
	uint8_t __otx2_io ip_proto;
	uint8_t __otx2_io tc;
	uint16_t __otx2_io sport;
	uint16_t __otx2_io dport;
};

struct npc_install_flow_req {
	struct mbox_msghdr hdr;
	struct flow_msg packet;
	struct flow_msg mask;
	uint64_t __otx2_io features;
	uint16_t __otx2_io entry;
	uint16_t __otx2_io channel;
	uint8_t __otx2_io intf;
	uint8_t __otx2_io set_cntr;
	uint8_t __otx2_io default_rule;
	/* Overwrite(0) or append(1) flow to default rule? */
	uint8_t __otx2_io append;
	uint16_t __otx2_io vf;
	/* action */
	uint32_t __otx2_io index;
	uint16_t __otx2_io match_id;
	uint8_t __otx2_io flow_key_alg;
	uint8_t __otx2_io op;
	/* vtag action */
	uint8_t __otx2_io vtag0_type;
	uint8_t __otx2_io vtag0_valid;
	uint8_t __otx2_io vtag1_type;
	uint8_t __otx2_io vtag1_valid;

	/* vtag tx action */
	uint16_t __otx2_io vtag0_def;
	uint8_t  __otx2_io vtag0_op;
	uint16_t __otx2_io vtag1_def;
	uint8_t  __otx2_io vtag1_op;
};

struct npc_install_flow_rsp {
	struct mbox_msghdr hdr;
	/* Negative if no counter else counter number */
	int __otx2_io counter;
};

struct npc_delete_flow_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io entry;
	uint16_t __otx2_io start;/*Disable range of entries */
	uint16_t __otx2_io end;
	uint8_t __otx2_io all; /* PF + VFs */
};

struct npc_mcam_read_entry_req {
	struct mbox_msghdr hdr;
	/* MCAM entry to read */
	uint16_t __otx2_io entry;
};

struct npc_mcam_read_entry_rsp {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
	uint8_t __otx2_io intf;
	uint8_t __otx2_io enable;
};

struct npc_mcam_read_base_rule_rsp {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
};

/* TIM mailbox error codes
 * Range 801 - 900.
 */
enum tim_af_status {
	TIM_AF_NO_RINGS_LEFT			= -801,
	TIM_AF_INVALID_NPA_PF_FUNC		= -802,
	TIM_AF_INVALID_SSO_PF_FUNC		= -803,
	TIM_AF_RING_STILL_RUNNING		= -804,
	TIM_AF_LF_INVALID			= -805,
	TIM_AF_CSIZE_NOT_ALIGNED		= -806,
	TIM_AF_CSIZE_TOO_SMALL			= -807,
	TIM_AF_CSIZE_TOO_BIG			= -808,
	TIM_AF_INTERVAL_TOO_SMALL		= -809,
	TIM_AF_INVALID_BIG_ENDIAN_VALUE		= -810,
	TIM_AF_INVALID_CLOCK_SOURCE		= -811,
	TIM_AF_GPIO_CLK_SRC_NOT_ENABLED		= -812,
	TIM_AF_INVALID_BSIZE			= -813,
	TIM_AF_INVALID_ENABLE_PERIODIC		= -814,
	TIM_AF_INVALID_ENABLE_DONTFREE		= -815,
	TIM_AF_ENA_DONTFRE_NSET_PERIODIC	= -816,
	TIM_AF_RING_ALREADY_DISABLED		= -817,
};

enum tim_clk_srcs {
	TIM_CLK_SRCS_TENNS	= 0,
	TIM_CLK_SRCS_GPIO	= 1,
	TIM_CLK_SRCS_GTI	= 2,
	TIM_CLK_SRCS_PTP	= 3,
	TIM_CLK_SRSC_INVALID,
};

enum tim_gpio_edge {
	TIM_GPIO_NO_EDGE		= 0,
	TIM_GPIO_LTOH_TRANS		= 1,
	TIM_GPIO_HTOL_TRANS		= 2,
	TIM_GPIO_BOTH_TRANS		= 3,
	TIM_GPIO_INVALID,
};

enum ptp_op {
	PTP_OP_ADJFINE = 0, /* adjfine(req.scaled_ppm); */
	PTP_OP_GET_CLOCK = 1, /* rsp.clk = get_clock() */
};

struct ptp_req {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io op;
	int64_t __otx2_io scaled_ppm;
	uint8_t __otx2_io is_pmu;
};

struct ptp_rsp {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io clk;
	uint64_t __otx2_io tsc;
};

struct get_hw_cap_rsp {
	struct mbox_msghdr hdr;
	/* Schq mapping fixed or flexible */
	uint8_t __otx2_io nix_fixed_txschq_mapping;
	uint8_t __otx2_io nix_shaping; /* Is shaping and coloring supported */
};

struct ndc_sync_op {
	struct mbox_msghdr hdr;
	uint8_t __otx2_io nix_lf_tx_sync;
	uint8_t __otx2_io nix_lf_rx_sync;
	uint8_t __otx2_io npa_lf_sync;
};

struct tim_lf_alloc_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io ring;
	uint16_t __otx2_io npa_pf_func;
	uint16_t __otx2_io sso_pf_func;
};

struct tim_ring_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io ring;
};

struct tim_config_req {
	struct mbox_msghdr hdr;
	uint16_t __otx2_io ring;
	uint8_t __otx2_io bigendian;
	uint8_t __otx2_io clocksource;
	uint8_t __otx2_io enableperiodic;
	uint8_t __otx2_io enabledontfreebuffer;
	uint32_t __otx2_io bucketsize;
	uint32_t __otx2_io chunksize;
	uint32_t __otx2_io interval;
};

struct tim_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io tenns_clk;
};

struct tim_enable_rsp {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io timestarted;
	uint32_t __otx2_io currentbucket;
};

/* REE mailbox error codes
 * Range 1001 - 1100.
 */
enum ree_af_status {
	REE_AF_ERR_RULE_UNKNOWN_VALUE		= -1001,
	REE_AF_ERR_LF_NO_MORE_RESOURCES		= -1002,
	REE_AF_ERR_LF_INVALID			= -1003,
	REE_AF_ERR_ACCESS_DENIED		= -1004,
	REE_AF_ERR_RULE_DB_PARTIAL		= -1005,
	REE_AF_ERR_RULE_DB_EQ_BAD_VALUE		= -1006,
	REE_AF_ERR_RULE_DB_BLOCK_ALLOC_FAILED	= -1007,
	REE_AF_ERR_BLOCK_NOT_IMPLEMENTED	= -1008,
	REE_AF_ERR_RULE_DB_INC_OFFSET_TOO_BIG	= -1009,
	REE_AF_ERR_RULE_DB_OFFSET_TOO_BIG	= -1010,
	REE_AF_ERR_Q_IS_GRACEFUL_DIS		= -1011,
	REE_AF_ERR_Q_NOT_GRACEFUL_DIS		= -1012,
	REE_AF_ERR_RULE_DB_ALLOC_FAILED		= -1013,
	REE_AF_ERR_RULE_DB_TOO_BIG		= -1014,
	REE_AF_ERR_RULE_DB_GEQ_BAD_VALUE	= -1015,
	REE_AF_ERR_RULE_DB_LEQ_BAD_VALUE	= -1016,
	REE_AF_ERR_RULE_DB_WRONG_LENGTH		= -1017,
	REE_AF_ERR_RULE_DB_WRONG_OFFSET		= -1018,
	REE_AF_ERR_RULE_DB_BLOCK_TOO_BIG	= -1019,
	REE_AF_ERR_RULE_DB_SHOULD_FILL_REQUEST	= -1020,
	REE_AF_ERR_RULE_DBI_ALLOC_FAILED	= -1021,
	REE_AF_ERR_LF_WRONG_PRIORITY		= -1022,
	REE_AF_ERR_LF_SIZE_TOO_BIG		= -1023,
};

/* REE mbox message formats */

struct ree_req_msg {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io blkaddr;
};

struct ree_lf_req_msg {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io blkaddr;
	uint32_t __otx2_io size;
	uint8_t __otx2_io lf;
	uint8_t __otx2_io pri;
};

struct ree_rule_db_prog_req_msg {
	struct mbox_msghdr hdr;
#define REE_RULE_DB_REQ_BLOCK_SIZE (MBOX_SIZE >> 1)
	uint8_t __otx2_io rule_db[REE_RULE_DB_REQ_BLOCK_SIZE];
	uint32_t __otx2_io blkaddr; /* REE0 or REE1 */
	uint32_t __otx2_io total_len; /* total len of rule db */
	uint32_t __otx2_io offset; /* offset of current rule db block */
	uint16_t __otx2_io len; /* length of rule db block */
	uint8_t __otx2_io is_last; /* is this the last block */
	uint8_t __otx2_io is_incremental; /* is incremental flow */
	uint8_t __otx2_io is_dbi; /* is rule db incremental */
};

struct ree_rule_db_get_req_msg {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io blkaddr;
	uint32_t __otx2_io offset; /* retrieve db from this offset */
	uint8_t __otx2_io is_dbi; /* is request for rule db incremental */
};

struct ree_rd_wr_reg_msg {
	struct mbox_msghdr hdr;
	uint64_t __otx2_io reg_offset;
	uint64_t __otx2_io *ret_val;
	uint64_t __otx2_io val;
	uint32_t __otx2_io blkaddr;
	uint8_t __otx2_io is_write;
};

struct ree_rule_db_len_rsp_msg {
	struct mbox_msghdr hdr;
	uint32_t __otx2_io blkaddr;
	uint32_t __otx2_io len;
	uint32_t __otx2_io inc_len;
};

struct ree_rule_db_get_rsp_msg {
	struct mbox_msghdr hdr;
#define REE_RULE_DB_RSP_BLOCK_SIZE (MBOX_DOWN_TX_SIZE - SZ_1K)
	uint8_t __otx2_io rule_db[REE_RULE_DB_RSP_BLOCK_SIZE];
	uint32_t __otx2_io total_len; /* total len of rule db */
	uint32_t __otx2_io offset; /* offset of current rule db block */
	uint16_t __otx2_io len; /* length of rule db block */
	uint8_t __otx2_io is_last; /* is this the last block */
};

__rte_internal
const char *otx2_mbox_id2name(uint16_t id);
int otx2_mbox_id2size(uint16_t id);
void otx2_mbox_reset(struct otx2_mbox *mbox, int devid);
int otx2_mbox_init(struct otx2_mbox *mbox, uintptr_t hwbase, uintptr_t reg_base,
		   int direction, int ndevsi, uint64_t intr_offset);
void otx2_mbox_fini(struct otx2_mbox *mbox);
__rte_internal
void otx2_mbox_msg_send(struct otx2_mbox *mbox, int devid);
__rte_internal
int otx2_mbox_wait_for_rsp(struct otx2_mbox *mbox, int devid);
int otx2_mbox_wait_for_rsp_tmo(struct otx2_mbox *mbox, int devid, uint32_t tmo);
__rte_internal
int otx2_mbox_get_rsp(struct otx2_mbox *mbox, int devid, void **msg);
__rte_internal
int otx2_mbox_get_rsp_tmo(struct otx2_mbox *mbox, int devid, void **msg,
			  uint32_t tmo);
int otx2_mbox_get_availmem(struct otx2_mbox *mbox, int devid);
__rte_internal
struct mbox_msghdr *otx2_mbox_alloc_msg_rsp(struct otx2_mbox *mbox, int devid,
					    int size, int size_rsp);

static inline struct mbox_msghdr *
otx2_mbox_alloc_msg(struct otx2_mbox *mbox, int devid, int size)
{
	return otx2_mbox_alloc_msg_rsp(mbox, devid, size, 0);
}

static inline void
otx2_mbox_req_init(uint16_t mbox_id, void *msghdr)
{
	struct mbox_msghdr *hdr = msghdr;

	hdr->sig = OTX2_MBOX_REQ_SIG;
	hdr->ver = OTX2_MBOX_VERSION;
	hdr->id = mbox_id;
	hdr->pcifunc = 0;
}

static inline void
otx2_mbox_rsp_init(uint16_t mbox_id, void *msghdr)
{
	struct mbox_msghdr *hdr = msghdr;

	hdr->sig = OTX2_MBOX_RSP_SIG;
	hdr->rc = -ETIMEDOUT;
	hdr->id = mbox_id;
}

static inline bool
otx2_mbox_nonempty(struct otx2_mbox *mbox, int devid)
{
	struct otx2_mbox_dev *mdev = &mbox->dev[devid];
	bool ret;

	rte_spinlock_lock(&mdev->mbox_lock);
	ret = mdev->num_msgs != 0;
	rte_spinlock_unlock(&mdev->mbox_lock);

	return ret;
}

static inline int
otx2_mbox_process(struct otx2_mbox *mbox)
{
	otx2_mbox_msg_send(mbox, 0);
	return otx2_mbox_get_rsp(mbox, 0, NULL);
}

static inline int
otx2_mbox_process_msg(struct otx2_mbox *mbox, void **msg)
{
	otx2_mbox_msg_send(mbox, 0);
	return otx2_mbox_get_rsp(mbox, 0, msg);
}

static inline int
otx2_mbox_process_tmo(struct otx2_mbox *mbox, uint32_t tmo)
{
	otx2_mbox_msg_send(mbox, 0);
	return otx2_mbox_get_rsp_tmo(mbox, 0, NULL, tmo);
}

static inline int
otx2_mbox_process_msg_tmo(struct otx2_mbox *mbox, void **msg, uint32_t tmo)
{
	otx2_mbox_msg_send(mbox, 0);
	return otx2_mbox_get_rsp_tmo(mbox, 0, msg, tmo);
}

int otx2_send_ready_msg(struct otx2_mbox *mbox, uint16_t *pf_func /* out */);
int otx2_reply_invalid_msg(struct otx2_mbox *mbox, int devid, uint16_t pf_func,
			uint16_t id);

#define M(_name, _id, _fn_name, _req_type, _rsp_type)			\
static inline struct _req_type						\
*otx2_mbox_alloc_msg_ ## _fn_name(struct otx2_mbox *mbox)		\
{									\
	struct _req_type *req;						\
									\
	req = (struct _req_type *)otx2_mbox_alloc_msg_rsp(		\
		mbox, 0, sizeof(struct _req_type),			\
		sizeof(struct _rsp_type));				\
	if (!req)							\
		return NULL;						\
									\
	req->hdr.sig = OTX2_MBOX_REQ_SIG;				\
	req->hdr.id = _id;						\
	otx2_mbox_dbg("id=0x%x (%s)",					\
			req->hdr.id, otx2_mbox_id2name(req->hdr.id));	\
	return req;							\
}

MBOX_MESSAGES
#undef M

/* This is required for copy operations from device memory which do not work on
 * addresses which are unaligned to 16B. This is because of specific
 * optimizations to libc memcpy.
 */
static inline volatile void *
otx2_mbox_memcpy(volatile void *d, const volatile void *s, size_t l)
{
	const volatile uint8_t *sb;
	volatile uint8_t *db;
	size_t i;

	if (!d || !s)
		return NULL;
	db = (volatile uint8_t *)d;
	sb = (const volatile uint8_t *)s;
	for (i = 0; i < l; i++)
		db[i] = sb[i];
	return d;
}

/* This is required for memory operations from device memory which do not
 * work on addresses which are unaligned to 16B. This is because of specific
 * optimizations to libc memset.
 */
static inline void
otx2_mbox_memset(volatile void *d, uint8_t val, size_t l)
{
	volatile uint8_t *db;
	size_t i = 0;

	if (!d || !l)
		return;
	db = (volatile uint8_t *)d;
	for (i = 0; i < l; i++)
		db[i] = val;
}

#endif /* __OTX2_MBOX_H__ */
