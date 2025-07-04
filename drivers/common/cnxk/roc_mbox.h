/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#ifndef __ROC_MBOX_H__
#define __ROC_MBOX_H__

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>

#include "hw/cpt.h"

#include "roc_platform.h"

/* Header which precedes all mbox messages */
struct mbox_hdr {
	uint64_t __io msg_size; /* Total msgs size embedded */
	uint16_t __io num_msgs; /* No of msgs embedded */
};

/* Header which precedes every msg and is also part of it */
struct mbox_msghdr {
	uint16_t __io pcifunc; /* Who's sending this msg */
	uint16_t __io id;      /* Mbox message ID */
#define MBOX_REQ_SIG (0xdead)
#define MBOX_RSP_SIG (0xbeef)
	/* Signature, for validating corrupted msgs */
	uint16_t __io sig;
#define MBOX_VERSION (0x000b)
	/* Version of msg's structure for this ID */
	uint16_t __io ver;
	/* Offset of next msg within mailbox region */
	uint16_t __io next_msgoff;
	int __io rc; /* Msg processed response code */
};

#define RVU_AF_AFPF_MBOX0 (0x02000)
#define RVU_AF_AFPF_MBOX1 (0x02008)

#define RVU_PF_PFAF_MBOX0 (0xC00)
#define RVU_PF_PFAF_MBOX1 (0xC08)

#define RVU_PF_VFX_PFVF_MBOX0 (0x0000)
#define RVU_PF_VFX_PFVF_MBOX1 (0x0008)

#define RVU_VF_VFPF_MBOX0 (0x0000)
#define RVU_VF_VFPF_MBOX1 (0x0008)

#define MBOX_DOWN_MSG 1
#define MBOX_UP_MSG   2

/* Mailbox message types */
#define MBOX_MSG_MASK	 0xFFFF
#define MBOX_MSG_INVALID 0xFFFE
#define MBOX_MSG_MAX	 0xFFFF
#define MBOX_MSG_GENERIC_MAX_ID 0x1FF
#define MBOX_MSG_REQ_SIZE_MAX	(16 * 1024)

