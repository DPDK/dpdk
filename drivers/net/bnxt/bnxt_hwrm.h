/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2021 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_HWRM_H_
#define _BNXT_HWRM_H_

#include <inttypes.h>
#include <stdbool.h>

struct bnxt;
struct bnxt_filter_info;
struct bnxt_cp_ring_info;
struct hwrm_func_qstats_output;

/* Convert Bit field location to value */
#define ASYNC_CMPL_EVENT_ID_LINK_STATUS_CHANGE	\
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE)
#define ASYNC_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED	\
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED)
#define ASYNC_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE	\
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE)
#define ASYNC_CMPL_EVENT_ID_LINK_SPEED_CHANGE \
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE)
#define ASYNC_CMPL_EVENT_ID_RESET_NOTIFY \
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_RESET_NOTIFY)
#define ASYNC_CMPL_EVENT_ID_ERROR_RECOVERY \
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_RECOVERY)
#define ASYNC_CMPL_EVENT_ID_PF_DRVR_UNLOAD	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD - 32))
#define ASYNC_CMPL_EVENT_ID_VF_CFG_CHANGE	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE - 32))
#define ASYNC_CMPL_EVENT_ID_DBG_NOTIFICATION	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DEBUG_NOTIFICATION - 32))
#define	ASYNC_CMPL_EVENT_ID_DEFAULT_VNIC_CHANGE	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DEFAULT_VNIC_CHANGE - 32))
#define	ASYNC_CMPL_EVENT_ID_ECHO_REQUEST	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ECHO_REQUEST - 64))
#define	ASYNC_CMPL_EVENT_ID_ERROR_REPORT	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_ERROR_REPORT - 64))

#define HWRM_QUEUE_SERVICE_PROFILE_LOSSY \
	HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSY

#define HWRM_QUEUE_SERVICE_PROFILE_UNKNOWN \
	HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_UNKNOWN

#define HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESV_STRATEGY_MINIMAL_STATIC \
	HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESERVATION_STRATEGY_MINIMAL_STATIC
#define HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESV_STRATEGY_MAXIMAL \
	HWRM_FUNC_RESOURCE_QCAPS_OUTPUT_VF_RESERVATION_STRATEGY_MAXIMAL

#define HWRM_PORT_PHY_CFG_IN_EN_FORCE_PAM4_LINK_SPEED \
	HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAM4_LINK_SPEED
#define HWRM_PORT_PHY_CFG_IN_EN_AUTO_PAM4_LINK_SPD_MASK \
	HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAM4_LINK_SPEED_MASK
#define HWRM_PORT_PHY_CFG_IN_EN_AUTO_LINK_SPEED_MASK \
	HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED_MASK

#define HWRM_SPEC_CODE_1_8_4		0x10804
#define HWRM_SPEC_CODE_1_9_0		0x10900
#define HWRM_SPEC_CODE_1_9_2		0x10902

#define FUNC_BACKING_STORE_CFG_INPUT_DFLT_ENABLES              \
	(HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_QP |        \
	HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_SRQ |        \
	HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_CQ |         \
	HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_VNIC |       \
	HWRM_FUNC_BACKING_STORE_CFG_INPUT_ENABLES_STAT)

#define GET_TX_QUEUE_INFO(x) \
	bp->tx_cos_queue[x].id = resp->queue_id##x; \
	bp->tx_cos_queue[x].profile =	\
		resp->queue_id##x##_service_profile

#define GET_RX_QUEUE_INFO(x) \
	bp->rx_cos_queue[x].id = resp->queue_id##x; \
	bp->rx_cos_queue[x].profile =	\
		resp->queue_id##x##_service_profile

int bnxt_hwrm_tf_message_tunneled(struct bnxt *bp,
				  bool use_kong_mb,
				  uint16_t tf_type,
				  uint16_t tf_subtype,
				  uint32_t *tf_response_code,
				  void *msg,
				  uint32_t msg_len,
				  void *response,
				  uint32_t response_len);

int bnxt_hwrm_tf_message_direct(struct bnxt *bp,
				bool use_kong_mb,
				uint16_t msg_type,
				void *msg,
				uint32_t msg_len,
				void *resp_msg,
				uint32_t resp_len);