#define MBOX_MESSAGES                                                          \
	/* Generic mbox IDs (range 0x000 - 0x1FF) */                           \
	M(READY, 0x001, ready, msg_req, ready_msg_rsp)                         \
	M(ATTACH_RESOURCES, 0x002, attach_resources, rsrc_attach_req, msg_rsp) \
	M(DETACH_RESOURCES, 0x003, detach_resources, rsrc_detach_req, msg_rsp) \
	M(FREE_RSRC_CNT, 0x004, free_rsrc_cnt, msg_req, free_rsrcs_rsp)        \
	M(MSIX_OFFSET, 0x005, msix_offset, msg_req, msix_offset_rsp)           \
	M(VF_FLR, 0x006, vf_flr, msg_req, msg_rsp)                             \
	M(PTP_OP, 0x007, ptp_op, ptp_req, ptp_rsp)                             \
	M(GET_HW_CAP, 0x008, get_hw_cap, msg_req, get_hw_cap_rsp)              \
	M(NDC_SYNC_OP, 0x009, ndc_sync_op, ndc_sync_op, msg_rsp)               \
	M(LMTST_TBL_SETUP, 0x00a, lmtst_tbl_setup, lmtst_tbl_setup_req,        \
	  msg_rsp)                                                             \
	M(GET_REP_CNT, 0x00d, get_rep_cnt, msg_req, get_rep_cnt_rsp)           \
	/* CGX mbox IDs (range 0x200 - 0x3FF) */                               \
	M(CGX_START_RXTX, 0x200, cgx_start_rxtx, msg_req, msg_rsp)             \
	M(CGX_STOP_RXTX, 0x201, cgx_stop_rxtx, msg_req, msg_rsp)               \
	M(CGX_STATS, 0x202, cgx_stats, msg_req, cgx_stats_rsp)                 \
	M(CGX_MAC_ADDR_SET, 0x203, cgx_mac_addr_set, cgx_mac_addr_set_or_get,  \
	  cgx_mac_addr_set_or_get)                                             \
	M(CGX_MAC_ADDR_GET, 0x204, cgx_mac_addr_get, cgx_mac_addr_set_or_get,  \
	  cgx_mac_addr_set_or_get)                                             \
	M(CGX_PROMISC_ENABLE, 0x205, cgx_promisc_enable, msg_req, msg_rsp)     \
	M(CGX_PROMISC_DISABLE, 0x206, cgx_promisc_disable, msg_req, msg_rsp)   \
	M(CGX_START_LINKEVENTS, 0x207, cgx_start_linkevents, msg_req, msg_rsp) \
	M(CGX_STOP_LINKEVENTS, 0x208, cgx_stop_linkevents, msg_req, msg_rsp)   \
	M(CGX_GET_LINKINFO, 0x209, cgx_get_linkinfo, msg_req,                  \
	  cgx_link_info_msg)                                                   \
	M(CGX_INTLBK_ENABLE, 0x20A, cgx_intlbk_enable, msg_req, msg_rsp)       \
	M(CGX_INTLBK_DISABLE, 0x20B, cgx_intlbk_disable, msg_req, msg_rsp)     \
	M(CGX_PTP_RX_ENABLE, 0x20C, cgx_ptp_rx_enable, msg_req, msg_rsp)       \
	M(CGX_PTP_RX_DISABLE, 0x20D, cgx_ptp_rx_disable, msg_req, msg_rsp)     \
	M(CGX_CFG_PAUSE_FRM, 0x20E, cgx_cfg_pause_frm, cgx_pause_frm_cfg,      \
	  cgx_pause_frm_cfg)                                                   \
	M(CGX_FW_DATA_GET, 0x20F, cgx_get_aux_link_info, msg_req, cgx_fw_data) \
	M(CGX_FEC_SET, 0x210, cgx_set_fec_param, fec_mode, fec_mode)           \
	M(CGX_MAC_ADDR_ADD, 0x211, cgx_mac_addr_add, cgx_mac_addr_add_req,     \
	  cgx_mac_addr_add_rsp)                                                \
	M(CGX_MAC_ADDR_DEL, 0x212, cgx_mac_addr_del, cgx_mac_addr_del_req,     \
	  msg_rsp)                                                             \
	M(CGX_MAC_MAX_ENTRIES_GET, 0x213, cgx_mac_max_entries_get, msg_req,    \
	  cgx_max_dmac_entries_get_rsp)                                        \
	M(CGX_SET_LINK_STATE, 0x214, cgx_set_link_state,                       \
	  cgx_set_link_state_msg, msg_rsp)                                     \
	M(CGX_GET_PHY_MOD_TYPE, 0x215, cgx_get_phy_mod_type, msg_req,          \
	  cgx_phy_mod_type)                                                    \
	M(CGX_SET_PHY_MOD_TYPE, 0x216, cgx_set_phy_mod_type, cgx_phy_mod_type, \
	  msg_rsp)                                                             \
	M(CGX_FEC_STATS, 0x217, cgx_fec_stats, msg_req, cgx_fec_stats_rsp)     \
	M(CGX_SET_LINK_MODE, 0x218, cgx_set_link_mode, cgx_set_link_mode_req,  \
	  cgx_set_link_mode_rsp)                                               \
	M(CGX_GET_PHY_FEC_STATS, 0x219, cgx_get_phy_fec_stats, msg_req,        \
	  msg_rsp)                                                             \
	M(CGX_STATS_RST, 0x21A, cgx_stats_rst, msg_req, msg_rsp)               \
	M(RPM_STATS, 0x21C, rpm_stats, msg_req, rpm_stats_rsp)                 \
	M(CGX_PRIO_FLOW_CTRL_CFG, 0x21F, cgx_prio_flow_ctrl_cfg, cgx_pfc_cfg,  \
	  cgx_pfc_rsp)                                                         \
	/* NPA mbox IDs (range 0x400 - 0x5FF) */                               \
	M(NPA_LF_ALLOC, 0x400, npa_lf_alloc, npa_lf_alloc_req,                 \
	  npa_lf_alloc_rsp)                                                    \
	M(NPA_LF_FREE, 0x401, npa_lf_free, msg_req, msg_rsp)                   \
	M(NPA_AQ_ENQ, 0x402, npa_aq_enq, npa_aq_enq_req, npa_aq_enq_rsp)       \
	M(NPA_HWCTX_DISABLE, 0x403, npa_hwctx_disable, hwctx_disable_req,      \
	  msg_rsp)                                                             \
	M(NPA_CN20K_AQ_ENQ, 0x404, npa_cn20k_aq_enq, npa_cn20k_aq_enq_req,     \
	  npa_cn20k_aq_enq_rsp)                                                \
	/* SSO/SSOW mbox IDs (range 0x600 - 0x7FF) */                          \
	M(SSO_LF_ALLOC, 0x600, sso_lf_alloc, sso_lf_alloc_req,                 \
	  sso_lf_alloc_rsp)                                                    \
	M(SSO_LF_FREE, 0x601, sso_lf_free, sso_lf_free_req, msg_rsp)           \
	M(SSOW_LF_ALLOC, 0x602, ssow_lf_alloc, ssow_lf_alloc_req, msg_rsp)     \
	M(SSOW_LF_FREE, 0x603, ssow_lf_free, ssow_lf_free_req, msg_rsp)        \
	M(SSO_HW_SETCONFIG, 0x604, sso_hw_setconfig, sso_hw_setconfig,         \
	  msg_rsp)                                                             \
	M(SSO_GRP_SET_PRIORITY, 0x605, sso_grp_set_priority, sso_grp_priority, \
	  msg_rsp)                                                             \
	M(SSO_GRP_GET_PRIORITY, 0x606, sso_grp_get_priority, sso_info_req,     \
	  sso_grp_priority)                                                    \
	M(SSO_WS_CACHE_INV, 0x607, sso_ws_cache_inv, ssow_lf_inv_req, msg_rsp) \
	M(SSO_GRP_QOS_CONFIG, 0x608, sso_grp_qos_config, sso_grp_qos_cfg,      \
	  msg_rsp)                                                             \
	M(SSO_GRP_GET_STATS, 0x609, sso_grp_get_stats, sso_info_req,           \
	  sso_grp_stats)                                                       \
	M(SSO_HWS_GET_STATS, 0x610, sso_hws_get_stats, sso_info_req,           \
	  sso_hws_stats)                                                       \
	M(SSO_HW_RELEASE_XAQ, 0x611, sso_hw_release_xaq_aura,                  \
	  sso_hw_xaq_release, msg_rsp)                                         \
	M(SSO_CONFIG_LSW, 0x612, ssow_config_lsw, ssow_config_lsw, msg_rsp)    \
	M(SSO_HWS_CHNG_MSHIP, 0x613, ssow_chng_mship, ssow_chng_mship,         \
	  msg_rsp)                                                             \
	M(SSO_GRP_STASH_CONFIG, 0x614, sso_grp_stash_config,                   \
	  sso_grp_stash_cfg, msg_rsp)                                          \
	M(SSO_AGGR_SET_CONFIG, 0x615, sso_aggr_setconfig, sso_aggr_setconfig,  \
	  msg_rsp)                                                             \
	M(SSO_AGGR_GET_STATS, 0x616, sso_aggr_get_stats, sso_info_req,         \
	  sso_aggr_stats)                                                      \
	M(SSO_GET_HW_INFO, 0x617, sso_get_hw_info, msg_req, sso_hw_info)       \
	/* TIM mbox IDs (range 0x800 - 0x9FF) */                               \
	M(TIM_LF_ALLOC, 0x800, tim_lf_alloc, tim_lf_alloc_req,                 \
	  tim_lf_alloc_rsp)                                                    \
	M(TIM_LF_FREE, 0x801, tim_lf_free, tim_ring_req, msg_rsp)              \
	M(TIM_CONFIG_RING, 0x802, tim_config_ring, tim_config_req, msg_rsp)    \
	M(TIM_ENABLE_RING, 0x803, tim_enable_ring, tim_ring_req,               \
	  tim_enable_rsp)                                                      \
	M(TIM_DISABLE_RING, 0x804, tim_disable_ring, tim_ring_req, msg_rsp)    \
	M(TIM_GET_MIN_INTVL, 0x805, tim_get_min_intvl, tim_intvl_req,          \
	  tim_intvl_rsp)                                                       \
	M(TIM_CAPTURE_COUNTERS, 0x806, tim_capture_counters, msg_req,          \
	  tim_capture_rsp)                                                     \
	M(TIM_CONFIG_HWWQE,    0x807, tim_config_hwwqe, tim_cfg_hwwqe_req,     \
	  msg_rsp)                                                             \
	M(TIM_GET_HW_INFO,     0x808, tim_get_hw_info, msg_req, tim_hw_info)   \
	/* CPT mbox IDs (range 0xA00 - 0xBFF) */                               \
	M(CPT_LF_ALLOC, 0xA00, cpt_lf_alloc, cpt_lf_alloc_req_msg, msg_rsp)    \
	M(CPT_LF_FREE, 0xA01, cpt_lf_free, msg_req, msg_rsp)                   \
	M(CPT_RD_WR_REGISTER, 0xA02, cpt_rd_wr_register, cpt_rd_wr_reg_msg,    \
	  cpt_rd_wr_reg_msg)                                                   \
	M(CPT_SET_CRYPTO_GRP, 0xA03, cpt_set_crypto_grp,                       \
	  cpt_set_crypto_grp_req_msg, msg_rsp)                                 \
	M(CPT_INLINE_IPSEC_CFG, 0xA04, cpt_inline_ipsec_cfg,                   \
	  cpt_inline_ipsec_cfg_msg, msg_rsp)                                   \
	M(CPT_STATS, 0xA05, cpt_sts_get, cpt_sts_req, cpt_sts_rsp)             \
	M(CPT_RXC_TIME_CFG, 0xA06, cpt_rxc_time_cfg, cpt_rxc_time_cfg_req,     \
	  msg_rsp)                                                             \
	M(CPT_CTX_CACHE_SYNC, 0xA07, cpt_ctx_cache_sync, msg_req, msg_rsp)     \
	M(CPT_LF_RESET, 0xA08, cpt_lf_reset, cpt_lf_rst_req, msg_rsp)          \
	M(CPT_FLT_ENG_INFO, 0xA09, cpt_flt_eng_info, cpt_flt_eng_info_req,	\
				   cpt_flt_eng_info_rsp)			\
	M(CPT_RX_INLINE_QALLOC, 0xA0A, cpt_rx_inline_qalloc, msg_req,		\
				       cpt_rx_inline_qalloc_rsp)		\
	M(CPT_RX_INL_QUEUE_CFG, 0xA0B, cpt_rx_inl_queue_cfg,			\
				       cpt_rx_inline_qcfg_req, msg_rsp)        \
	M(CPT_RX_INLINE_LF_CFG, 0xBFE, cpt_rx_inline_lf_cfg,                   \
	  cpt_rx_inline_lf_cfg_msg, msg_rsp)                                   \
	M(CPT_GET_CAPS, 0xBFD, cpt_caps_get, msg_req, cpt_caps_rsp_msg)        \
	M(CPT_GET_ENG_GRP, 0xBFF, cpt_eng_grp_get, cpt_eng_grp_req,            \
	  cpt_eng_grp_rsp)                                                     \
	M(CPT_SET_QUEUE_PRI, 0xBFB, cpt_set_que_pri, cpt_queue_pri_req_msg,	\
			       msg_rsp)					\
	/* REE mbox IDs (range 0xE00 - 0xFFF) */                               \
	M(REE_CONFIG_LF, 0xE01, ree_config_lf, ree_lf_req_msg, msg_rsp)        \
	M(REE_RD_WR_REGISTER, 0xE02, ree_rd_wr_register, ree_rd_wr_reg_msg,    \
	  ree_rd_wr_reg_msg)                                                   \
	M(REE_RULE_DB_PROG, 0xE03, ree_rule_db_prog, ree_rule_db_prog_req_msg, \
	  msg_rsp)                                                             \
	M(REE_RULE_DB_LEN_GET, 0xE04, ree_rule_db_len_get, ree_req_msg,        \
	  ree_rule_db_len_rsp_msg)                                             \
	M(REE_RULE_DB_GET, 0xE05, ree_rule_db_get, ree_rule_db_get_req_msg,    \
	  ree_rule_db_get_rsp_msg)                                             \
	/* SDP mbox IDs (range 0x1000 - 0x11FF) */                             \
	M(SET_SDP_CHAN_INFO, 0x1000, set_sdp_chan_info, sdp_chan_info_msg,     \
	  msg_rsp)                                                             \
	/* NPC mbox IDs (range 0x6000 - 0x7FFF) */                             \
	M(NPC_MCAM_ALLOC_ENTRY, 0x6000, npc_mcam_alloc_entry,                  \
	  npc_mcam_alloc_entry_req, npc_mcam_alloc_entry_rsp)                  \
	M(NPC_MCAM_FREE_ENTRY, 0x6001, npc_mcam_free_entry,                    \
	  npc_mcam_free_entry_req, msg_rsp)                                    \
	M(NPC_MCAM_WRITE_ENTRY, 0x6002, npc_mcam_write_entry,                  \
	  npc_mcam_write_entry_req, msg_rsp)                                   \
	M(NPC_MCAM_ENA_ENTRY, 0x6003, npc_mcam_ena_entry,                      \
	  npc_mcam_ena_dis_entry_req, msg_rsp)                                 \
	M(NPC_MCAM_DIS_ENTRY, 0x6004, npc_mcam_dis_entry,                      \
	  npc_mcam_ena_dis_entry_req, msg_rsp)                                 \
	M(NPC_MCAM_SHIFT_ENTRY, 0x6005, npc_mcam_shift_entry,                  \
	  npc_mcam_shift_entry_req, npc_mcam_shift_entry_rsp)                  \
	M(NPC_MCAM_ALLOC_COUNTER, 0x6006, npc_mcam_alloc_counter,              \
	  npc_mcam_alloc_counter_req, npc_mcam_alloc_counter_rsp)              \
	M(NPC_MCAM_FREE_COUNTER, 0x6007, npc_mcam_free_counter,                \
	  npc_mcam_oper_counter_req, msg_rsp)                                  \
	M(NPC_MCAM_UNMAP_COUNTER, 0x6008, npc_mcam_unmap_counter,              \
	  npc_mcam_unmap_counter_req, msg_rsp)                                 \
	M(NPC_MCAM_CLEAR_COUNTER, 0x6009, npc_mcam_clear_counter,              \
	  npc_mcam_oper_counter_req, msg_rsp)                                  \
	M(NPC_MCAM_COUNTER_STATS, 0x600a, npc_mcam_counter_stats,              \
	  npc_mcam_oper_counter_req, npc_mcam_oper_counter_rsp)                \
	M(NPC_MCAM_ALLOC_AND_WRITE_ENTRY, 0x600b,                              \
	  npc_mcam_alloc_and_write_entry, npc_mcam_alloc_and_write_entry_req,  \
	  npc_mcam_alloc_and_write_entry_rsp)                                  \
	M(NPC_GET_KEX_CFG, 0x600c, npc_get_kex_cfg, msg_req,                   \
	  npc_get_kex_cfg_rsp)                                                 \
	M(NPC_INSTALL_FLOW, 0x600d, npc_install_flow, npc_install_flow_req,    \
	  npc_install_flow_rsp)                                                \
	M(NPC_DELETE_FLOW, 0x600e, npc_delete_flow, npc_delete_flow_req,       \
	  msg_rsp)                                                             \
	M(NPC_MCAM_READ_ENTRY, 0x600f, npc_mcam_read_entry,                    \
	  npc_mcam_read_entry_req, npc_mcam_read_entry_rsp)                    \
	M(NPC_SET_PKIND, 0x6010, npc_set_pkind, npc_set_pkind, msg_rsp)        \
	M(NPC_MCAM_READ_BASE_RULE, 0x6011, npc_read_base_steer_rule, msg_req,  \
	  npc_mcam_read_base_rule_rsp)                                         \
	M(NPC_MCAM_GET_STATS, 0x6012, npc_mcam_entry_stats,                    \
	  npc_mcam_get_stats_req, npc_mcam_get_stats_rsp)                      \
	M(NPC_GET_FIELD_HASH_INFO, 0x6013, npc_get_field_hash_info,            \
	  npc_get_field_hash_info_req, npc_get_field_hash_info_rsp)            \
	M(NPC_MCAM_GET_HIT_STATUS, 0x6015, npc_mcam_get_hit_status,            \
	  npc_mcam_get_hit_status_req, npc_mcam_get_hit_status_rsp)            \
	M(NPC_MCAM_DEFRAG, 0x6016, npc_defrag, msg_req,	msg_rsp)               \
	M(NPC_CN20K_GET_KEX_CFG, 0x6017, npc_cn20k_get_kex_cfg, msg_req,       \
	  npc_cn20k_get_kex_cfg_rsp)                                           \
	M(NPC_CN20K_MCAM_GET_FREE_COUNT, 0x6018, npc_cn20k_get_free_count,     \
	  msg_req, npc_cn20k_get_free_count_rsp)                               \
	M(NPC_CN20K_MCAM_WRITE_ENTRY,	0x6019, npc_cn20k_mcam_write_entry,    \
	  npc_cn20k_mcam_write_entry_req, msg_rsp)	                       \
	M(NPC_CN20K_MCAM_ALLOC_AND_WRITE_ENTRY, 0x601a,			       \
	  npc_cn20k_mcam_alloc_and_write_entry,				       \
	  npc_cn20k_mcam_alloc_and_write_entry_req,			       \
	  npc_mcam_alloc_and_write_entry_rsp)				       \
	M(NPC_CN20K_MCAM_READ_ENTRY,	0x601b, npc_cn20k_mcam_read_entry,     \
	  npc_mcam_read_entry_req, npc_cn20k_mcam_read_entry_rsp)	       \
	M(NPC_CN20K_MCAM_READ_BASE_RULE, 0x601c,                               \
	  npc_cn20k_read_base_steer_rule, msg_req,                             \
	  npc_cn20k_mcam_read_base_rule_rsp)                                   \
	/* NIX mbox IDs (range 0x8000 - 0xFFFF) */                             \
	M(NIX_LF_ALLOC, 0x8000, nix_lf_alloc, nix_lf_alloc_req,                \
	  nix_lf_alloc_rsp)                                                    \
	M(NIX_LF_FREE, 0x8001, nix_lf_free, nix_lf_free_req, msg_rsp)          \
	M(NIX_AQ_ENQ, 0x8002, nix_aq_enq, nix_aq_enq_req, nix_aq_enq_rsp)      \
	M(NIX_HWCTX_DISABLE, 0x8003, nix_hwctx_disable, hwctx_disable_req,     \
	  msg_rsp)                                                             \
	M(NIX_TXSCH_ALLOC, 0x8004, nix_txsch_alloc, nix_txsch_alloc_req,       \
	  nix_txsch_alloc_rsp)                                                 \
	M(NIX_TXSCH_FREE, 0x8005, nix_txsch_free, nix_txsch_free_req, msg_rsp) \
	M(NIX_TXSCHQ_CFG, 0x8006, nix_txschq_cfg, nix_txschq_config,           \
	  nix_txschq_config)                                                   \
	M(NIX_STATS_RST, 0x8007, nix_stats_rst, msg_req, msg_rsp)              \
	M(NIX_VTAG_CFG, 0x8008, nix_vtag_cfg, nix_vtag_config, msg_rsp)        \
	M(NIX_RSS_FLOWKEY_CFG, 0x8009, nix_rss_flowkey_cfg,                    \
	  nix_rss_flowkey_cfg, nix_rss_flowkey_cfg_rsp)                        \
	M(NIX_SET_MAC_ADDR, 0x800a, nix_set_mac_addr, nix_set_mac_addr,        \
	  msg_rsp)                                                             \
	M(NIX_SET_RX_MODE, 0x800b, nix_set_rx_mode, nix_rx_mode, msg_rsp)      \
	M(NIX_SET_HW_FRS, 0x800c, nix_set_hw_frs, nix_frs_cfg, msg_rsp)        \
	M(NIX_LF_START_RX, 0x800d, nix_lf_start_rx, msg_req, msg_rsp)          \
	M(NIX_LF_STOP_RX, 0x800e, nix_lf_stop_rx, msg_req, msg_rsp)            \
	M(NIX_MARK_FORMAT_CFG, 0x800f, nix_mark_format_cfg,                    \
	  nix_mark_format_cfg, nix_mark_format_cfg_rsp)                        \
	M(NIX_SET_RX_CFG, 0x8010, nix_set_rx_cfg, nix_rx_cfg, msg_rsp)         \
	M(NIX_LSO_FORMAT_CFG, 0x8011, nix_lso_format_cfg, nix_lso_format_cfg,  \
	  nix_lso_format_cfg_rsp)                                              \
	M(NIX_LF_PTP_TX_ENABLE, 0x8013, nix_lf_ptp_tx_enable, msg_req,         \
	  msg_rsp)                                                             \
	M(NIX_LF_PTP_TX_DISABLE, 0x8014, nix_lf_ptp_tx_disable, msg_req,       \
	  msg_rsp)                                                             \
	M(NIX_SET_VLAN_TPID, 0x8015, nix_set_vlan_tpid, nix_set_vlan_tpid,     \
	  msg_rsp)                                                             \
	M(NIX_BP_ENABLE, 0x8016, nix_bp_enable, nix_bp_cfg_req,                \
	  nix_bp_cfg_rsp)                                                      \
	M(NIX_BP_DISABLE, 0x8017, nix_bp_disable, nix_bp_cfg_req, msg_rsp)     \
	M(NIX_GET_MAC_ADDR, 0x8018, nix_get_mac_addr, msg_req,                 \
	  nix_get_mac_addr_rsp)                                                \
	M(NIX_INLINE_IPSEC_CFG, 0x8019, nix_inline_ipsec_cfg,                  \
	  nix_inline_ipsec_cfg, msg_rsp)                                       \
	M(NIX_INLINE_IPSEC_LF_CFG, 0x801a, nix_inline_ipsec_lf_cfg,            \
	  nix_inline_ipsec_lf_cfg, msg_rsp)                                    \
	M(NIX_CN10K_AQ_ENQ, 0x801b, nix_cn10k_aq_enq, nix_cn10k_aq_enq_req,    \
	  nix_cn10k_aq_enq_rsp)                                                \
	M(NIX_GET_HW_INFO, 0x801c, nix_get_hw_info, msg_req, nix_hw_info)      \
	M(NIX_BANDPROF_ALLOC, 0x801d, nix_bandprof_alloc,                      \
	  nix_bandprof_alloc_req, nix_bandprof_alloc_rsp)                      \
	M(NIX_BANDPROF_FREE, 0x801e, nix_bandprof_free, nix_bandprof_free_req, \
	  msg_rsp)                                                             \
	M(NIX_BANDPROF_GET_HWINFO, 0x801f, nix_bandprof_get_hwinfo, msg_req,   \
	  nix_bandprof_get_hwinfo_rsp)                                         \
	M(NIX_CPT_BP_ENABLE, 0x8020, nix_cpt_bp_enable, nix_bp_cfg_req,        \
	  nix_bp_cfg_rsp)                                                      \
	M(NIX_CPT_BP_DISABLE, 0x8021, nix_cpt_bp_disable, nix_bp_cfg_req,      \
	  msg_rsp)                                                             \
	M(NIX_RX_SW_SYNC, 0x8022, nix_rx_sw_sync, msg_req, msg_rsp)            \
	M(NIX_READ_INLINE_IPSEC_CFG, 0x8023, nix_read_inline_ipsec_cfg,        \
	  msg_req, nix_inline_ipsec_cfg)                                       \
	M(NIX_LF_INLINE_RQ_CFG, 0x8024, nix_lf_inline_rq_cfg,                  \
	  nix_rq_cpt_field_mask_cfg_req, msg_rsp)                              \
	M(NIX_SPI_TO_SA_ADD, 0x8026, nix_spi_to_sa_add, nix_spi_to_sa_add_req, \
	  nix_spi_to_sa_add_rsp)                                               \
	M(NIX_SPI_TO_SA_DELETE, 0x8027, nix_spi_to_sa_delete,                  \
	  nix_spi_to_sa_delete_req, msg_rsp)                                   \
	M(NIX_ALLOC_BPIDS, 0x8028, nix_alloc_bpids, nix_alloc_bpid_req,        \
	  nix_bpids)                                                           \
	M(NIX_FREE_BPIDS, 0x8029, nix_free_bpids, nix_bpids, msg_rsp)          \
	M(NIX_RX_CHAN_CFG, 0x802a, nix_rx_chan_cfg, nix_rx_chan_cfg,           \
	  nix_rx_chan_cfg)                                                     \
	M(NIX_MCAST_GRP_CREATE, 0x802b, nix_mcast_grp_create, nix_mcast_grp_create_req,            \
	  nix_mcast_grp_create_rsp)                                                                \
	M(NIX_MCAST_GRP_DESTROY, 0x802c, nix_mcast_grp_destroy, nix_mcast_grp_destroy_req, msg_rsp)\
	M(NIX_MCAST_GRP_UPDATE, 0x802d, nix_mcast_grp_update, nix_mcast_grp_update_req,            \
	  nix_mcast_grp_update_rsp)                                                                \
	M(NIX_GET_LF_STATS,    0x802e, nix_get_lf_stats, nix_get_lf_stats_req, nix_lf_stats_rsp)   \
	M(NIX_CN20K_AQ_ENQ, 0x802f, nix_cn20k_aq_enq, nix_cn20k_aq_enq_req, nix_cn20k_aq_enq_rsp)  \
	M(NIX_LSO_ALT_FLAGS_CFG, 0x8030, nix_lso_alt_flags_cfg, nix_lso_alt_flags_cfg_req, \
				nix_lso_alt_flags_cfg_rsp)			\
	M(NIX_RX_INLINE_PROFILE_CFG, 0x8031, nix_rx_inl_profile_cfg,			\
				nix_rx_inl_profile_cfg_req,			\
				nix_rx_inl_profile_cfg_rsp)			\
	M(NIX_RX_INLINE_LF_CFG, 0x8032, nix_rx_inl_lf_cfg, nix_rx_inl_lf_cfg_req,	\
				msg_rsp)					\
	M(NIX_RX_INL_QUEUE_CFG,	0x8033, nix_rx_inl_queue_cfg,				\
			      nix_rx_inline_qcfg_req, msg_rsp)		\
	/* MCS mbox IDs (range 0xa000 - 0xbFFF) */                                                 \
	M(MCS_ALLOC_RESOURCES, 0xa000, mcs_alloc_resources, mcs_alloc_rsrc_req,                    \
	  mcs_alloc_rsrc_rsp)                                                                      \
	M(MCS_FREE_RESOURCES, 0xa001, mcs_free_resources, mcs_free_rsrc_req, msg_rsp)              \
	M(MCS_FLOWID_ENTRY_WRITE, 0xa002, mcs_flowid_entry_write, mcs_flowid_entry_write_req,      \
	  msg_rsp)                                                                                 \
	M(MCS_SECY_PLCY_WRITE, 0xa003, mcs_secy_plcy_write, mcs_secy_plcy_write_req, msg_rsp)      \
	M(MCS_RX_SC_CAM_WRITE, 0xa004, mcs_rx_sc_cam_write, mcs_rx_sc_cam_write_req, msg_rsp)      \
	M(MCS_SA_PLCY_WRITE, 0xa005, mcs_sa_plcy_write, mcs_sa_plcy_write_req, msg_rsp)            \
	M(MCS_TX_SC_SA_MAP_WRITE, 0xa006, mcs_tx_sc_sa_map_write, mcs_tx_sc_sa_map, msg_rsp)       \
	M(MCS_RX_SC_SA_MAP_WRITE, 0xa007, mcs_rx_sc_sa_map_write, mcs_rx_sc_sa_map, msg_rsp)       \
	M(MCS_FLOWID_ENA_ENTRY, 0xa008, mcs_flowid_ena_entry, mcs_flowid_ena_dis_entry, msg_rsp)   \
	M(MCS_PN_TABLE_WRITE, 0xa009, mcs_pn_table_write, mcs_pn_table_write_req, msg_rsp)         \
	M(MCS_SET_ACTIVE_LMAC, 0xa00a, mcs_set_active_lmac, mcs_set_active_lmac, msg_rsp)          \
	M(MCS_GET_HW_INFO, 0xa00b, mcs_get_hw_info, msg_req, mcs_hw_info)                          \
	M(MCS_GET_FLOWID_STATS, 0xa00c, mcs_get_flowid_stats, mcs_stats_req, mcs_flowid_stats)     \
	M(MCS_GET_SECY_STATS, 0xa00d, mcs_get_secy_stats, mcs_stats_req, mcs_secy_stats)           \
	M(MCS_GET_SC_STATS, 0xa00e, mcs_get_sc_stats, mcs_stats_req, mcs_sc_stats)                 \
	M(MCS_GET_PORT_STATS, 0xa010, mcs_get_port_stats, mcs_stats_req, mcs_port_stats)           \
	M(MCS_CLEAR_STATS, 0xa011, mcs_clear_stats, mcs_clear_stats, msg_rsp)                      \
	M(MCS_INTR_CFG, 0xa012, mcs_intr_cfg, mcs_intr_cfg, msg_rsp)                               \
	M(MCS_SET_LMAC_MODE, 0xa013, mcs_set_lmac_mode, mcs_set_lmac_mode, msg_rsp)                \
	M(MCS_SET_PN_THRESHOLD, 0xa014, mcs_set_pn_threshold, mcs_set_pn_threshold, msg_rsp)       \
	M(MCS_ALLOC_CTRL_PKT_RULE, 0xa015, mcs_alloc_ctrl_pkt_rule, mcs_alloc_ctrl_pkt_rule_req,   \
	  mcs_alloc_ctrl_pkt_rule_rsp)                                                             \
	M(MCS_FREE_CTRL_PKT_RULE, 0xa016, mcs_free_ctrl_pkt_rule, mcs_free_ctrl_pkt_rule_req,      \
	  msg_rsp)                                                                                 \
	M(MCS_CTRL_PKT_RULE_WRITE, 0xa017, mcs_ctrl_pkt_rule_write, mcs_ctrl_pkt_rule_write_req,   \
	  msg_rsp)                                                                                 \
	M(MCS_PORT_RESET, 0xa018, mcs_port_reset, mcs_port_reset_req, msg_rsp)                     \
	M(MCS_PORT_CFG_SET, 0xa019, mcs_port_cfg_set, mcs_port_cfg_set_req, msg_rsp)               \
	M(MCS_PORT_CFG_GET, 0xa020, mcs_port_cfg_get, mcs_port_cfg_get_req, mcs_port_cfg_get_rsp)  \
	M(MCS_CUSTOM_TAG_CFG_GET, 0xa021, mcs_custom_tag_cfg_get, mcs_custom_tag_cfg_get_req,      \
	  mcs_custom_tag_cfg_get_rsp)                                                              \
	M(MCS_FIPS_RESET, 0xa040, mcs_fips_reset, mcs_fips_req, msg_rsp)                           \
	M(MCS_FIPS_MODE_SET, 0xa041, mcs_fips_mode_set, mcs_fips_mode_req, msg_rsp)                \
	M(MCS_FIPS_CTL_SET, 0xa042, mcs_fips_ctl_set, mcs_fips_ctl_req, msg_rsp)                   \
	M(MCS_FIPS_IV_SET, 0xa043, mcs_fips_iv_set, mcs_fips_iv_req, msg_rsp)                      \
	M(MCS_FIPS_CTR_SET, 0xa044, mcs_fips_ctr_set, mcs_fips_ctr_req, msg_rsp)                   \
	M(MCS_FIPS_KEY_SET, 0xa045, mcs_fips_key_set, mcs_fips_key_req, msg_rsp)                   \
	M(MCS_FIPS_BLOCK_SET, 0xa046, mcs_fips_block_set, mcs_fips_block_req, msg_rsp)             \
	M(MCS_FIPS_START, 0xa047, mcs_fips_start, mcs_fips_req, msg_rsp)                           \
	M(MCS_FIPS_RESULT_GET, 0xa048, mcs_fips_result_get, mcs_fips_req, mcs_fips_result_rsp)

/* Messages initiated by AF (range 0xC00 - 0xDFF) */
#define MBOX_UP_CGX_MESSAGES                                                   \
	M(CGX_LINK_EVENT, 0xC00, cgx_link_event, cgx_link_info_msg, msg_rsp)   \
	M(CGX_PTP_RX_INFO, 0xC01, cgx_ptp_rx_info, cgx_ptp_rx_info_msg, msg_rsp)

#define MBOX_UP_MCS_MESSAGES M(MCS_INTR_NOTIFY, 0xE00, mcs_intr_notify, mcs_intr_info, msg_rsp)

#define MBOX_UP_REP_MESSAGES M(REP_EVENT_UP_NOTIFY, 0xEF0, rep_event_up_notify, rep_event, msg_rsp)

enum {
#define M(_name, _id, _1, _2, _3) MBOX_MSG_##_name = _id,
	MBOX_MESSAGES MBOX_UP_CGX_MESSAGES MBOX_UP_MCS_MESSAGES MBOX_UP_REP_MESSAGES
#undef M
};

/* Mailbox message formats */

#define RVU_DEFAULT_PF_FUNC 0xFFFF

/* Generic request msg used for those mbox messages which
 * don't send any data in the request.
 */
struct msg_req {
	struct mbox_msghdr hdr;
};

/* Generic response msg used a ack or response for those mbox
 * messages which does not have a specific rsp msg format.
 */
struct msg_rsp {
	struct mbox_msghdr hdr;
};

/* RVU mailbox error codes
 * Range 256 - 300.
 */
enum rvu_af_status {
	RVU_INVALID_VF_ID = -256,
};

/* For NIX RX vtag action  */
enum nix_rx_vtag0_type {
	NIX_RX_VTAG_TYPE0,
	NIX_RX_VTAG_TYPE1,
	NIX_RX_VTAG_TYPE2,
	NIX_RX_VTAG_TYPE3,
	NIX_RX_VTAG_TYPE4,
	NIX_RX_VTAG_TYPE5,
	NIX_RX_VTAG_TYPE6,
	NIX_RX_VTAG_TYPE7,
};

struct ready_msg_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io sclk_freq; /* SCLK frequency */
	uint16_t __io rclk_freq; /* RCLK frequency */
};

enum npc_pkind_type {
	NPC_RX_CUSTOM_PRE_L2_PKIND = 55ULL,
	NPC_RX_VLAN_EXDSA_PKIND = 56ULL,
	NPC_RX_CHLEN24B_PKIND,
	NPC_RX_CPT_HDR_PKIND,
	NPC_RX_CHLEN90B_PKIND,
	NPC_TX_HIGIG_PKIND,
	NPC_RX_HIGIG_PKIND,
	NPC_RX_EXDSA_PKIND,
	NPC_RX_EDSA_PKIND,
	NPC_TX_DEF_PKIND,
};

/* Struct to set pkind */
struct npc_set_pkind {
	struct mbox_msghdr hdr;
#define ROC_PRIV_FLAGS_DEFAULT	  BIT_ULL(0)
#define ROC_PRIV_FLAGS_EDSA	  BIT_ULL(1)
#define ROC_PRIV_FLAGS_HIGIG	  BIT_ULL(2)
#define ROC_PRIV_FLAGS_LEN_90B	  BIT_ULL(3)
#define ROC_PRIV_FLAGS_EXDSA	  BIT_ULL(4)
#define ROC_PRIV_FLAGS_VLAN_EXDSA BIT_ULL(5)
#define ROC_PRIV_FLAGS_PRE_L2	  BIT_ULL(6)
#define ROC_PRIV_FLAGS_CUSTOM	  BIT_ULL(63)
	uint64_t __io mode;
#define PKIND_TX BIT_ULL(0)
#define PKIND_RX BIT_ULL(1)
	uint8_t __io dir;
	uint8_t __io pkind; /* valid only in case custom flag */
	uint8_t __io var_len_off;
	/* Offset of custom header length field.
	 * Valid only for pkind NPC_RX_CUSTOM_PRE_L2_PKIND
	 */
	uint8_t __io var_len_off_mask; /* Mask for length with in offset */
	uint8_t __io shift_dir;
	/* Shift direction to get length of the
	 * header at var_len_off
	 */
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
	uint8_t __io modify : 1;
	uint8_t __io npalf : 1;
	uint8_t __io nixlf : 1;
	uint16_t __io sso;
	uint16_t __io ssow;
	uint16_t __io timlfs;
	uint16_t __io cptlfs;
	uint16_t __io reelfs;
	/* BLKADDR_CPT0/BLKADDR_CPT1 or 0 for BLKADDR_CPT0 */
	int __io cpt_blkaddr;
	/* BLKADDR_REE0/BLKADDR_REE1 or 0 for BLKADDR_REE0 */
	int __io ree_blkaddr;
};

/* Structure for relinquishing resources.
 * 'partial' flag to be used when relinquishing all resources
 * but only of a certain type. If not set, all resources of all
 * types provisioned to the RVU function will be detached.
 */
struct rsrc_detach_req {
	struct mbox_msghdr hdr;
	uint8_t __io partial : 1;
	uint8_t __io npalf : 1;
	uint8_t __io nixlf : 1;
	uint8_t __io sso : 1;
	uint8_t __io ssow : 1;
	uint8_t __io timlfs : 1;
	uint8_t __io cptlfs : 1;
	uint8_t __io reelfs : 1;
};

/* NIX Transmit schedulers */
#define NIX_TXSCH_LVL_SMQ 0x0
#define NIX_TXSCH_LVL_MDQ 0x0
#define NIX_TXSCH_LVL_TL4 0x1
#define NIX_TXSCH_LVL_TL3 0x2
#define NIX_TXSCH_LVL_TL2 0x3
#define NIX_TXSCH_LVL_TL1 0x4
#define NIX_TXSCH_LVL_CNT 0x5

/*
 * Number of resources available to the caller.
 * In reply to MBOX_MSG_FREE_RSRC_CNT.
 */
struct free_rsrcs_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io schq[NIX_TXSCH_LVL_CNT];
	uint16_t __io sso;
	uint16_t __io tim;
	uint16_t __io ssow;
	uint16_t __io cpt;
	uint8_t __io npa;
	uint8_t __io nix;
	uint16_t __io schq_nix1[NIX_TXSCH_LVL_CNT];
	uint8_t __io nix1;
	uint8_t __io cpt1;
	uint8_t __io ree0;
	uint8_t __io ree1;
};

#define MSIX_VECTOR_INVALID 0xFFFF
#define MAX_RVU_BLKLF_CNT   256

struct msix_offset_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io npa_msixoff;
	uint16_t __io nix_msixoff;
	uint16_t __io sso;
	uint16_t __io ssow;
	uint16_t __io timlfs;
	uint16_t __io cptlfs;
	uint16_t __io sso_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __io ssow_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __io timlf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __io cptlf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __io cpt1_lfs;
	uint16_t __io ree0_lfs;
	uint16_t __io ree1_lfs;
	uint16_t __io cpt1_lf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __io ree0_lf_msixoff[MAX_RVU_BLKLF_CNT];
	uint16_t __io ree1_lf_msixoff[MAX_RVU_BLKLF_CNT];
};

struct lmtst_tbl_setup_req {
	struct mbox_msghdr hdr;

	uint64_t __io dis_sched_early_comp : 1;
	uint64_t __io sched_ena : 1;
	uint64_t __io dis_line_pref : 1;
	uint64_t __io ssow_pf_func : 13;
	uint16_t __io pcifunc;
	uint8_t __io use_local_lmt_region;
	uint64_t __io lmt_iova;
	uint64_t __io rsvd[2]; /* Future use */
};

#define MAX_PFVF_REP 64
struct get_rep_cnt_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io rep_cnt;
	uint16_t __io rep_pfvf_map[MAX_PFVF_REP];
};

/* CGX mbox message formats */
/* CGX mailbox error codes
 * Range 1101 - 1200.
 */
enum cgx_af_status {
	LMAC_AF_ERR_INVALID_PARAM = -1101,
	LMAC_AF_ERR_PF_NOT_MAPPED = -1102,
	LMAC_AF_ERR_PERM_DENIED = -1103,
	LMAC_AF_ERR_PFC_ENADIS_PERM_DENIED = -1104,
	LMAC_AF_ERR_8023PAUSE_ENADIS_PERM_DENIED = -1105,
	LMAC_AF_ERR_CMD_TIMEOUT = -1106,
	LMAC_AF_ERR_FIRMWARE_DATA_NOT_MAPPED = -1107,
	LMAC_AF_ERR_EXACT_MATCH_TBL_ADD_FAILED = -1108,
	LMAC_AF_ERR_EXACT_MATCH_TBL_DEL_FAILED = -1109,
	LMAC_AF_ERR_EXACT_MATCH_TBL_LOOK_UP_FAILED = -1110,
};

struct cgx_stats_rsp {
	struct mbox_msghdr hdr;
#define CGX_RX_STATS_COUNT 9
#define CGX_TX_STATS_COUNT 18
	uint64_t __io rx_stats[CGX_RX_STATS_COUNT];
	uint64_t __io tx_stats[CGX_TX_STATS_COUNT];
};

struct rpm_stats_rsp {
	struct mbox_msghdr hdr;
#define RPM_RX_STATS_COUNT 43
#define RPM_TX_STATS_COUNT 34
	uint64_t __io rx_stats[RPM_RX_STATS_COUNT];
	uint64_t __io tx_stats[RPM_TX_STATS_COUNT];
};

struct cgx_fec_stats_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io fec_corr_blks;
	uint64_t __io fec_uncorr_blks;
};

/* Structure for requesting the operation for
 * setting/getting mac address in the CGX interface
 */
struct cgx_mac_addr_set_or_get {
	struct mbox_msghdr hdr;
	uint8_t __io mac_addr[PLT_ETHER_ADDR_LEN];
	uint32_t index;
};

/* Structure for requesting the operation to
 * add DMAC filter entry into CGX interface
 */
struct cgx_mac_addr_add_req {
	struct mbox_msghdr hdr;
	uint8_t __io mac_addr[PLT_ETHER_ADDR_LEN];
};

/* Structure for response against the operation to
 * add DMAC filter entry into CGX interface
 */
struct cgx_mac_addr_add_rsp {
	struct mbox_msghdr hdr;
	uint32_t __io index;
};

/* Structure for requesting the operation to
 * delete DMAC filter entry from CGX interface
 */
struct cgx_mac_addr_del_req {
	struct mbox_msghdr hdr;
	uint32_t __io index;
};

/* Structure for response against the operation to
 * get maximum supported DMAC filter entries
 */
struct cgx_max_dmac_entries_get_rsp {
	struct mbox_msghdr hdr;
	uint32_t __io max_dmac_filters;
};

struct cgx_link_user_info {
	uint64_t __io link_up : 1;
	uint64_t __io full_duplex : 1;
	uint64_t __io lmac_type_id : 4;
	uint64_t __io speed : 20; /* speed in Mbps */
	uint64_t __io an : 1;	  /* AN supported or not */
	uint64_t __io fec : 2;	  /* FEC type if enabled else 0 */
	uint64_t __io port : 8;
#define LMACTYPE_STR_LEN 16
	char lmac_type[LMACTYPE_STR_LEN];
};

struct cgx_link_info_msg {
	struct mbox_msghdr hdr;
	struct cgx_link_user_info link_info;
};

struct cgx_ptp_rx_info_msg {
	struct mbox_msghdr hdr;
	uint8_t __io ptp_en;
};

struct cgx_pause_frm_cfg {
	struct mbox_msghdr hdr;
	uint8_t __io set;
	/* set = 1 if the request is to config pause frames */
	/* set = 0 if the request is to fetch pause frames config */
	uint8_t __io rx_pause;
	uint8_t __io tx_pause;
};

struct cgx_pfc_cfg {
	struct mbox_msghdr hdr;
	uint8_t __io rx_pause;
	uint8_t __io tx_pause;
	uint16_t __io pfc_en; /*  bitmap indicating enabled traffic classes */
};

struct cgx_pfc_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io rx_pause;
	uint8_t __io tx_pause;
};

struct sfp_eeprom_s {
#define SFP_EEPROM_SIZE 256
	uint16_t __io sff_id;
	uint8_t __io buf[SFP_EEPROM_SIZE];
	uint64_t __io reserved;
};

enum fec_type {
	ROC_FEC_NONE,
	ROC_FEC_BASER,
	ROC_FEC_RS,
};

struct phy_s {
	uint64_t __io can_change_mod_type : 1;
	uint64_t __io mod_type : 1;
};

struct cgx_lmac_fwdata_s {
	uint16_t __io rw_valid;
	uint64_t __io supported_fec;
	uint64_t __io supported_an;
	uint64_t __io supported_link_modes;
	/* Only applicable if AN is supported */
	uint64_t __io advertised_fec;
	uint64_t __io advertised_link_modes;
	/* Only applicable if SFP/QSFP slot is present */
	struct sfp_eeprom_s sfp_eeprom;
	struct phy_s phy;
#define LMAC_FWDATA_RESERVED_MEM 1023
	uint64_t __io reserved[LMAC_FWDATA_RESERVED_MEM];
};

struct cgx_fw_data {
	struct mbox_msghdr hdr;
	struct cgx_lmac_fwdata_s fwdata;
};

struct fec_mode {
	struct mbox_msghdr hdr;
	int __io fec;
};

struct cgx_set_link_state_msg {
	struct mbox_msghdr hdr;
	uint8_t __io enable;
};

struct cgx_phy_mod_type {
	struct mbox_msghdr hdr;
	int __io mod;
};

struct cgx_set_link_mode_args {
	uint32_t __io speed;
	uint8_t __io duplex;
	uint8_t __io an;
	uint8_t __io ports;
	uint64_t __io mode;
};

struct cgx_set_link_mode_req {
	struct mbox_msghdr hdr;
	struct cgx_set_link_mode_args args;
};

struct cgx_set_link_mode_rsp {
	struct mbox_msghdr hdr;
	int __io status;
};

/* MCS mbox structures */
enum mcs_direction {
	MCS_RX,
	MCS_TX,
};

enum mcs_rsrc_type {
	MCS_RSRC_TYPE_FLOWID,
	MCS_RSRC_TYPE_SECY,
	MCS_RSRC_TYPE_SC,
	MCS_RSRC_TYPE_SA,
};

struct mcs_alloc_rsrc_req {
	struct mbox_msghdr hdr;
	uint8_t __io rsrc_type;
	uint8_t __io rsrc_cnt; /* Resources count */
	uint8_t __io mcs_id;   /* MCS block ID */
	uint8_t __io dir;      /* Macsec ingress or egress side */
	uint8_t __io all;      /* Allocate all resource type one each */
	uint64_t __io rsvd;
};

struct mcs_alloc_rsrc_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io flow_ids[128]; /* Index of reserved entries */
	uint8_t __io secy_ids[128];
	uint8_t __io sc_ids[128];
	uint8_t __io sa_ids[256];
	uint8_t __io rsrc_type;
	uint8_t __io rsrc_cnt; /* No of entries reserved */
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint8_t __io all;
	uint8_t __io rsvd[256];
};

struct mcs_free_rsrc_req {
	struct mbox_msghdr hdr;
	uint8_t __io rsrc_id; /* Index of the entry to be freed */
	uint8_t __io rsrc_type;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint8_t __io all; /* Free all the cam resources */
	uint64_t __io rsvd;
};