#define CFA_COUNTER_CFG_IN_COUNTER_TYPE_FC \
	HWRM_CFA_COUNTER_CFG_INPUT_COUNTER_TYPE_FC

enum bnxt_flow_dir {
	BNXT_DIR_RX = 0,
	BNXT_DIR_TX,
	BNXT_DIR_LOOPBACK,
	BNXT_DIR_MAX
};

struct bnxt_pf_resource_info {
	uint16_t num_rsscos_ctxs;
	uint16_t num_stat_ctxs;
	uint16_t num_tx_rings;
	uint16_t num_rx_rings;
	uint16_t num_cp_rings;
	uint16_t num_l2_ctxs;
	uint16_t num_nq_rings;
	uint16_t num_vnics;
	uint32_t num_hw_ring_grps;
};

#define BNXT_CTX_VAL_INVAL	0xFFFF

#define BNXT_TUNNELED_OFFLOADS_CAP_VXLAN_EN(bp)		\
	(!((bp)->tunnel_disable_flag & HWRM_FUNC_QCAPS_OUTPUT_TUNNEL_DISABLE_FLAG_DISABLE_VXLAN))
#define BNXT_TUNNELED_OFFLOADS_CAP_NGE_EN(bp)		\
	(!((bp)->tunnel_disable_flag & HWRM_FUNC_QCAPS_OUTPUT_TUNNEL_DISABLE_FLAG_DISABLE_NGE))
#define BNXT_TUNNELED_OFFLOADS_CAP_GRE_EN(bp)		\
	(!((bp)->tunnel_disable_flag & HWRM_FUNC_QCAPS_OUTPUT_TUNNEL_DISABLE_FLAG_DISABLE_GRE))
#define BNXT_TUNNELED_OFFLOADS_CAP_IPINIP_EN(bp)	\
	(!((bp)->tunnel_disable_flag & HWRM_FUNC_QCAPS_OUTPUT_TUNNEL_DISABLE_FLAG_DISABLE_IPINIP))

/*
 * If the device supports VXLAN, GRE, IPIP and GENEVE tunnel parsing, then report
 * RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM, RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM and
 * RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM in the Rx/Tx offload capabilities of the device.
 */
#define BNXT_TUNNELED_OFFLOADS_CAP_ALL_EN(bp)			\
	(BNXT_TUNNELED_OFFLOADS_CAP_VXLAN_EN(bp) &&		\
	 BNXT_TUNNELED_OFFLOADS_CAP_NGE_EN(bp)   &&		\
	 BNXT_TUNNELED_OFFLOADS_CAP_GRE_EN(bp)   &&		\
	 BNXT_TUNNELED_OFFLOADS_CAP_IPINIP_EN(bp))

#define BNXT_SIG_MODE_NRZ	HWRM_PORT_PHY_QCFG_OUTPUT_SIGNAL_MODE_NRZ
#define BNXT_SIG_MODE_PAM4	HWRM_PORT_PHY_QCFG_OUTPUT_SIGNAL_MODE_PAM4

int bnxt_hwrm_cfa_l2_clear_rx_mask(struct bnxt *bp,
				   struct bnxt_vnic_info *vnic);
int bnxt_hwrm_cfa_l2_set_rx_mask(struct bnxt *bp, struct bnxt_vnic_info *vnic,
				 uint16_t vlan_count,
				 struct bnxt_vlan_table_entry *vlan_table);
int bnxt_hwrm_cfa_vlan_antispoof_cfg(struct bnxt *bp, uint16_t fid,
			uint16_t vlan_count,
			struct bnxt_vlan_antispoof_table_entry *vlan_table);
int bnxt_hwrm_clear_l2_filter(struct bnxt *bp,
			   struct bnxt_filter_info *filter);
int bnxt_hwrm_set_l2_filter(struct bnxt *bp,
			 uint16_t dst_id,
			 struct bnxt_filter_info *filter);
int bnxt_hwrm_exec_fwd_resp(struct bnxt *bp, uint16_t target_id,
			    void *encaped, size_t ec_size);