struct mcs_flowid_entry_write_req {
	struct mbox_msghdr hdr;
	uint64_t __io data[4];
	uint64_t __io mask[4];
	uint64_t __io sci; /* CNF10K-B for tx_secy_mem_map */
	uint8_t __io flow_id;
	uint8_t __io secy_id; /* secyid for which flowid is mapped */
	/* sc_id is Valid if dir = MCS_TX, SC_CAM id mapped to flowid */
	uint8_t __io sc_id;
	uint8_t __io ena; /* Enable tcam entry */
	uint8_t __io ctr_pkt;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_secy_plcy_write_req {
	struct mbox_msghdr hdr;
	uint64_t __io plcy;
	uint8_t __io secy_id;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

/* RX SC_CAM mapping */
struct mcs_rx_sc_cam_write_req {
	struct mbox_msghdr hdr;
	uint64_t __io sci;     /* SCI */
	uint64_t __io secy_id; /* secy index mapped to SC */
	uint8_t __io sc_id;    /* SC CAM entry index */
	uint8_t __io mcs_id;
	uint64_t __io rsvd;
};

struct mcs_sa_plcy_write_req {
	struct mbox_msghdr hdr;
	uint64_t __io plcy[2][9]; /* Support 2 SA policy */
	uint8_t __io sa_index[2];
	uint8_t __io sa_cnt;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_tx_sc_sa_map {
	struct mbox_msghdr hdr;
	uint8_t __io sa_index0;
	uint8_t __io sa_index1;
	uint8_t __io rekey_ena;
	uint8_t __io sa_index0_vld;
	uint8_t __io sa_index1_vld;
	uint8_t __io tx_sa_active;
	uint64_t __io sectag_sci;
	uint8_t __io sc_id; /* used as index for SA_MEM_MAP */
	uint8_t __io mcs_id;
	uint64_t __io rsvd;
};

struct mcs_rx_sc_sa_map {
	struct mbox_msghdr hdr;
	uint8_t __io sa_index;
	uint8_t __io sa_in_use;
	uint8_t __io sc_id;
	/* an range is 0-3, sc_id + an used as index SA_MEM_MAP */
	uint8_t __io an;
	uint8_t __io mcs_id;
	uint64_t __io rsvd;
};

struct mcs_flowid_ena_dis_entry {
	struct mbox_msghdr hdr;
	uint8_t __io flow_id;
	uint8_t __io ena;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_pn_table_write_req {
	struct mbox_msghdr hdr;
	uint64_t __io next_pn;
	uint8_t __io pn_id;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_cam_entry_read_req {
	struct mbox_msghdr hdr;
	uint8_t __io rsrc_type; /* TCAM/SECY/SC/SA/PN */
	uint8_t __io rsrc_id;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_cam_entry_read_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io reg_val[10];
	uint8_t __io rsrc_type;
	uint8_t __io rsrc_id;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_hw_info {
	struct mbox_msghdr hdr;
	uint8_t __io num_mcs_blks; /* Number of MCS blocks */
	uint8_t __io tcam_entries; /* RX/TX Tcam entries per mcs block */
	uint8_t __io secy_entries; /* RX/TX SECY entries per mcs block */
	uint8_t __io sc_entries;   /* RX/TX SC CAM entries per mcs block */
	uint16_t __io sa_entries;  /* PN table entries = SA entries */
	uint64_t __io rsvd[16];
};

struct mcs_set_active_lmac {
	struct mbox_msghdr hdr;
	uint32_t __io lmac_bmap; /* bitmap of active lmac per mcs block */
	uint8_t __io mcs_id;
	uint16_t __io channel_base; /* MCS channel base */
	uint64_t __io rsvd;
};

#define MCS_CPM_RX_SECTAG_V_EQ1_INT	     BIT_ULL(0)
#define MCS_CPM_RX_SECTAG_E_EQ0_C_EQ1_INT    BIT_ULL(1)
#define MCS_CPM_RX_SECTAG_SL_GTE48_INT	     BIT_ULL(2)
#define MCS_CPM_RX_SECTAG_ES_EQ1_SC_EQ1_INT  BIT_ULL(3)
#define MCS_CPM_RX_SECTAG_SC_EQ1_SCB_EQ1_INT BIT_ULL(4)
#define MCS_CPM_RX_PACKET_XPN_EQ0_INT	     BIT_ULL(5)
#define MCS_CPM_RX_PN_THRESH_REACHED_INT     BIT_ULL(6)
#define MCS_CPM_TX_PACKET_XPN_EQ0_INT	     BIT_ULL(7)
#define MCS_CPM_TX_PN_THRESH_REACHED_INT     BIT_ULL(8)
#define MCS_CPM_TX_SA_NOT_VALID_INT	     BIT_ULL(9)
#define MCS_BBE_RX_DFIFO_OVERFLOW_INT	     BIT_ULL(10)
#define MCS_BBE_RX_PLFIFO_OVERFLOW_INT	     BIT_ULL(11)
#define MCS_BBE_TX_DFIFO_OVERFLOW_INT	     BIT_ULL(12)
#define MCS_BBE_TX_PLFIFO_OVERFLOW_INT	     BIT_ULL(13)
#define MCS_PAB_RX_CHAN_OVERFLOW_INT	     BIT_ULL(14)
#define MCS_PAB_TX_CHAN_OVERFLOW_INT	     BIT_ULL(15)

struct mcs_intr_cfg {
	struct mbox_msghdr hdr;
	uint64_t __io intr_mask; /* Interrupt enable mask */
	uint8_t __io mcs_id;
};

struct mcs_intr_info {
	struct mbox_msghdr hdr;
	uint64_t __io intr_mask;
	int __io sa_id;
	uint8_t __io mcs_id;
	uint8_t __io lmac_id;
	uint64_t __io rsvd;
};

struct mcs_set_lmac_mode {
	struct mbox_msghdr hdr;
	uint8_t __io mode; /* '1' for internal bypass mode (passthrough), '0' for MCS processing */
	uint8_t __io lmac_id;
	uint8_t __io mcs_id;
	uint64_t __io rsvd;
};

struct mcs_set_pn_threshold {
	struct mbox_msghdr hdr;
	uint64_t __io threshold;
	uint8_t __io xpn; /* '1' for setting xpn threshold */
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

enum mcs_ctrl_pkt_rule_type {
	MCS_CTRL_PKT_RULE_TYPE_ETH,
	MCS_CTRL_PKT_RULE_TYPE_DA,
	MCS_CTRL_PKT_RULE_TYPE_RANGE,
	MCS_CTRL_PKT_RULE_TYPE_COMBO,
	MCS_CTRL_PKT_RULE_TYPE_MAC,
};

struct mcs_alloc_ctrl_pkt_rule_req {
	struct mbox_msghdr hdr;
	uint8_t __io rule_type;
	uint8_t __io mcs_id; /* MCS block ID */
	uint8_t __io dir;    /* Macsec ingress or egress side */
	uint64_t __io rsvd;
};

struct mcs_alloc_ctrl_pkt_rule_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io rule_idx;
	uint8_t __io rule_type;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_free_ctrl_pkt_rule_req {
	struct mbox_msghdr hdr;
	uint8_t __io rule_idx;
	uint8_t __io rule_type;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint8_t __io all; /* Free all the rule resources */
	uint64_t __io rsvd;
};

struct mcs_ctrl_pkt_rule_write_req {
	struct mbox_msghdr hdr;
	uint64_t __io data0;
	uint64_t __io data1;
	uint64_t __io data2;
	uint8_t __io rule_idx;
	uint8_t __io rule_type;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_port_cfg_set_req {
	struct mbox_msghdr hdr;
	uint8_t __io cstm_tag_rel_mode_sel;
	uint8_t __io custom_hdr_enb;
	uint8_t __io fifo_skid;
	uint8_t __io lmac_mode;
	uint8_t __io lmac_id;
	uint8_t __io mcs_id;
	uint64_t __io rsvd;
};

struct mcs_port_cfg_get_req {
	struct mbox_msghdr hdr;
	uint8_t __io lmac_id;
	uint8_t __io mcs_id;
	uint64_t __io rsvd;
};

struct mcs_port_cfg_get_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io cstm_tag_rel_mode_sel;
	uint8_t __io custom_hdr_enb;
	uint8_t __io fifo_skid;
	uint8_t __io lmac_mode;
	uint8_t __io lmac_id;
	uint8_t __io mcs_id;
	uint64_t __io rsvd;
};

struct mcs_custom_tag_cfg_get_req {
	struct mbox_msghdr hdr;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_custom_tag_cfg_get_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io cstm_etype[8];
	uint8_t __io cstm_indx[8];
	uint8_t __io cstm_etype_en;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_port_reset_req {
	struct mbox_msghdr hdr;
	uint8_t __io reset;
	uint8_t __io mcs_id;
	uint8_t __io lmac_id;
	uint64_t __io rsvd;
};

struct mcs_stats_req {
	struct mbox_msghdr hdr;
	uint8_t __io id;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint64_t __io rsvd;
};

struct mcs_flowid_stats {
	struct mbox_msghdr hdr;
	uint64_t __io tcam_hit_cnt;
	uint64_t __io rsvd;
};

struct mcs_secy_stats {
	struct mbox_msghdr hdr;
	uint64_t __io ctl_pkt_bcast_cnt;
	uint64_t __io ctl_pkt_mcast_cnt;
	uint64_t __io ctl_pkt_ucast_cnt;
	uint64_t __io ctl_octet_cnt;
	uint64_t __io unctl_pkt_bcast_cnt;
	uint64_t __io unctl_pkt_mcast_cnt;
	uint64_t __io unctl_pkt_ucast_cnt;
	uint64_t __io unctl_octet_cnt;
	/* Valid only for RX */
	uint64_t __io octet_decrypted_cnt;
	uint64_t __io octet_validated_cnt;
	uint64_t __io pkt_port_disabled_cnt;
	uint64_t __io pkt_badtag_cnt;
	uint64_t __io pkt_nosa_cnt;
	uint64_t __io pkt_nosaerror_cnt;
	uint64_t __io pkt_tagged_ctl_cnt;
	uint64_t __io pkt_untaged_cnt;
	uint64_t __io pkt_ctl_cnt;   /* CN10K-B */
	uint64_t __io pkt_notag_cnt; /* CNF10K-B */
	/* Valid only for TX */
	uint64_t __io octet_encrypted_cnt;
	uint64_t __io octet_protected_cnt;
	uint64_t __io pkt_noactivesa_cnt;
	uint64_t __io pkt_toolong_cnt;
	uint64_t __io pkt_untagged_cnt;
	uint64_t __io rsvd[4];
};

struct mcs_port_stats {
	struct mbox_msghdr hdr;
	uint64_t __io tcam_miss_cnt;
	uint64_t __io parser_err_cnt;
	uint64_t __io preempt_err_cnt; /* CNF10K-B */
	uint64_t __io sectag_insert_err_cnt;
	uint64_t __io rsvd[4];
};

struct mcs_sc_stats {
	struct mbox_msghdr hdr;
	/* RX */
	uint64_t __io hit_cnt;
	uint64_t __io pkt_invalid_cnt;
	uint64_t __io pkt_late_cnt;
	uint64_t __io pkt_notvalid_cnt;
	uint64_t __io pkt_unchecked_cnt;
	uint64_t __io pkt_delay_cnt;	  /* CNF10K-B */
	uint64_t __io pkt_ok_cnt;	  /* CNF10K-B */
	uint64_t __io octet_decrypt_cnt;  /* CN10K-B */
	uint64_t __io octet_validate_cnt; /* CN10K-B */
	/* TX */
	uint64_t __io pkt_encrypt_cnt;
	uint64_t __io pkt_protected_cnt;
	uint64_t __io octet_encrypt_cnt;   /* CN10K-B */
	uint64_t __io octet_protected_cnt; /* CN10K-B */
	uint64_t __io rsvd[4];
};

struct mcs_clear_stats {
	struct mbox_msghdr hdr;
#define MCS_FLOWID_STATS 0
#define MCS_SECY_STATS	 1
#define MCS_SC_STATS	 2
#define MCS_SA_STATS	 3
#define MCS_PORT_STATS	 4
	uint8_t __io type; /* FLOWID, SECY, SC, SA, PORT */
	/* type = PORT, If id = FF(invalid) port no is derived from pcifunc */
	uint8_t __io id;
	uint8_t __io mcs_id;
	uint8_t __io dir;
	uint8_t __io all; /* All resources stats mapped to PF are cleared */
};

struct mcs_fips_req {
	struct mbox_msghdr hdr;
	uint8_t __io mcs_id;
	uint8_t __io dir;
};

struct mcs_fips_mode_req {
	struct mbox_msghdr hdr;
	uint64_t __io mode;
	uint8_t __io mcs_id;
	uint8_t __io dir;
};

struct mcs_fips_ctl_req {
	struct mbox_msghdr hdr;
	uint64_t __io ctl;
	uint8_t __io mcs_id;
	uint8_t __io dir;
};

struct mcs_fips_iv_req {
	struct mbox_msghdr hdr;
	uint32_t __io iv_bits95_64;
	uint64_t __io iv_bits63_0;
	uint8_t __io mcs_id;
	uint8_t __io dir;
};

struct mcs_fips_ctr_req {
	struct mbox_msghdr hdr;
	uint32_t __io fips_ctr;
	uint8_t __io mcs_id;
	uint8_t __io dir;
};

struct mcs_fips_key_req {
	struct mbox_msghdr hdr;
	uint64_t __io sak_bits255_192;
	uint64_t __io sak_bits191_128;
	uint64_t __io sak_bits127_64;
	uint64_t __io sak_bits63_0;
	uint64_t __io hashkey_bits127_64;
	uint64_t __io hashkey_bits63_0;
	uint8_t __io sak_len;
	uint8_t __io mcs_id;
	uint8_t __io dir;
};

struct mcs_fips_block_req {
	struct mbox_msghdr hdr;
	uint64_t __io blk_bits127_64;
	uint64_t __io blk_bits63_0;
	uint8_t __io mcs_id;
	uint8_t __io dir;
};

struct mcs_fips_result_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io blk_bits127_64;
	uint64_t __io blk_bits63_0;
	uint64_t __io icv_bits127_64;
	uint64_t __io icv_bits63_0;
	uint8_t __io result_pass;
};

/* NPA mbox message formats */

/* NPA mailbox error codes
 * Range 301 - 400.
 */
enum npa_af_status {
	NPA_AF_ERR_PARAM = -301,
	NPA_AF_ERR_AQ_FULL = -302,
	NPA_AF_ERR_AQ_ENQUEUE = -303,
	NPA_AF_ERR_AF_LF_INVALID = -304,
	NPA_AF_ERR_AF_LF_ALLOC = -305,
	NPA_AF_ERR_LF_RESET = -306,
};

#define NPA_AURA_SZ_0	 0
#define NPA_AURA_SZ_128	 1
#define NPA_AURA_SZ_256	 2
#define NPA_AURA_SZ_512	 3
#define NPA_AURA_SZ_1K	 4
#define NPA_AURA_SZ_2K	 5
#define NPA_AURA_SZ_4K	 6
#define NPA_AURA_SZ_8K	 7
#define NPA_AURA_SZ_16K	 8
#define NPA_AURA_SZ_32K	 9
#define NPA_AURA_SZ_64K	 10
#define NPA_AURA_SZ_128K 11
#define NPA_AURA_SZ_256K 12
#define NPA_AURA_SZ_512K 13
#define NPA_AURA_SZ_1M	 14
#define NPA_AURA_SZ_MAX	 15

/* For NPA LF context alloc and init */
struct npa_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __io node;
	int __io aura_sz;	/* No of auras. See NPA_AURA_SZ_* */
	uint32_t __io nr_pools; /* No of pools */
	uint64_t __io way_mask;
};

struct npa_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint32_t __io stack_pg_ptrs;  /* No of ptrs per stack page */
	uint32_t __io stack_pg_bytes; /* Size of stack page */
	uint16_t __io qints;	      /* NPA_AF_CONST::QINTS */
	uint8_t __io cache_lines;     /* Batch Alloc DMA */
};

/* NPA AQ enqueue msg */
struct npa_aq_enq_req {
	struct mbox_msghdr hdr;
	uint32_t __io aura_id;
	uint8_t __io ctype;
	uint8_t __io op;
	union {
		/* Valid when op == WRITE/INIT and ctype == AURA.
		 * LF fills the pool_id in aura.pool_addr. AF will translate
		 * the pool_id to pool context pointer.
		 */
		__io struct npa_aura_s aura;
		/* Valid when op == WRITE/INIT and ctype == POOL */
		__io struct npa_pool_s pool;
	};
	/* Mask data when op == WRITE (1=write, 0=don't write) */
	union {
		/* Valid when op == WRITE and ctype == AURA */
		__io struct npa_aura_s aura_mask;
		/* Valid when op == WRITE and ctype == POOL */
		__io struct npa_pool_s pool_mask;
	};
};

struct npa_aq_enq_rsp {
	struct mbox_msghdr hdr;
	union {
		/* Valid when op == READ and ctype == AURA */
		__io struct npa_aura_s aura;
		/* Valid when op == READ and ctype == POOL */
		__io struct npa_pool_s pool;
	};
};

struct npa_cn20k_aq_enq_req {
	struct mbox_msghdr hdr;
	uint32_t __io aura_id;
	uint8_t __io ctype;
	uint8_t __io op;
	union {
		/* Valid when op == WRITE/INIT and ctype == AURA */
		__io struct npa_cn20k_aura_s aura;
		/* Valid when op == WRITE/INIT and ctype == POOL */
		__io struct npa_cn20k_pool_s pool;
	};
	/* Mask data when op == WRITE (1=write, 0=don't write) */
	union {
		/* Valid when op == WRITE and ctype == AURA */
		__io struct npa_cn20k_aura_s aura_mask;
		/* Valid when op == WRITE and ctype == POOL */
		__io struct npa_cn20k_pool_s pool_mask;
	};
};

struct npa_cn20k_aq_enq_rsp {
	struct mbox_msghdr hdr;
	union {
		/* Valid when op == READ and ctype == AURA */
		__io struct npa_cn20k_aura_s aura;
		/* Valid when op == READ and ctype == POOL */
		__io struct npa_cn20k_pool_s pool;
	};
};

/* Disable all contexts of type 'ctype' */
struct hwctx_disable_req {
	struct mbox_msghdr hdr;
	uint8_t __io ctype;
};

/* NIX mbox message formats */

/* NIX mailbox error codes
 * Range 401 - 500.
 */
enum nix_af_status {
	NIX_AF_ERR_PARAM = -401,
	NIX_AF_ERR_AQ_FULL = -402,
	NIX_AF_ERR_AQ_ENQUEUE = -403,
	NIX_AF_ERR_AF_LF_INVALID = -404,
	NIX_AF_ERR_AF_LF_ALLOC = -405,
	NIX_AF_ERR_TLX_ALLOC_FAIL = -406,
	NIX_AF_ERR_TLX_INVALID = -407,
	NIX_AF_ERR_RSS_SIZE_INVALID = -408,
	NIX_AF_ERR_RSS_GRPS_INVALID = -409,
	NIX_AF_ERR_FRS_INVALID = -410,
	NIX_AF_ERR_RX_LINK_INVALID = -411,
	NIX_AF_INVAL_TXSCHQ_CFG = -412,
	NIX_AF_SMQ_FLUSH_FAILED = -413,
	NIX_AF_ERR_LF_RESET = -414,
	NIX_AF_ERR_RSS_NOSPC_FIELD = -415,
	NIX_AF_ERR_RSS_NOSPC_ALGO = -416,
	NIX_AF_ERR_MARK_CFG_FAIL = -417,
	NIX_AF_ERR_LSO_CFG_FAIL = -418,
	NIX_AF_INVAL_NPA_PF_FUNC = -419,
	NIX_AF_INVAL_SSO_PF_FUNC = -420,
	NIX_AF_ERR_TX_VTAG_NOSPC = -421,
	NIX_AF_ERR_RX_VTAG_INUSE = -422,
	NIX_AF_ERR_PTP_CONFIG_FAIL = -423,
};

/* For NIX LF context alloc and init */
struct nix_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __io node;
	uint32_t __io rq_cnt; /* No of receive queues */
	uint32_t __io sq_cnt; /* No of send queues */
	uint32_t __io cq_cnt; /* No of completion queues */
	uint8_t __io xqe_sz;
	uint16_t __io rss_sz;
	uint8_t __io rss_grps;
	uint16_t __io npa_func;
	/* RVU_DEFAULT_PF_FUNC == default pf_func associated with lf */
	uint16_t __io sso_func;
	uint64_t __io rx_cfg; /* See NIX_AF_LF(0..127)_RX_CFG */
	uint64_t __io way_mask;
#define NIX_LF_RSS_TAG_LSB_AS_ADDER BIT_ULL(0)
#define NIX_LF_LBK_BLK_SEL	    BIT_ULL(1)
	uint64_t __io flags;
};

struct nix_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io sqb_size;
	uint16_t __io rx_chan_base;
	uint16_t __io tx_chan_base;
	uint8_t __io rx_chan_cnt; /* Total number of RX channels */
	uint8_t __io tx_chan_cnt; /* Total number of TX channels */
	uint8_t __io lso_tsov4_idx;
	uint8_t __io lso_tsov6_idx;
	uint8_t __io mac_addr[PLT_ETHER_ADDR_LEN];
	uint8_t __io lf_rx_stats;     /* NIX_AF_CONST1::LF_RX_STATS */
	uint8_t __io lf_tx_stats;     /* NIX_AF_CONST1::LF_TX_STATS */
	uint16_t __io cints;	      /* NIX_AF_CONST2::CINTS */
	uint16_t __io qints;	      /* NIX_AF_CONST2::QINTS */
	uint8_t __io hw_rx_tstamp_en; /*set if rx timestamping enabled */
	uint8_t __io cgx_links;	      /* No. of CGX links present in HW */
	uint8_t __io lbk_links;	      /* No. of LBK links present in HW */
	uint8_t __io sdp_links;	      /* No. of SDP links present in HW */
	uint8_t __io tx_link;	      /* Transmit channel link number */
};

struct nix_lf_free_req {
	struct mbox_msghdr hdr;
#define NIX_LF_DISABLE_FLOWS	 BIT_ULL(0)
#define NIX_LF_DONT_FREE_TX_VTAG BIT_ULL(1)
	uint64_t __io flags;
};

/* CN20x NIX AQ enqueue msg */
struct nix_cn20k_aq_enq_req {
	struct mbox_msghdr hdr;
	uint32_t __io qidx;
	uint8_t __io ctype;
	uint8_t __io op;
	union {
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RQ */
		__io struct nix_cn20k_rq_ctx_s rq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_SQ */
		__io struct nix_cn20k_sq_ctx_s sq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_CQ */
		__io struct nix_cn20k_cq_ctx_s cq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RSS */
		__io struct nix_rsse_s rss;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_MCE */
		__io struct nix_rx_mce_s mce;
		/* Valid when op == WRITE/INIT and
		 * ctype == NIX_AQ_CTYPE_BAND_PROF
		 */
		__io struct nix_band_prof_s prof;
	};
	/* Mask data when op == WRITE (1=write, 0=don't write) */
	union {
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RQ */
		__io struct nix_cn20k_rq_ctx_s rq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_SQ */
		__io struct nix_cn20k_sq_ctx_s sq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_CQ */
		__io struct nix_cn20k_cq_ctx_s cq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RSS */
		__io struct nix_rsse_s rss_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_MCE */
		__io struct nix_rx_mce_s mce_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_BAND_PROF */
		__io struct nix_band_prof_s prof_mask;
	};
};

struct nix_cn20k_aq_enq_rsp {
	struct mbox_msghdr hdr;
	union {
		__io struct nix_cn20k_rq_ctx_s rq;
		__io struct nix_cn20k_sq_ctx_s sq;
		__io struct nix_cn20k_cq_ctx_s cq;
		__io struct nix_rsse_s rss;
		__io struct nix_rx_mce_s mce;
		__io struct nix_band_prof_s prof;
	};
};

/* CN10x NIX AQ enqueue msg */
struct nix_cn10k_aq_enq_req {
	struct mbox_msghdr hdr;
	uint32_t __io qidx;
	uint8_t __io ctype;
	uint8_t __io op;
	union {
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RQ */
		__io struct nix_cn10k_rq_ctx_s rq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_SQ */
		__io struct nix_cn10k_sq_ctx_s sq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_CQ */
		__io struct nix_cq_ctx_s cq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RSS */
		__io struct nix_rsse_s rss;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_MCE */
		__io struct nix_rx_mce_s mce;
		/* Valid when op == WRITE/INIT and
		 * ctype == NIX_AQ_CTYPE_BAND_PROF
		 */
		__io struct nix_band_prof_s prof;
	};
	/* Mask data when op == WRITE (1=write, 0=don't write) */
	union {
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RQ */
		__io struct nix_cn10k_rq_ctx_s rq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_SQ */
		__io struct nix_cn10k_sq_ctx_s sq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_CQ */
		__io struct nix_cq_ctx_s cq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RSS */
		__io struct nix_rsse_s rss_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_MCE */
		__io struct nix_rx_mce_s mce_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_BAND_PROF */
		__io struct nix_band_prof_s prof_mask;
	};
};

struct nix_cn10k_aq_enq_rsp {
	struct mbox_msghdr hdr;
	union {
		__io struct nix_cn10k_rq_ctx_s rq;
		__io struct nix_cn10k_sq_ctx_s sq;
		__io struct nix_cq_ctx_s cq;
		__io struct nix_rsse_s rss;
		__io struct nix_rx_mce_s mce;
		__io struct nix_band_prof_s prof;
	};
};

/* NIX AQ enqueue msg */
struct nix_aq_enq_req {
	struct mbox_msghdr hdr;
	uint32_t __io qidx;
	uint8_t __io ctype;
	uint8_t __io op;
	union {
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RQ */
		__io struct nix_rq_ctx_s rq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_SQ */
		__io struct nix_sq_ctx_s sq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_CQ */
		__io struct nix_cq_ctx_s cq;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_RSS */
		__io struct nix_rsse_s rss;
		/* Valid when op == WRITE/INIT and ctype == NIX_AQ_CTYPE_MCE */
		__io struct nix_rx_mce_s mce;
	};
	/* Mask data when op == WRITE (1=write, 0=don't write) */
	union {
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RQ */
		__io struct nix_rq_ctx_s rq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_SQ */
		__io struct nix_sq_ctx_s sq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_CQ */
		__io struct nix_cq_ctx_s cq_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_RSS */
		__io struct nix_rsse_s rss_mask;
		/* Valid when op == WRITE and ctype == NIX_AQ_CTYPE_MCE */
		__io struct nix_rx_mce_s mce_mask;
	};
};

struct nix_aq_enq_rsp {
	struct mbox_msghdr hdr;
	union {
		__io struct nix_rq_ctx_s rq;
		__io struct nix_sq_ctx_s sq;
		__io struct nix_cq_ctx_s cq;
		__io struct nix_rsse_s rss;
		__io struct nix_rx_mce_s mce;
	};
};

/* Tx scheduler/shaper mailbox messages */

#define MAX_TXSCHQ_PER_FUNC 128

struct nix_txsch_alloc_req {
	struct mbox_msghdr hdr;
	/* Scheduler queue count request at each level */
	uint16_t __io schq_contig[NIX_TXSCH_LVL_CNT]; /* Contig. queues */
	uint16_t __io schq[NIX_TXSCH_LVL_CNT];	      /* Non-Contig. queues */
};

struct nix_txsch_alloc_rsp {
	struct mbox_msghdr hdr;
	/* Scheduler queue count allocated at each level */
	uint16_t __io schq_contig[NIX_TXSCH_LVL_CNT]; /* Contig. queues */
	uint16_t __io schq[NIX_TXSCH_LVL_CNT];	      /* Non-Contig. queues */
	/* Scheduler queue list allocated at each level */
	uint16_t __io schq_contig_list[NIX_TXSCH_LVL_CNT][MAX_TXSCHQ_PER_FUNC];
	uint16_t __io schq_list[NIX_TXSCH_LVL_CNT][MAX_TXSCHQ_PER_FUNC];
	/* Traffic aggregation scheduler level */
	uint8_t __io aggr_level;
	/* Aggregation lvl's RR_PRIO config */
	uint8_t __io aggr_lvl_rr_prio;
	/* LINKX_CFG CSRs mapped to TL3 or TL2's index ? */
	uint8_t __io link_cfg_lvl;
};

struct nix_txsch_free_req {
	struct mbox_msghdr hdr;
#define TXSCHQ_FREE_ALL BIT_ULL(0)
	uint16_t __io flags;
	/* Scheduler queue level to be freed */
	uint16_t __io schq_lvl;
	/* List of scheduler queues to be freed */
	uint16_t __io schq;
};

struct nix_txschq_config {
	struct mbox_msghdr hdr;
	uint8_t __io lvl; /* SMQ/MDQ/TL4/TL3/TL2/TL1 */
	uint8_t __io read;
#define TXSCHQ_IDX_SHIFT       16
#define TXSCHQ_IDX_MASK	       (BIT_ULL(10) - 1)
#define TXSCHQ_IDX(reg, shift) (((reg) >> (shift)) & TXSCHQ_IDX_MASK)
	uint8_t __io num_regs;
#define MAX_REGS_PER_MBOX_MSG 20
	uint64_t __io reg[MAX_REGS_PER_MBOX_MSG];
	uint64_t __io regval[MAX_REGS_PER_MBOX_MSG];
	/* All 0's => overwrite with new value */
	uint64_t __io regval_mask[MAX_REGS_PER_MBOX_MSG];
};

struct nix_vtag_config {
	struct mbox_msghdr hdr;
	/* '0' for 4 octet VTAG, '1' for 8 octet VTAG */
	uint8_t __io vtag_size;
	/* cfg_type is '0' for tx vlan cfg
	 * cfg_type is '1' for rx vlan cfg
	 */
	uint8_t __io cfg_type;
	union {
		/* Valid when cfg_type is '0' */
		struct {
			uint64_t __io vtag0;
			uint64_t __io vtag1;

			/* cfg_vtag0 & cfg_vtag1 fields are valid
			 * when free_vtag0 & free_vtag1 are '0's.
			 */
			/* cfg_vtag0 = 1 to configure vtag0 */
			uint8_t __io cfg_vtag0 : 1;
			/* cfg_vtag1 = 1 to configure vtag1 */
			uint8_t __io cfg_vtag1 : 1;

			/* vtag0_idx & vtag1_idx are only valid when
			 * both cfg_vtag0 & cfg_vtag1 are '0's,
			 * these fields are used along with free_vtag0
			 * & free_vtag1 to free the nix lf's tx_vlan
			 * configuration.
			 *
			 * Denotes the indices of tx_vtag def registers
			 * that needs to be cleared and freed.
			 */
			int __io vtag0_idx;
			int __io vtag1_idx;

			/* Free_vtag0 & free_vtag1 fields are valid
			 * when cfg_vtag0 & cfg_vtag1 are '0's.
			 */
			/* Free_vtag0 = 1 clears vtag0 configuration
			 * vtag0_idx denotes the index to be cleared.
			 */
			uint8_t __io free_vtag0 : 1;
			/* Free_vtag1 = 1 clears vtag1 configuration
			 * vtag1_idx denotes the index to be cleared.
			 */
			uint8_t __io free_vtag1 : 1;
		} tx;

		/* Valid when cfg_type is '1' */
		struct {
			/* Rx vtag type index, valid values are in 0..7 range */
			uint8_t __io vtag_type;
			/* Rx vtag strip */
			uint8_t __io strip_vtag : 1;
			/* Rx vtag capture */
			uint8_t __io capture_vtag : 1;
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
	int __io vtag0_idx;
	int __io vtag1_idx;
};

struct nix_rss_flowkey_cfg {
	struct mbox_msghdr hdr;
	int __io mcam_index;	   /* MCAM entry index to modify */
	uint32_t __io flowkey_cfg; /* Flowkey types selected */
#define FLOW_KEY_TYPE_PORT	    BIT(0)
#define FLOW_KEY_TYPE_IPV4	    BIT(1)
#define FLOW_KEY_TYPE_IPV6	    BIT(2)
#define FLOW_KEY_TYPE_TCP	    BIT(3)
#define FLOW_KEY_TYPE_UDP	    BIT(4)
#define FLOW_KEY_TYPE_SCTP	    BIT(5)
#define FLOW_KEY_TYPE_NVGRE	    BIT(6)
#define FLOW_KEY_TYPE_VXLAN	    BIT(7)
#define FLOW_KEY_TYPE_GENEVE	    BIT(8)
#define FLOW_KEY_TYPE_ETH_DMAC	    BIT(9)
#define FLOW_KEY_TYPE_IPV6_EXT	    BIT(10)
#define FLOW_KEY_TYPE_GTPU	    BIT(11)
#define FLOW_KEY_TYPE_INNR_IPV4	    BIT(12)
#define FLOW_KEY_TYPE_INNR_IPV6	    BIT(13)
#define FLOW_KEY_TYPE_INNR_TCP	    BIT(14)
#define FLOW_KEY_TYPE_INNR_UDP	    BIT(15)
#define FLOW_KEY_TYPE_INNR_SCTP	    BIT(16)
#define FLOW_KEY_TYPE_INNR_ETH_DMAC BIT(17)
#define FLOW_KEY_TYPE_CH_LEN_90B    BIT(18)
#define FLOW_KEY_TYPE_CUSTOM0	    BIT(19)
#define FLOW_KEY_TYPE_VLAN	    BIT(20)
#define FLOW_KEY_TYPE_ESP	    BIT(23)
#define FLOW_KEY_TYPE_ROCEV2	    BIT(24)
#define FLOW_KEY_TYPE_L4_DST	    BIT(28)
#define FLOW_KEY_TYPE_L4_SRC	    BIT(29)
#define FLOW_KEY_TYPE_L3_DST	    BIT(30)
#define FLOW_KEY_TYPE_L3_SRC	    BIT(31)
	uint8_t __io group; /* RSS context or group */
};

struct nix_rss_flowkey_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io alg_idx; /* Selected algo index */
};

struct nix_set_mac_addr {
	struct mbox_msghdr hdr;
	uint8_t __io mac_addr[PLT_ETHER_ADDR_LEN];
};

struct nix_get_mac_addr_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io mac_addr[PLT_ETHER_ADDR_LEN];
};

struct nix_mark_format_cfg {
	struct mbox_msghdr hdr;
	uint8_t __io offset;
	uint8_t __io y_mask;
	uint8_t __io y_val;
	uint8_t __io r_mask;
	uint8_t __io r_val;
};

struct nix_mark_format_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io mark_format_idx;
};

struct nix_rq_cpt_field_mask_cfg_req {
	struct mbox_msghdr hdr;
#define RQ_CTX_MASK_MAX 6
	union {
		uint64_t __io rq_ctx_word_set[RQ_CTX_MASK_MAX];
		__io struct nix_cn10k_rq_ctx_s rq_set;
	};
	union {
		uint64_t __io rq_ctx_word_mask[RQ_CTX_MASK_MAX];
		__io struct nix_cn10k_rq_ctx_s rq_mask;
	};
	struct nix_lf_rx_ipec_cfg1_req {
		uint32_t __io spb_cpt_aura;
		uint8_t __io rq_mask_enable;
		uint8_t __io spb_cpt_sizem1;
		uint8_t __io spb_cpt_enable;
	} ipsec_cfg1;
};

struct nix_lso_format_cfg {
	struct mbox_msghdr hdr;
	uint64_t __io field_mask;
	uint64_t __io fields[NIX_LSO_FIELD_MAX];
};

struct nix_lso_format_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io lso_format_idx;
};

struct nix_rx_mode {
	struct mbox_msghdr hdr;
#define NIX_RX_MODE_UCAST    BIT(0)
#define NIX_RX_MODE_PROMISC  BIT(1)
#define NIX_RX_MODE_ALLMULTI BIT(2)
#define NIX_RX_MODE_USE_MCE  BIT(3)
	uint16_t __io mode;
};

struct nix_rx_cfg {
	struct mbox_msghdr hdr;
#define NIX_RX_OL3_VERIFY BIT(0)
#define NIX_RX_OL4_VERIFY BIT(1)
#define NIX_RX_DROP_RE	  BIT(2)
	uint8_t __io len_verify; /* Outer L3/L4 len check */
#define NIX_RX_CSUM_OL4_VERIFY BIT(0)
	uint8_t __io csum_verify; /* Outer L4 checksum verification */
};

struct nix_frs_cfg {
	struct mbox_msghdr hdr;
	uint8_t __io update_smq;    /* Update SMQ's min/max lens */
	uint8_t __io update_minlen; /* Set minlen also */
	uint8_t __io sdp_link;	    /* Set SDP RX link */
	uint16_t __io maxlen;
	uint16_t __io minlen;
};

struct nix_set_vlan_tpid {
	struct mbox_msghdr hdr;
#define NIX_VLAN_TYPE_INNER 0
#define NIX_VLAN_TYPE_OUTER 1
	uint8_t __io vlan_type;
	uint16_t __io tpid;
};

struct nix_bp_cfg_req {
	struct mbox_msghdr hdr;
	uint16_t __io chan_base; /* Starting channel number */
	uint8_t __io chan_cnt;	 /* Number of channels */
	uint8_t __io bpid_per_chan;
	/* bpid_per_chan = 0  assigns single bp id for range of channels */
	/* bpid_per_chan = 1 assigns separate bp id for each channel */
};

/* PF can be mapped to either CGX or LBK or SDP interface,
 * so maximum 256 channels are possible.
 */
#define NIX_MAX_CHAN	 256
#define NIX_CGX_MAX_CHAN 8
#define NIX_LBK_MAX_CHAN 1
struct nix_bp_cfg_rsp {
	struct mbox_msghdr hdr;
	/* Channel and bpid mapping */
	uint16_t __io chan_bpid[NIX_MAX_CHAN];
	/* Number of channel for which bpids are assigned */
	uint8_t __io chan_cnt;
};

struct nix_alloc_bpid_req {
	struct mbox_msghdr hdr;
	uint8_t __io bpid_cnt;
	uint8_t __io type;
	uint64_t __io rsvd;
};

struct nix_bpids {
#define ROC_NIX_MAX_BPID_CNT	8
	struct mbox_msghdr hdr;
	uint8_t __io bpid_cnt;
	uint16_t __io bpids[ROC_NIX_MAX_BPID_CNT];
	uint64_t __io rsvd;
};

struct nix_rx_chan_cfg {
	struct mbox_msghdr hdr;
	uint8_t __io type; /* Interface type(CGX/CPT/LBK) */
	uint8_t __io read;
	uint16_t __io chan; /* RX channel to be configured */
	uint64_t __io val; /* NIX_AF_RX_CHAN_CFG value */
	uint64_t __io rsvd;
};

struct nix_mcast_grp_create_req {
	struct mbox_msghdr hdr;
#define NIX_MCAST_INGRESS 0
#define NIX_MCAST_EGRESS  1
	uint8_t __io dir;
	uint8_t __io reserved[11];
	/* Reserving few bytes for future requirement */
};

struct nix_mcast_grp_create_rsp {
	struct mbox_msghdr hdr;
	/* This mcast_grp_idx should be passed during MCAM
	 * write entry for multicast. AF will identify the
	 * corresponding multicast table index associated
	 * with the group id and program the same to MCAM entry.
	 * This group id is also needed during group delete
	 * and update request.
	 */
	uint32_t __io mcast_grp_idx;
};
struct nix_mcast_grp_destroy_req {
	struct mbox_msghdr hdr;
	/* Group id returned by nix_mcast_grp_create_rsp */
	uint32_t __io mcast_grp_idx;
};

struct nix_mcast_grp_update_req {
	struct mbox_msghdr hdr;
	/* Group id returned by nix_mcast_grp_create_rsp */
	uint32_t __io mcast_grp_idx;
	/* Number of multicast/mirror entries requested */
	uint32_t __io num_mce_entry;
#define NIX_MCE_ENTRY_MAX 64
#define NIX_RX_RQ	  0
#define NIX_RX_RSS	  1
	/* Receive queue or RSS index within pf_func */
	uint32_t __io rq_rss_index[NIX_MCE_ENTRY_MAX];
	uint16_t __io pcifunc[NIX_MCE_ENTRY_MAX];
	uint16_t __io channel[NIX_MCE_ENTRY_MAX];
#define NIX_MCAST_OP_ADD_ENTRY 0
#define NIX_MCAST_OP_DEL_ENTRY 1
	/* Destination type. 0:Receive queue, 1:RSS*/
	uint8_t __io dest_type[NIX_MCE_ENTRY_MAX];
	uint8_t __io op;
};

struct nix_mcast_grp_update_rsp {
	struct mbox_msghdr hdr;
	uint32_t __io mce_start_index;
};

#define IPSEC_GEN_CFG_EGRP   GENMASK_ULL(50, 48)
#define IPSEC_GEN_CFG_OPCODE GENMASK_ULL(47, 32)
#define IPSEC_GEN_CFG_PARAM1 GENMASK_ULL(31, 16)
#define IPSEC_GEN_CFG_PARAM2 GENMASK_ULL(15, 0)

#define CPT_INST_QSEL_BLOCK   GENMASK_ULL(28, 24)
#define CPT_INST_QSEL_PF_FUNC GENMASK_ULL(23, 8)
#define CPT_INST_QSEL_SLOT    GENMASK_ULL(7, 0)

#define CPT_INST_CREDIT_HYST GENMASK_ULL(61, 56)
#define CPT_INST_CREDIT_TH   GENMASK_ULL(53, 32)
#define CPT_INST_CREDIT_BPID GENMASK_ULL(30, 22)
#define CPT_INST_CREDIT_CNT  GENMASK_ULL(21, 0)

/* Per queue NIX inline IPSec configuration */
struct nix_rx_inline_qcfg_req {
	struct mbox_msghdr hdr;
	uint32_t __io cpt_credit;
	uint32_t __io credit_th;
	uint16_t __io cpt_pf_func;
	uint16_t __io bpid;
	uint8_t __io cpt_slot;
	uint8_t __io rx_queue_id;
	uint8_t __io enable;
	uint8_t __io hysteresis;
	uint8_t __io rsvd[32];
};

struct nix_get_lf_stats_req {
	struct mbox_msghdr hdr;
	uint16_t __io pcifunc;
	uint64_t __io rsvd;
};

struct nix_lf_stats_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io pcifunc;
	struct {
		uint64_t __io octs;
		uint64_t __io ucast;
		uint64_t __io bcast;
		uint64_t __io mcast;
		uint64_t __io drop;
		uint64_t __io drop_octs;
		uint64_t __io drop_mcast;
		uint64_t __io drop_bcast;
		uint64_t __io err;
		uint64_t __io rsvd[5];
	} rx;
	struct {
		uint64_t __io ucast;
		uint64_t __io bcast;
		uint64_t __io mcast;
		uint64_t __io drop;
		uint64_t __io octs;
	} tx;
};

/* Global NIX inline IPSec configuration */
struct nix_inline_ipsec_cfg {
	struct mbox_msghdr hdr;
	uint32_t __io cpt_credit;
	struct {
		uint8_t __io egrp;
		uint16_t __io opcode;
		uint16_t __io param1;
		uint16_t __io param2;
	} gen_cfg;
	struct {
		uint16_t __io cpt_pf_func;
		uint8_t __io cpt_slot;
	} inst_qsel;
	uint8_t __io enable;
	uint16_t __io bpid;
	uint32_t __io credit_th;
};

#define NIX_RX_INL_PROFILE_PROTO_CNT 9
struct nix_rx_inl_profile_cfg_req {
	struct mbox_msghdr hdr;
	uint64_t __io def_cfg;
	uint64_t __io extract_cfg;
	uint64_t __io gen_cfg;
	uint64_t __io prot_field_cfg[NIX_RX_INL_PROFILE_PROTO_CNT];
	uint64_t __io rsvd[32];		/* reserved fields for future expansion */
};

struct nix_rx_inl_profile_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io profile_id;
	uint8_t __io rsvd[32];		/* reserved fields for future expansion */
};

struct nix_rx_inl_lf_cfg_req {
	struct mbox_msghdr hdr;
	uint64_t __io rx_inline_cfg0;
	uint64_t __io rx_inline_cfg1;
	uint64_t __io rx_inline_sa_base;
	uint8_t __io enable;
	uint8_t __io profile_id;
	uint8_t __io rsvd[32];		/* reserved fields for future expansion */
};