int bnxt_hwrm_reject_fwd_resp(struct bnxt *bp, uint16_t target_id,
			      void *encaped, size_t ec_size);

int bnxt_hwrm_func_buf_rgtr(struct bnxt *bp, int num_vfs);
int bnxt_hwrm_func_buf_unrgtr(struct bnxt *bp);
int bnxt_hwrm_func_driver_register(struct bnxt *bp);
int bnxt_hwrm_func_qcaps(struct bnxt *bp);
int bnxt_hwrm_func_reset(struct bnxt *bp);
int bnxt_hwrm_func_driver_unregister(struct bnxt *bp);
int bnxt_hwrm_func_qstats(struct bnxt *bp, uint16_t fid,
			  struct rte_eth_stats *stats,
			  struct hwrm_func_qstats_output *func_qstats);
int bnxt_hwrm_func_qstats_tx_drop(struct bnxt *bp, uint16_t fid,
				  uint64_t *dropped);
int bnxt_hwrm_func_clr_stats(struct bnxt *bp, uint16_t fid);
int bnxt_hwrm_func_cfg_def_cp(struct bnxt *bp);
int bnxt_hwrm_vf_func_cfg_def_cp(struct bnxt *bp);

int bnxt_hwrm_queue_qportcfg(struct bnxt *bp);

int bnxt_hwrm_set_async_event_cr(struct bnxt *bp);
int bnxt_hwrm_ring_alloc(struct bnxt *bp,
			 struct bnxt_ring *ring,
			 uint32_t ring_type, uint32_t map_index,
			 uint32_t stats_ctx_id, uint32_t cmpl_ring_id,
			 uint16_t tx_cosq_id);
int bnxt_hwrm_ring_free(struct bnxt *bp,
			struct bnxt_ring *ring, uint32_t ring_type,
			uint16_t cp_ring_id);
int bnxt_hwrm_ring_grp_alloc(struct bnxt *bp, unsigned int idx);
int bnxt_hwrm_ring_grp_free(struct bnxt *bp, unsigned int idx);

int bnxt_hwrm_stat_clear(struct bnxt *bp, struct bnxt_cp_ring_info *cpr);
int bnxt_hwrm_ver_get(struct bnxt *bp, uint32_t timeout);

int bnxt_hwrm_vnic_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_cfg(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_qcfg(struct bnxt *bp, struct bnxt_vnic_info *vnic,
				int16_t fw_vf_id);
int bnxt_hwrm_vnic_qcaps(struct bnxt *bp);
int bnxt_hwrm_vnic_ctx_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic,
			     uint16_t ctx_idx);
int bnxt_hwrm_vnic_ctx_free(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_free(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_rss_cfg(struct bnxt *bp,
			   struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_plcmode_cfg(struct bnxt *bp,
				struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_tpa_cfg(struct bnxt *bp,
			   struct bnxt_vnic_info *vnic, bool enable);

int bnxt_clear_all_hwrm_stat_ctxs(struct bnxt *bp);
int bnxt_alloc_all_hwrm_ring_grps(struct bnxt *bp);
void bnxt_free_cp_ring(struct bnxt *bp, struct bnxt_cp_ring_info *cpr);
void bnxt_free_nq_ring(struct bnxt *bp, struct bnxt_cp_ring_info *cpr);
int bnxt_set_hwrm_vnic_filters(struct bnxt *bp, struct bnxt_vnic_info *vnic);
void bnxt_free_all_hwrm_resources(struct bnxt *bp);
void bnxt_free_hwrm_resources(struct bnxt *bp);
void bnxt_free_hwrm_rx_ring(struct bnxt *bp, int queue_index);
int bnxt_alloc_hwrm_resources(struct bnxt *bp);
int bnxt_get_hwrm_link_config(struct bnxt *bp, struct rte_eth_link *link);
int bnxt_set_hwrm_link_config(struct bnxt *bp, bool link_up);
int bnxt_hwrm_func_qcfg(struct bnxt *bp, uint16_t *mtu);
int bnxt_hwrm_func_resc_qcaps(struct bnxt *bp);
int bnxt_hwrm_func_reserve_vf_resc(struct bnxt *bp, bool test);
int bnxt_hwrm_allocate_pf_only(struct bnxt *bp);
int bnxt_hwrm_allocate_vfs(struct bnxt *bp, int num_vfs);
int bnxt_hwrm_func_vf_mac(struct bnxt *bp, uint16_t vf,
			  const uint8_t *mac_addr);
int bnxt_hwrm_pf_evb_mode(struct bnxt *bp);
int bnxt_hwrm_func_bw_cfg(struct bnxt *bp, uint16_t vf,
			uint16_t max_bw, uint16_t enables);
int bnxt_hwrm_set_vf_vlan(struct bnxt *bp, int vf);
int bnxt_hwrm_func_qcfg_vf_default_mac(struct bnxt *bp, uint16_t vf,
				       struct rte_ether_addr *mac);
int bnxt_hwrm_func_qcfg_current_vf_vlan(struct bnxt *bp, int vf);
int bnxt_hwrm_tunnel_dst_port_alloc(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type);
int bnxt_hwrm_tunnel_dst_port_free(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type);
int bnxt_hwrm_set_default_vlan(struct bnxt *bp, int vf, uint8_t is_vf);
int bnxt_hwrm_port_qstats(struct bnxt *bp);
int bnxt_hwrm_port_clr_stats(struct bnxt *bp);
int bnxt_hwrm_port_led_cfg(struct bnxt *bp, bool led_on);
int bnxt_hwrm_port_led_qcaps(struct bnxt *bp);
int bnxt_hwrm_port_mac_qcfg(struct bnxt *bp);
int bnxt_hwrm_func_cfg_vf_set_flags(struct bnxt *bp, uint16_t vf,
					uint32_t flags);
void vf_vnic_set_rxmask_cb(struct bnxt_vnic_info *vnic, void *flagp);
int bnxt_set_rx_mask_no_vlan(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_vf_vnic_count(struct bnxt *bp, uint16_t vf);
int bnxt_hwrm_func_vf_vnic_query_and_config(struct bnxt *bp, uint16_t vf,
	void (*vnic_cb)(struct bnxt_vnic_info *, void *), void *cbdata,
	int (*hwrm_cb)(struct bnxt *bp, struct bnxt_vnic_info *vnic));
int bnxt_hwrm_func_cfg_vf_set_vlan_anti_spoof(struct bnxt *bp, uint16_t vf,
					      bool on);
int bnxt_hwrm_func_qcfg_vf_dflt_vnic_id(struct bnxt *bp, int vf);
int bnxt_hwrm_set_em_filter(struct bnxt *bp, uint16_t dst_id,
			struct bnxt_filter_info *filter);
int bnxt_hwrm_clear_em_filter(struct bnxt *bp, struct bnxt_filter_info *filter);

int bnxt_hwrm_set_ntuple_filter(struct bnxt *bp, uint16_t dst_id,
			 struct bnxt_filter_info *filter);
int bnxt_hwrm_clear_ntuple_filter(struct bnxt *bp,
				struct bnxt_filter_info *filter);
int bnxt_get_nvram_directory(struct bnxt *bp, uint32_t len, uint8_t *data);
int bnxt_hwrm_nvm_get_dir_info(struct bnxt *bp, uint32_t *entries,
			       uint32_t *length);
int bnxt_hwrm_get_nvram_item(struct bnxt *bp, uint32_t index,
			     uint32_t offset, uint32_t length,
			     uint8_t *data);
int bnxt_hwrm_erase_nvram_directory(struct bnxt *bp, uint8_t index);
int bnxt_hwrm_flash_nvram(struct bnxt *bp, uint16_t dir_type,
			  uint16_t dir_ordinal, uint16_t dir_ext,
			  uint16_t dir_attr, const uint8_t *data,
			  size_t data_len);
int bnxt_hwrm_ptp_cfg(struct bnxt *bp);
int bnxt_vnic_rss_configure(struct bnxt *bp,
			    struct bnxt_vnic_info *vnic);
int bnxt_hwrm_set_ring_coal(struct bnxt *bp,
			struct bnxt_coal *coal, uint16_t ring_id);
int bnxt_hwrm_check_vf_rings(struct bnxt *bp);
int bnxt_hwrm_ext_port_qstats(struct bnxt *bp);
int bnxt_hwrm_func_backing_store_qcaps(struct bnxt *bp);
int bnxt_hwrm_func_backing_store_cfg(struct bnxt *bp, uint32_t enables);
int bnxt_alloc_ctx_mem(struct bnxt *bp);
int bnxt_hwrm_tunnel_redirect(struct bnxt *bp, uint8_t type);
int bnxt_hwrm_tunnel_redirect_free(struct bnxt *bp, uint8_t type);
int bnxt_hwrm_tunnel_redirect_query(struct bnxt *bp, uint32_t *type);
int bnxt_hwrm_tunnel_redirect_info(struct bnxt *bp, uint8_t tun_type,
				   uint16_t *dst_fid);
int bnxt_hwrm_set_mac(struct bnxt *bp);
int bnxt_hwrm_if_change(struct bnxt *bp, bool state);
int bnxt_hwrm_error_recovery_qcfg(struct bnxt *bp);
int bnxt_hwrm_fw_reset(struct bnxt *bp);
int bnxt_hwrm_port_ts_query(struct bnxt *bp, uint8_t path,
			    uint64_t *timestamp);
int bnxt_hwrm_cfa_counter_qcaps(struct bnxt *bp, uint16_t *max_fc);
int bnxt_hwrm_ctx_rgtr(struct bnxt *bp, rte_iova_t dma_addr, uint16_t *ctx_id);
int bnxt_hwrm_ctx_unrgtr(struct bnxt *bp, uint16_t ctx_id);
int bnxt_hwrm_cfa_counter_cfg(struct bnxt *bp, enum bnxt_flow_dir dir,
			      uint16_t cntr, uint16_t ctx_id,
			      uint32_t num_entries, bool enable);
int bnxt_hwrm_cfa_counter_qstats(struct bnxt *bp,
				 enum bnxt_flow_dir dir,
				 uint16_t cntr,
				 uint16_t num_entries);
int bnxt_hwrm_get_dflt_vnic_svif(struct bnxt *bp, uint16_t fid,
				 uint16_t *vnic_id, uint16_t *svif);
int bnxt_hwrm_parent_pf_qcfg(struct bnxt *bp);
int bnxt_hwrm_port_phy_qcaps(struct bnxt *bp);
int bnxt_clear_one_vnic_filter(struct bnxt *bp,
			       struct bnxt_filter_info *filter);
void bnxt_free_vf_info(struct bnxt *bp);
int bnxt_hwrm_first_vf_id_query(struct bnxt *bp, uint16_t fid,
				uint16_t *first_vf_id);
int bnxt_hwrm_cfa_pair_exists(struct bnxt *bp, struct bnxt_representor *rep_bp);
int bnxt_hwrm_cfa_pair_alloc(struct bnxt *bp, struct bnxt_representor *rep);
int bnxt_hwrm_cfa_pair_free(struct bnxt *bp, struct bnxt_representor *rep);
int bnxt_hwrm_fw_echo_reply(struct bnxt *bp, uint32_t echo_req_data1,
			    uint32_t echo_req_data2);
int bnxt_hwrm_poll_ver_get(struct bnxt *bp);
int bnxt_hwrm_rx_ring_reset(struct bnxt *bp, int queue_index);
int bnxt_hwrm_ring_stats(struct bnxt *bp, uint32_t cid, int idx,
			 struct bnxt_ring_stats *stats, bool rx);
int bnxt_hwrm_read_sfp_module_eeprom_info(struct bnxt *bp, uint16_t i2c_addr,
					  uint16_t page_number, uint16_t start_addr,
					  uint16_t data_length, uint8_t *buf);
int bnxt_hwrm_stat_ctx_alloc(struct bnxt *bp, struct bnxt_cp_ring_info *cpr);
void bnxt_free_hwrm_tx_ring(struct bnxt *bp, int queue_index);
int bnxt_alloc_hwrm_tx_ring(struct bnxt *bp, int queue_index);
int bnxt_hwrm_config_host_mtu(struct bnxt *bp);
int bnxt_vnic_rss_clear_p5(struct bnxt *bp, struct bnxt_vnic_info *vnic);
#endif