/* Per NIX LF inline IPSec configuration */
struct nix_inline_ipsec_lf_cfg {
	struct mbox_msghdr hdr;
	uint64_t __io sa_base_addr;
	struct {
		uint32_t __io tag_const;
		uint16_t __io lenm1_max;
		uint8_t __io sa_pow2_size;
		uint8_t __io tt;
	} ipsec_cfg0;
	struct {
		uint32_t __io sa_idx_max;
		uint8_t __io sa_idx_w;
	} ipsec_cfg1;
	uint8_t __io enable;
	struct {
		uint8_t __io res_addr_offset;
		uint8_t __io res_addr_offset_valid;
	} ipsec_cfg0_ext;
};

struct nix_hw_info {
	struct mbox_msghdr hdr;
	uint16_t __io vwqe_delay;
	uint16_t __io max_mtu;
	uint16_t __io min_mtu;
	uint32_t __io rpm_dwrr_mtu;
	uint32_t __io sdp_dwrr_mtu;
	uint32_t __io lbk_dwrr_mtu;
	uint32_t __io rsvd32[1];
	uint64_t __io rsvd[15]; /* Add reserved fields for future expansion */
};

struct nix_bandprof_alloc_req {
	struct mbox_msghdr hdr;
	/* Count of profiles needed per layer */
	uint16_t __io prof_count[NIX_RX_BAND_PROF_LAYER_MAX];
};

struct nix_bandprof_alloc_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io prof_count[NIX_RX_BAND_PROF_LAYER_MAX];

#define BANDPROF_PER_PFFUNC 64
	uint16_t __io prof_idx[NIX_RX_BAND_PROF_LAYER_MAX][BANDPROF_PER_PFFUNC];
};

struct nix_bandprof_free_req {
	struct mbox_msghdr hdr;
	uint8_t __io free_all;
	uint16_t __io prof_count[NIX_RX_BAND_PROF_LAYER_MAX];
	uint16_t __io prof_idx[NIX_RX_BAND_PROF_LAYER_MAX][BANDPROF_PER_PFFUNC];
};

struct nix_bandprof_get_hwinfo_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io prof_count[NIX_RX_BAND_PROF_LAYER_MAX];
	uint32_t __io policer_timeunit;
};

struct nix_lso_alt_flags_cfg_req {
	struct mbox_msghdr hdr;
	uint64_t __io cfg;
	uint64_t __io cfg1;
};

struct nix_lso_alt_flags_cfg_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io lso_alt_flags_idx;
};

/* SSO mailbox error codes
 * Range 501 - 600.
 */
enum sso_af_status {
	SSO_AF_ERR_PARAM = -501,
	SSO_AF_ERR_LF_INVALID = -502,
	SSO_AF_ERR_AF_LF_ALLOC = -503,
	SSO_AF_ERR_GRP_EBUSY = -504,
	SSO_AF_INVAL_NPA_PF_FUNC = -505,
};

struct sso_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __io node;
	uint16_t __io hwgrps;
};

struct sso_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint32_t __io xaq_buf_size;
	uint32_t __io xaq_wq_entries;
	uint32_t __io in_unit_entries;
	uint16_t __io hwgrps;
};

struct sso_lf_free_req {
	struct mbox_msghdr hdr;
	int __io node;
	uint16_t __io hwgrps;
};

/* SSOW mailbox error codes
 * Range 601 - 700.
 */
enum ssow_af_status {
	SSOW_AF_ERR_PARAM = -601,
	SSOW_AF_ERR_LF_INVALID = -602,
	SSOW_AF_ERR_AF_LF_ALLOC = -603,
};

struct ssow_lf_alloc_req {
	struct mbox_msghdr hdr;
	int __io node;
	uint16_t __io hws;
};

struct ssow_lf_free_req {
	struct mbox_msghdr hdr;
	int __io node;
	uint16_t __io hws;
};

#define SSOW_INVAL_SELECTIVE_VER 0x1000
struct ssow_lf_inv_req {
	struct mbox_msghdr hdr;
	uint16_t __io nb_hws;		      /* Number of HWS to invalidate*/
	uint16_t __io hws[MAX_RVU_BLKLF_CNT]; /* Array of HWS */
};

struct ssow_config_lsw {
	struct mbox_msghdr hdr;
#define SSOW_LSW_DIS	 0
#define SSOW_LSW_GW_WAIT 1
#define SSOW_LSW_GW_IMM	 2
	uint8_t __io lsw_mode;
#define SSOW_WQE_REL_LSW_WAIT 0
#define SSOW_WQE_REL_IMM      1
	uint8_t __io wqe_release;
};

struct ssow_chng_mship {
	struct mbox_msghdr hdr;
	uint8_t __io set;	 /* Membership set to modify. */
	uint8_t __io enable;	 /* Enable/Disable the hwgrps. */
	uint8_t __io hws;	 /* HWS to modify. */
	uint16_t __io nb_hwgrps; /* Number of hwgrps in the array */
	uint16_t __io hwgrps[MAX_RVU_BLKLF_CNT]; /* Array of hwgrps. */
};

struct sso_feat_info {
	uint8_t __io hw_flr : 1;
	uint8_t __io hw_prefetch : 1;
	uint8_t __io sw_prefetch : 1;
	uint8_t __io lsw : 1;
	uint8_t __io fwd_grp : 1;
	uint8_t __io eva_present : 1;
	uint8_t __io no_nsched : 1;
	uint8_t __io tag_cfg : 1;
	uint8_t __io gwc_per_core;
	uint16_t __io hws;
	uint16_t __io hwgrps;
	uint16_t __io hwgrps_per_pf;
	uint16_t __io iue;
	uint16_t __io taq_lines;
	uint16_t __io taq_ent_per_line;
	uint16_t __io xaq_buf_size;
	uint16_t __io xaq_wq_entries;
	uint32_t __io eva_ctx_per_hwgrp;
	uint64_t __io rsvd[2];
};

struct sso_hw_info {
	struct mbox_msghdr hdr;
	struct sso_feat_info feat;
};

struct sso_hw_setconfig {
	struct mbox_msghdr hdr;
	uint32_t __io npa_aura_id;
	uint16_t __io npa_pf_func;
	uint16_t __io hwgrps;
};

struct sso_hw_xaq_release {
	struct mbox_msghdr hdr;
	uint16_t __io hwgrps;
};

struct sso_info_req {
	struct mbox_msghdr hdr;
	union {
		uint16_t __io grp;
		uint16_t __io hws;
	};
};

struct sso_grp_priority {
	struct mbox_msghdr hdr;
	uint16_t __io grp;
	uint8_t __io priority;
	uint8_t __io affinity;
	uint8_t __io weight;
};

struct sso_grp_qos_cfg {
	struct mbox_msghdr hdr;
	uint16_t __io grp;
	uint32_t __io rsvd;
	uint16_t __io taq_thr;
	uint16_t __io iaq_thr;
};

struct sso_grp_stash_cfg {
	struct mbox_msghdr hdr;
	uint16_t __io grp;
	uint8_t __io ena;
	uint8_t __io offset : 4;
	uint8_t __io num_linesm1 : 4;
};

struct sso_aggr_setconfig {
	struct mbox_msghdr hdr;
	uint16_t __io npa_pf_func;
	uint16_t __io hwgrp;
	uint64_t __io rsvd[2];
};

struct sso_grp_stats {
	struct mbox_msghdr hdr;
	uint16_t __io grp;
	uint64_t __io ws_pc;
	uint64_t __io ext_pc;
	uint64_t __io wa_pc;
	uint64_t __io ts_pc;
	uint64_t __io ds_pc;
	uint64_t __io dq_pc;
	uint64_t __io aw_status;
	uint64_t __io page_cnt;
};

struct sso_hws_stats {
	struct mbox_msghdr hdr;
	uint16_t __io hws;
	uint64_t __io arbitration;
};

struct sso_aggr_stats {
	struct mbox_msghdr hdr;
	uint16_t __io grp;
	uint64_t __io flushed;
	uint64_t __io completed;
	uint64_t __io npa_fail;
	uint64_t __io timeout;
	uint64_t __io rsvd[4];
};

/* CPT mailbox error codes
 * Range 901 - 1000.
 */
enum cpt_af_status {
	CPT_AF_ERR_PARAM = -901,
	CPT_AF_ERR_GRP_INVALID = -902,
	CPT_AF_ERR_LF_INVALID = -903,
	CPT_AF_ERR_ACCESS_DENIED = -904,
	CPT_AF_ERR_SSO_PF_FUNC_INVALID = -905,
	CPT_AF_ERR_NIX_PF_FUNC_INVALID = -906,
	CPT_AF_ERR_INLINE_IPSEC_INB_ENA = -907,
	CPT_AF_ERR_INLINE_IPSEC_OUT_ENA = -908
};

/* CPT mbox message formats */

struct cpt_rd_wr_reg_msg {
	struct mbox_msghdr hdr;
	uint64_t __io reg_offset;
	uint64_t __io *ret_val;
	uint64_t __io val;
	uint8_t __io is_write;
};

struct cpt_set_crypto_grp_req_msg {
	struct mbox_msghdr hdr;
	uint8_t __io crypto_eng_grp;
};

struct cpt_lf_alloc_req_msg {
	struct mbox_msghdr hdr;
	uint16_t __io nix_pf_func;
	uint16_t __io sso_pf_func;
	uint16_t __io eng_grpmsk;
	uint8_t __io blkaddr;
	uint8_t __io ctx_ilen_valid : 1;
	uint8_t __io ctx_ilen : 7;
	uint8_t __io rxc_ena : 1;
	uint8_t __io rxc_ena_lf_id : 7;
};

struct cpt_rx_inline_qalloc_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io rx_queue_id;
	uint64_t __io rsvd[8]; /* For future extensions */
};

struct cpt_queue_pri_req_msg {
	struct mbox_msghdr hdr;
	uint32_t __io slot;
	uint8_t __io queue_pri;
};

struct cpt_rx_inline_qcfg_req {
	struct mbox_msghdr hdr;
	uint16_t __io sso_pf_func; /* inbound path SSO_PF_FUNC */
	uint16_t __io nix_pf_func; /* outbound path NIX_PF_FUNC */
	uint16_t __io ctx_pf_func;
	uint8_t __io eng_grpmsk;
	uint8_t __io enable;
	uint8_t __io slot;
	uint8_t __io rx_queue_id;
	uint8_t __io ctx_ilen;
	uint8_t __io pf_func_ctx;
	uint8_t __io inflight_limit;
	uint8_t __io queue_pri;
	uint8_t __io rsvd[32]; /* For future extensions */
};

#define CPT_INLINE_INBOUND  0
#define CPT_INLINE_OUTBOUND 1

struct cpt_inline_ipsec_cfg_msg {
	struct mbox_msghdr hdr;
	uint8_t __io enable;
	uint8_t __io slot;
	uint8_t __io dir;
	uint8_t __io sso_pf_func_ovrd;
	uint16_t __io sso_pf_func; /* Inbound path SSO_PF_FUNC */
	uint16_t __io nix_pf_func; /* Outbound path NIX_PF_FUNC */
};

struct cpt_sts_req {
	struct mbox_msghdr hdr;
	uint8_t __io blkaddr;
};

struct cpt_sts_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io inst_req_pc;
	uint64_t __io inst_lat_pc;
	uint64_t __io rd_req_pc;
	uint64_t __io rd_lat_pc;
	uint64_t __io rd_uc_pc;
	uint64_t __io active_cycles_pc;
	uint64_t __io ctx_mis_pc;
	uint64_t __io ctx_hit_pc;
	uint64_t __io ctx_aop_pc;
	uint64_t __io ctx_aop_lat_pc;
	uint64_t __io ctx_ifetch_pc;
	uint64_t __io ctx_ifetch_lat_pc;
	uint64_t __io ctx_ffetch_pc;
	uint64_t __io ctx_ffetch_lat_pc;
	uint64_t __io ctx_wback_pc;
	uint64_t __io ctx_wback_lat_pc;
	uint64_t __io ctx_psh_pc;
	uint64_t __io ctx_psh_lat_pc;
	uint64_t __io ctx_err;
	uint64_t __io ctx_enc_id;
	uint64_t __io ctx_flush_timer;
	uint64_t __io rxc_time;
	uint64_t __io rxc_time_cfg;
	uint64_t __io rxc_active_sts;
	uint64_t __io rxc_zombie_sts;
	uint64_t __io busy_sts_ae;
	uint64_t __io free_sts_ae;
	uint64_t __io busy_sts_se;
	uint64_t __io free_sts_se;
	uint64_t __io busy_sts_ie;
	uint64_t __io free_sts_ie;
	uint64_t __io exe_err_info;
	uint64_t __io cptclk_cnt;
	uint64_t __io diag;
	uint64_t __io rxc_dfrg;
	uint64_t __io x2p_link_cfg0;
	uint64_t __io x2p_link_cfg1;
};

struct cpt_rxc_time_cfg_req {
	struct mbox_msghdr hdr;
	int blkaddr;
	uint32_t __io step;
	uint16_t __io zombie_thres;
	uint16_t __io zombie_limit;
	uint16_t __io active_thres;
	uint16_t __io active_limit;
	uint16_t __io queue_id;
	uint64_t __io cpt_af_rxc_que_cfg;
};

/* Mailbox message format to request for CPT faulted engines */
struct cpt_flt_eng_info_req {
	struct mbox_msghdr hdr;
	int __io blkaddr;
	bool __io reset;
	uint32_t __io rsvd;
};

struct cpt_flt_eng_info_rsp {
	struct mbox_msghdr hdr;
#define CPT_AF_MAX_FLT_INT_VECS 3
	uint64_t __io flt_eng_map[CPT_AF_MAX_FLT_INT_VECS];
	uint64_t __io rcvrd_eng_map[CPT_AF_MAX_FLT_INT_VECS];
	uint64_t __io rsvd;
};

struct cpt_rx_inline_lf_cfg_msg {
	struct mbox_msghdr hdr;
	uint16_t __io sso_pf_func;
	uint16_t __io param1;
	uint16_t __io param2;
	uint16_t __io opcode;
	uint32_t __io credit;
	uint32_t __io credit_th;
	uint16_t __io bpid;
	uint32_t __io reserved;
	uint8_t __io ctx_ilen_valid : 1;
	uint8_t __io ctx_ilen : 7;
};

struct cpt_caps_rsp_msg {
	struct mbox_msghdr hdr;
	uint16_t __io cpt_pf_drv_version;
	uint8_t __io cpt_revision;
	union cpt_eng_caps eng_caps[CPT_MAX_ENG_TYPES];
};

struct cpt_eng_grp_req {
	struct mbox_msghdr hdr;
	uint8_t __io eng_type;
};

struct cpt_eng_grp_rsp {
	struct mbox_msghdr hdr;
	uint8_t __io eng_type;
	uint8_t __io eng_grp_num;
};

struct cpt_lf_rst_req {
	struct mbox_msghdr hdr;
	uint32_t __io slot;
};

/* REE mailbox error codes
 * Range 1001 - 1100.
 */
enum ree_af_status {
	REE_AF_ERR_RULE_UNKNOWN_VALUE = -1001,
	REE_AF_ERR_LF_NO_MORE_RESOURCES = -1002,
	REE_AF_ERR_LF_INVALID = -1003,
	REE_AF_ERR_ACCESS_DENIED = -1004,
	REE_AF_ERR_RULE_DB_PARTIAL = -1005,
	REE_AF_ERR_RULE_DB_EQ_BAD_VALUE = -1006,
	REE_AF_ERR_RULE_DB_BLOCK_ALLOC_FAILED = -1007,
	REE_AF_ERR_BLOCK_NOT_IMPLEMENTED = -1008,
	REE_AF_ERR_RULE_DB_INC_OFFSET_TOO_BIG = -1009,
	REE_AF_ERR_RULE_DB_OFFSET_TOO_BIG = -1010,
	REE_AF_ERR_Q_IS_GRACEFUL_DIS = -1011,
	REE_AF_ERR_Q_NOT_GRACEFUL_DIS = -1012,
	REE_AF_ERR_RULE_DB_ALLOC_FAILED = -1013,
	REE_AF_ERR_RULE_DB_TOO_BIG = -1014,
	REE_AF_ERR_RULE_DB_GEQ_BAD_VALUE = -1015,
	REE_AF_ERR_RULE_DB_LEQ_BAD_VALUE = -1016,
	REE_AF_ERR_RULE_DB_WRONG_LENGTH = -1017,
	REE_AF_ERR_RULE_DB_WRONG_OFFSET = -1018,
	REE_AF_ERR_RULE_DB_BLOCK_TOO_BIG = -1019,
	REE_AF_ERR_RULE_DB_SHOULD_FILL_REQUEST = -1020,
	REE_AF_ERR_RULE_DBI_ALLOC_FAILED = -1021,
	REE_AF_ERR_LF_WRONG_PRIORITY = -1022,
	REE_AF_ERR_LF_SIZE_TOO_BIG = -1023,
};

/* REE mbox message formats */

struct ree_req_msg {
	struct mbox_msghdr hdr;
	uint32_t __io blkaddr;
};

struct ree_lf_req_msg {
	struct mbox_msghdr hdr;
	uint32_t __io blkaddr;
	uint32_t __io size;
	uint8_t __io lf;
	uint8_t __io pri;
};

struct ree_rule_db_prog_req_msg {
	struct mbox_msghdr hdr;
#define REE_RULE_DB_REQ_BLOCK_SIZE ((64ULL * 1024ULL) >> 1)
	uint8_t __io rule_db[REE_RULE_DB_REQ_BLOCK_SIZE];
	uint32_t __io blkaddr;	     /* REE0 or REE1 */
	uint32_t __io total_len;     /* total len of rule db */
	uint32_t __io offset;	     /* offset of current rule db block */
	uint16_t __io len;	     /* length of rule db block */
	uint8_t __io is_last;	     /* is this the last block */
	uint8_t __io is_incremental; /* is incremental flow */
	uint8_t __io is_dbi;	     /* is rule db incremental */
};

struct ree_rule_db_get_req_msg {
	struct mbox_msghdr hdr;
	uint32_t __io blkaddr;
	uint32_t __io offset; /* retrieve db from this offset */
	uint8_t __io is_dbi;  /* is request for rule db incremental */
};

struct ree_rd_wr_reg_msg {
	struct mbox_msghdr hdr;
	uint64_t __io reg_offset;
	uint64_t __io *ret_val;
	uint64_t __io val;
	uint32_t __io blkaddr;
	uint8_t __io is_write;
};

struct ree_rule_db_len_rsp_msg {
	struct mbox_msghdr hdr;
	uint32_t __io blkaddr;
	uint32_t __io len;
	uint32_t __io inc_len;
};

struct ree_rule_db_get_rsp_msg {
	struct mbox_msghdr hdr;
#define REE_RULE_DB_RSP_BLOCK_SIZE (15ULL * 1024ULL)
	uint8_t __io rule_db[REE_RULE_DB_RSP_BLOCK_SIZE];
	uint32_t __io total_len; /* total len of rule db */
	uint32_t __io offset;	 /* offset of current rule db block */
	uint16_t __io len;	 /* length of rule db block */
	uint8_t __io is_last;	 /* is this the last block */
};

/* NPC mbox message structs */

#define NPC_MCAM_ENTRY_INVALID 0xFFFF
#define NPC_MCAM_INVALID_MAP   0xFFFF

/* NPC mailbox error codes
 * Range 701 - 800.
 */
enum npc_af_status {
	NPC_MCAM_INVALID_REQ = -701,
	NPC_MCAM_ALLOC_DENIED = -702,
	NPC_MCAM_ALLOC_FAILED = -703,
	NPC_MCAM_PERM_DENIED = -704,
	NPC_AF_ERR_HIGIG_CONFIG_FAIL = -705,
	NPC_AF_ERR_HIGIG_NOT_SUPPORTED = -706,
	NPC_FLOW_INTF_INVALID = -707,
	NPC_FLOW_CHAN_INVALID = -708,
	NPC_FLOW_NO_NIXLF = -709,
	NPC_FLOW_NOT_SUPPORTED = -710,
	NPC_FLOW_VF_PERM_DENIED = -711,
	NPC_FLOW_VF_NOT_INIT = -712,
	NPC_FLOW_VF_OVERLAP = -713,
};

struct npc_mcam_alloc_entry_req {
	struct mbox_msghdr hdr;
#define NPC_MAX_NONCONTIG_ENTRIES 256
	uint8_t __io contig; /* Contiguous entries ? */
#define NPC_MCAM_ANY_PRIO    0
#define NPC_MCAM_LOWER_PRIO  1
#define NPC_MCAM_HIGHER_PRIO 2
	uint8_t __io ref_priority; /* Lower or higher w.r.t ref_entry */
	uint16_t __io ref_entry;
	uint16_t __io count; /* Number of entries requested */
	uint8_t __io kw_type; /* Key type */
	uint8_t __io virt;    /* Request virtual index */
	uint16_t __io rsvd[16];	/* Reserved */
};

struct npc_mcam_alloc_entry_rsp {
	struct mbox_msghdr hdr;
	/* Entry alloc'ed or start index if contiguous.
	 * Invalid in case of non-contiguous.
	 */
	uint16_t __io entry;
	uint16_t __io count;	  /* Number of entries allocated */
	uint16_t __io free_count; /* Number of entries available */
	uint16_t __io entry_list[NPC_MAX_NONCONTIG_ENTRIES];
	uint16_t __io rsvd[16];	/* Reserved */
};

struct npc_mcam_free_entry_req {
	struct mbox_msghdr hdr;
	uint16_t __io entry; /* Entry index to be freed */
	uint8_t __io all;    /* Free all entries alloc'ed to this PFVF */
};

struct mcam_entry {
#define NPC_MAX_KWS_IN_KEY 7 /* Number of keywords in max key width */
	uint64_t __io kw[NPC_MAX_KWS_IN_KEY];
	uint64_t __io kw_mask[NPC_MAX_KWS_IN_KEY];
	uint64_t __io action;
	uint64_t __io vtag_action;
};

struct npc_mcam_write_entry_req {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
	uint16_t __io entry;	   /* MCAM entry to write this match key */
	uint16_t __io cntr;	   /* Counter for this MCAM entry */
	uint8_t __io intf;	   /* Rx or Tx interface */
	uint8_t __io enable_entry; /* Enable this MCAM entry ? */
	uint8_t __io set_cntr;	   /* Set counter for this entry ? */
};

struct cn20k_mcam_entry {
#define NPC_CN20K_MAX_KWS_IN_KEY 8 /* Number of keywords in max keywidth */
	uint64_t __io kw[NPC_CN20K_MAX_KWS_IN_KEY];
	uint64_t __io kw_mask[NPC_CN20K_MAX_KWS_IN_KEY];
	uint64_t __io action;
	uint64_t __io vtag_action;
	uint64_t __io action2;
};

struct npc_cn20k_mcam_write_entry_req {
	struct mbox_msghdr hdr;
	struct cn20k_mcam_entry entry_data;
	uint16_t __io entry;	   /* MCAM entry to write this match key */
	uint16_t __io cntr;	   /* Counter for this MCAM entry */
	uint8_t __io intf;	   /* Rx or Tx interface */
	uint8_t __io enable_entry; /* Enable this MCAM entry ? */
	uint8_t __io hw_prio;	   /* hardware priority, valid for cn20k */
	uint8_t __io req_kw_type;  /* Type of kw which should be written */
	uint64_t __io reserved;	   /* reserved for future use */
};

/* Enable/Disable a given entry */
struct npc_mcam_ena_dis_entry_req {
	struct mbox_msghdr hdr;
	uint16_t __io entry;
};

struct npc_mcam_shift_entry_req {
	struct mbox_msghdr hdr;
#define NPC_MCAM_MAX_SHIFTS 64
	uint16_t __io curr_entry[NPC_MCAM_MAX_SHIFTS];
	uint16_t __io new_entry[NPC_MCAM_MAX_SHIFTS];
	uint16_t __io shift_count; /* Number of entries to shift */
};

struct npc_mcam_shift_entry_rsp {
	struct mbox_msghdr hdr;
	/* Index in 'curr_entry', not entry itself */
	uint16_t __io failed_entry_idx;
};

struct npc_mcam_alloc_counter_req {
	struct mbox_msghdr hdr;
	uint8_t __io contig; /* Contiguous counters ? */
#define NPC_MAX_NONCONTIG_COUNTERS 64
	uint16_t __io count; /* Number of counters requested */
};

struct npc_mcam_alloc_counter_rsp {
	struct mbox_msghdr hdr;
	/* Counter alloc'ed or start idx if contiguous.
	 * Invalid in case of non-contiguous.
	 */
	uint16_t __io cntr;
	uint16_t __io count; /* Number of counters allocated */
	uint16_t __io cntr_list[NPC_MAX_NONCONTIG_COUNTERS];
};

struct npc_mcam_oper_counter_req {
	struct mbox_msghdr hdr;
	uint16_t __io cntr; /* Free a counter or clear/fetch it's stats */
};

struct npc_mcam_oper_counter_rsp {
	struct mbox_msghdr hdr;
	/* valid only while fetching counter's stats */
	uint64_t __io stat;
};

struct npc_mcam_unmap_counter_req {
	struct mbox_msghdr hdr;
	uint16_t __io cntr;
	uint16_t __io entry; /* Entry and counter to be unmapped */
	uint8_t __io all;    /* Unmap all entries using this counter ? */
};

struct npc_mcam_alloc_and_write_entry_req {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
	uint16_t __io ref_entry;
	uint8_t __io ref_priority; /* Lower or higher w.r.t ref_entry */
	uint8_t __io intf;	   /* Rx or Tx interface */
	uint8_t __io enable_entry; /* Enable this MCAM entry ? */
	uint8_t __io alloc_cntr;   /* Allocate counter and map ? */
};

struct npc_cn20k_mcam_alloc_and_write_entry_req {
	struct mbox_msghdr hdr;
	struct cn20k_mcam_entry entry_data;
	uint16_t __io ref_entry;
	uint8_t __io ref_prio;	   /* Lower or higher w.r.t ref_entry */
	uint8_t __io intf;	   /* Rx or Tx interface */
	uint8_t __io enable_entry; /* Enable this MCAM entry ? */
	uint8_t __io hw_prio;	   /* hardware priority, valid for cn20k */
	uint8_t __io virt;	   /* Allocate virtual index */
	uint8_t __io req_kw_type;  /* Key type to be written */
	uint16_t __io reserved[4]; /* reserved for future use */
};

struct npc_mcam_alloc_and_write_entry_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io entry;
	uint16_t __io cntr;
};

struct npc_get_kex_cfg_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io rx_keyx_cfg; /* NPC_AF_INTF(0)_KEX_CFG */
	uint64_t __io tx_keyx_cfg; /* NPC_AF_INTF(1)_KEX_CFG */
#define NPC_MAX_INTF 2
#define NPC_MAX_LID  8
#define NPC_MAX_LT   16
#define NPC_MAX_LD   2
#define NPC_MAX_LFL  16
	/* NPC_AF_KEX_LDATA(0..1)_FLAGS_CFG */
	uint64_t __io kex_ld_flags[NPC_MAX_LD];
	/* NPC_AF_INTF(0..1)_LID(0..7)_LT(0..15)_LD(0..1)_CFG */
	uint64_t __io intf_lid_lt_ld[NPC_MAX_INTF][NPC_MAX_LID][NPC_MAX_LT]
				    [NPC_MAX_LD];
	/* NPC_AF_INTF(0..1)_LDATA(0..1)_FLAGS(0..15)_CFG */
	uint64_t __io intf_ld_flags[NPC_MAX_INTF][NPC_MAX_LD][NPC_MAX_LFL];
#define MKEX_NAME_LEN 128
	uint8_t __io mkex_pfl_name[MKEX_NAME_LEN];
};

struct npc_delete_flow_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io cntr_val;
};

/* Available entries to use */
struct npc_cn20k_get_free_count_rsp {
	struct mbox_msghdr hdr;
	int __io free_x2;
	int __io free_x4;
	int __io free_subbanks;
};

struct npc_cn20k_get_kex_cfg_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io rx_keyx_cfg; /* NPC_AF_INTF(0)_KEX_CFG */
	uint64_t __io tx_keyx_cfg; /* NPC_AF_INTF(1)_KEX_CFG */
#define NPC_MAX_EXTRACTOR 24
	/* MKEX Extractor data */
	uint64_t __io intf_extr[NPC_MAX_INTF][NPC_MAX_EXTRACTOR];
	/* KEX configuration per extractor */
	uint64_t __io intf_extr_lt[NPC_MAX_INTF][NPC_MAX_EXTRACTOR][NPC_MAX_LT];
#define MKEX_NAME_LEN 128
	uint8_t __io mkex_pfl_name[MKEX_NAME_LEN];
};

struct npc_get_field_hash_info_req {
	struct mbox_msghdr hdr;
	uint8_t __io intf;
};

struct npc_get_field_hash_info_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io secret_key[3];
#define NPC_MAX_HASH	  2
#define NPC_MAX_HASH_MASK 2
	/* NPC_AF_INTF(0..1)_HASH(0..1)_MASK(0..1) */
	uint64_t __io hash_mask[NPC_MAX_INTF][NPC_MAX_HASH][NPC_MAX_HASH_MASK];
	/* NPC_AF_INTF(0..1)_HASH(0..1)_RESULT_CTRL */
	uint64_t __io hash_ctrl[NPC_MAX_INTF][NPC_MAX_HASH];
};

enum header_fields {
	NPC_DMAC,
	NPC_SMAC,
	NPC_ETYPE,
	NPC_VLAN_ETYPE_CTAG, /* 0x8100 */
	NPC_VLAN_ETYPE_STAG, /* 0x88A8 */
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
	unsigned char __io dmac[6];
	unsigned char __io smac[6];
	uint16_t __io etype;
	uint16_t __io vlan_etype;
	uint16_t __io vlan_tci;
	union {
		uint32_t __io ip4src;
		uint32_t __io ip6src[4];
	};
	union {
		uint32_t __io ip4dst;
		uint32_t __io ip6dst[4];
	};
	union {
		uint32_t spi;
	};
	uint8_t __io tos;
	uint8_t __io ip_ver;
	uint8_t __io ip_proto;
	uint8_t __io tc;
	uint16_t __io sport;
	uint16_t __io dport;
	union {
		uint8_t __io ip_flag;
		uint8_t __io next_header;
	};
	uint16_t __io vlan_itci;
	uint8_t __io icmp_type;
	uint8_t __io icmp_code;
	uint16_t __io tcp_flags;
	uint32_t __io gtpu_teid;
	uint32_t __io gtpc_teid;
	uint32_t __io mpls_lse[4];
	uint16_t __io sq_id;
};

struct npc_install_flow_req {
	struct mbox_msghdr hdr;
	struct flow_msg packet;
	struct flow_msg mask;
	uint64_t __io features;
	uint16_t __io entry;
	uint16_t __io channel;
	uint16_t __io chan_mask;
	uint8_t __io intf;
	uint8_t __io set_cntr;
	uint8_t __io default_rule;
	/* Overwrite(0) or append(1) flow to default rule? */
	uint8_t __io append;
	uint16_t __io vf;
	/* action */
	uint32_t __io index;
	uint16_t __io match_id;
	uint8_t __io flow_key_alg;
	uint8_t __io op;
	/* vtag action */
	uint8_t __io vtag0_type;
	uint8_t __io vtag0_valid;
	uint8_t __io vtag1_type;
	uint8_t __io vtag1_valid;

	/* vtag tx action */
	uint16_t __io vtag0_def;
	uint8_t __io vtag0_op;
	uint16_t __io vtag1_def;
	uint8_t __io vtag1_op;
	/* old counter value */
	uint16_t __io cntr_val;
	/* hardware priority, supported for cn20k */
	uint8_t __io hw_prio;
};

struct npc_install_flow_rsp {
	struct mbox_msghdr hdr;
	/* Negative if no counter else counter number */
	int __io counter;
};

struct npc_delete_flow_req {
	struct mbox_msghdr hdr;
	uint16_t __io entry;
	uint16_t __io start; /*Disable range of entries */
	uint16_t __io end;
	uint8_t __io all; /* PF + VFs */
	uint16_t __io vf; /* Requesting VF */
};

struct npc_mcam_read_entry_req {
	struct mbox_msghdr hdr;
	/* MCAM entry to read */
	uint16_t __io entry;
};

struct npc_mcam_read_entry_rsp {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
	uint8_t __io intf;
	uint8_t __io enable;
};

struct npc_cn20k_mcam_read_entry_rsp {
	struct mbox_msghdr hdr;
	struct cn20k_mcam_entry entry_data;
	uint8_t __io intf;
	uint8_t __io enable;
	uint8_t __io hw_prio; /* valid for cn20k */
};

struct npc_mcam_read_base_rule_rsp {
	struct mbox_msghdr hdr;
	struct mcam_entry entry_data;
};

struct npc_cn20k_mcam_read_base_rule_rsp {
	struct mbox_msghdr hdr;
	struct cn20k_mcam_entry entry;
};

struct npc_mcam_get_stats_req {
	struct mbox_msghdr hdr;
	uint16_t __io entry; /* mcam entry */
};

struct npc_mcam_get_stats_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io stat;  /* counter stats */
	uint8_t __io stat_ena; /* enabled */
};

#define MCAM_ARR_SIZE    256
#define MCAM_ARR_ELEM_SZ 64

struct npc_mcam_get_hit_status_req {
	struct mbox_msghdr hdr;
	/* If clear == true, then if the hit status bit for mcam id is set,
	 * then needs to cleared by writing 1 back.
	 * If clear == false, then leave the hit status bit as is.
	 */
	bool __io clear;
	uint8_t __io reserved[3];
	/* Start range of mcam id */
	uint32_t __io range_valid_mcam_ids_start;
	/* End range of mcam id */
	uint32_t __io range_valid_mcam_ids_end;
	/* Bitmap of mcam ids for which the hit status needs to checked */
	uint64_t __io mcam_ids[MCAM_ARR_SIZE];
};

struct npc_mcam_get_hit_status_rsp {
	struct mbox_msghdr hdr;
	/* Bitmap of mcam hit status, prior to clearing */
	uint64_t __io mcam_hit_status[MCAM_ARR_SIZE];
};

/* TIM mailbox error codes
 * Range 801 - 900.
 */
enum tim_af_status {
	TIM_AF_NO_RINGS_LEFT = -801,
	TIM_AF_INVALID_NPA_PF_FUNC = -802,
	TIM_AF_INVALID_SSO_PF_FUNC = -803,
	TIM_AF_RING_STILL_RUNNING = -804,
	TIM_AF_LF_INVALID = -805,
	TIM_AF_CSIZE_NOT_ALIGNED = -806,
	TIM_AF_CSIZE_TOO_SMALL = -807,
	TIM_AF_CSIZE_TOO_BIG = -808,
	TIM_AF_INTERVAL_TOO_SMALL = -809,
	TIM_AF_INVALID_BIG_ENDIAN_VALUE = -810,
	TIM_AF_INVALID_CLOCK_SOURCE = -811,
	TIM_AF_GPIO_CLK_SRC_NOT_ENABLED = -812,
	TIM_AF_INVALID_BSIZE = -813,
	TIM_AF_INVALID_ENABLE_PERIODIC = -814,
	TIM_AF_INVALID_ENABLE_DONTFREE = -815,
	TIM_AF_ENA_DONTFRE_NSET_PERIODIC = -816,
	TIM_AF_RING_ALREADY_DISABLED = -817,
	TIM_AF_LF_START_SYNC_FAIL = -818,
};

enum tim_clk_srcs {
	TIM_CLK_SRCS_TENNS = 0,
	TIM_CLK_SRCS_GPIO = 1,
	TIM_CLK_SRCS_GTI = 2,
	TIM_CLK_SRCS_PTP = 3,
	TIM_CLK_SRCS_SYNCE = 4,
	TIM_CLK_SRCS_BTS = 5,
	TIM_CLK_SRCS_EXT_MIO = 6,
	TIM_CLK_SRCS_EXT_GTI = 7,
	TIM_CLK_SRSC_INVALID,
};

enum tim_gpio_edge {
	TIM_GPIO_NO_EDGE = 0,
	TIM_GPIO_LTOH_TRANS = 1,
	TIM_GPIO_HTOL_TRANS = 2,
	TIM_GPIO_BOTH_TRANS = 3,
	TIM_GPIO_INVALID,
};

enum ptp_op {
	PTP_OP_ADJFINE = 0,   /* adjfine(req.scaled_ppm); */
	PTP_OP_GET_CLOCK = 1, /* rsp.clk = get_clock() */
};

struct ptp_req {
	struct mbox_msghdr hdr;
	uint8_t __io op;
	int64_t __io scaled_ppm;
	uint8_t __io is_pmu;
};

struct ptp_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io clk;
	uint64_t __io tsc;
};

struct get_hw_cap_rsp {
	struct mbox_msghdr hdr;
	/* Schq mapping fixed or flexible */
	uint8_t __io nix_fixed_txschq_mapping;
	uint8_t __io nix_shaping;      /* Is shaping and coloring supported */
	uint8_t __io npc_hash_extract; /* Is hash extract supported */
#define HW_CAP_MACSEC BIT_ULL(1)
	uint64_t hw_caps;
};

struct ndc_sync_op {
	struct mbox_msghdr hdr;
	uint8_t __io nix_lf_tx_sync;
	uint8_t __io nix_lf_rx_sync;
	uint8_t __io npa_lf_sync;
};

struct tim_lf_alloc_req {
	struct mbox_msghdr hdr;
	uint16_t __io ring;
	uint16_t __io npa_pf_func;
	uint16_t __io sso_pf_func;
};

struct tim_ring_req {
	struct mbox_msghdr hdr;
	uint16_t __io ring;
};

struct tim_config_req {
	struct mbox_msghdr hdr;
	uint16_t __io ring;
	uint8_t __io bigendian;
	uint8_t __io clocksource;
	uint8_t __io enableperiodic;
	uint8_t __io enabledontfreebuffer;
	uint32_t __io bucketsize;
	uint32_t __io chunksize;
	uint32_t __io interval_lo;
	uint8_t __io gpioedge;
	uint8_t __io rsvd[3];
	uint32_t __io interval_hi;
	uint64_t __io intervalns;
	uint64_t __io clockfreq;
};

struct tim_cfg_hwwqe_req {
	struct mbox_msghdr hdr;
	uint16_t __io ring;
	uint8_t __io grp_ena;
	uint8_t __io hwwqe_ena;
	uint8_t __io ins_min_gap;
	uint8_t __io flw_ctrl_ena;
	uint8_t __io wqe_rd_clr_ena;
	uint16_t __io grp_tmo_cntr;
	uint16_t __io npa_tmo_cntr;
	uint16_t __io result_offset;
	uint16_t __io event_count_offset;
	uint64_t __io rsvd[2];
};

struct tim_feat_info {
	uint16_t __io rings;
	uint8_t __io engines;
	uint8_t __io hwwqe : 1;
	uint8_t __io intvl_ext : 1;
	uint8_t __io rsvd8[4];
	uint64_t __io rsvd[2];
};

struct tim_hw_info {
	struct mbox_msghdr hdr;
	struct tim_feat_info feat;
};

struct tim_lf_alloc_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io tenns_clk;
};

struct tim_enable_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io timestarted;
	uint32_t __io currentbucket;
};

struct tim_intvl_req {
	struct mbox_msghdr hdr;
	uint8_t __io clocksource;
	uint64_t __io clockfreq;
};

struct tim_intvl_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io intvl_cyc;
	uint64_t __io intvl_ns;
};

struct tim_capture_rsp {
	struct mbox_msghdr hdr;
	uint64_t __io counters[TIM_CLK_SRSC_INVALID];
};

struct sdp_node_info {
	/* Node to which this PF belons to */
	uint8_t __io node_id;
	uint8_t __io max_vfs;
	uint8_t __io num_pf_rings;
	uint8_t __io pf_srn;
#define SDP_MAX_VFS	128
	uint8_t __io vf_rings[SDP_MAX_VFS];
};

struct sdp_chan_info_msg {
	struct mbox_msghdr hdr;
	struct sdp_node_info info;
};

/* For SPI to SA index add */
struct nix_spi_to_sa_add_req {
	struct mbox_msghdr hdr;
	uint32_t __io sa_index;
	uint32_t __io spi_index;
	uint16_t __io match_id;
	bool __io valid;
	uint8_t __io inline_profile_id;
};

struct nix_spi_to_sa_add_rsp {
	struct mbox_msghdr hdr;
	uint16_t __io hash_index;
	uint8_t __io way;
	uint8_t __io is_duplicate;
};

/* To free SPI to SA index */
struct nix_spi_to_sa_delete_req {
	struct mbox_msghdr hdr;
	uint16_t __io hash_index;
	uint8_t __io way;
};

struct rep_evt_data {
	uint8_t __io port_state;
	uint8_t __io vf_state;
	uint16_t __io rx_mode;
	uint16_t __io rx_flags;
	uint16_t __io mtu;
	uint64_t __io rsvd[5];
};

struct rep_event {
	struct mbox_msghdr hdr;
	uint16_t __io pcifunc;
#define RVU_EVENT_PORT_STATE	 BIT_ULL(0)
#define RVU_EVENT_PFVF_STATE	 BIT_ULL(1)
#define RVU_EVENT_MTU_CHANGE	 BIT_ULL(2)
#define RVU_EVENT_RX_MODE_CHANGE BIT_ULL(3)
	uint16_t __io event;
	struct rep_evt_data evt_data;
};

#endif /* __ROC_MBOX_H__ */
