/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2014-2018 Broadcom
 * All rights reserved.
 */

#ifndef _BNXT_HWRM_H_
#define _BNXT_HWRM_H_

#include <inttypes.h>
#include <stdbool.h>

struct bnxt;
struct bnxt_filter_info;
struct bnxt_cp_ring_info;

#define HWRM_SEQ_ID_INVALID -1U
/* Convert Bit field location to value */
#define ASYNC_CMPL_EVENT_ID_LINK_STATUS_CHANGE	\
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE)
#define ASYNC_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED	\
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED)
#define ASYNC_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE	\
	(1 << HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_CHANGE)
#define ASYNC_CMPL_EVENT_ID_PF_DRVR_UNLOAD	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD - 32))
#define ASYNC_CMPL_EVENT_ID_VF_CFG_CHANGE	\
	(1 << (HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_CFG_CHANGE - 32))

#define HWRM_QUEUE_SERVICE_PROFILE_LOSSY \
	HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSY

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

int bnxt_hwrm_func_buf_rgtr(struct bnxt *bp);
int bnxt_hwrm_func_buf_unrgtr(struct bnxt *bp);
int bnxt_hwrm_func_driver_register(struct bnxt *bp);
int bnxt_hwrm_func_qcaps(struct bnxt *bp);
int bnxt_hwrm_func_reset(struct bnxt *bp);
int bnxt_hwrm_func_driver_unregister(struct bnxt *bp, uint32_t flags);
int bnxt_hwrm_func_qstats(struct bnxt *bp, uint16_t fid,
			  struct rte_eth_stats *stats);
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
			 uint32_t stats_ctx_id, uint32_t cmpl_ring_id);
int bnxt_hwrm_ring_free(struct bnxt *bp,
			struct bnxt_ring *ring, uint32_t ring_type);
int bnxt_hwrm_ring_grp_alloc(struct bnxt *bp, unsigned int idx);
int bnxt_hwrm_ring_grp_free(struct bnxt *bp, unsigned int idx);

int bnxt_hwrm_stat_clear(struct bnxt *bp, struct bnxt_cp_ring_info *cpr);
int bnxt_hwrm_stat_ctx_alloc(struct bnxt *bp,
			     struct bnxt_cp_ring_info *cpr, unsigned int idx);
int bnxt_hwrm_stat_ctx_free(struct bnxt *bp,
			    struct bnxt_cp_ring_info *cpr, unsigned int idx);
int bnxt_hwrm_ctx_qstats(struct bnxt *bp, uint32_t cid, int idx,
			 struct rte_eth_stats *stats, uint8_t rx);

int bnxt_hwrm_ver_get(struct bnxt *bp);

int bnxt_hwrm_vnic_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_cfg(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_qcfg(struct bnxt *bp, struct bnxt_vnic_info *vnic,
				int16_t fw_vf_id);
int bnxt_hwrm_vnic_ctx_alloc(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_ctx_free(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_free(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_rss_cfg(struct bnxt *bp,
			   struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_plcmode_cfg(struct bnxt *bp,
				struct bnxt_vnic_info *vnic);
int bnxt_hwrm_vnic_tpa_cfg(struct bnxt *bp,
			   struct bnxt_vnic_info *vnic, bool enable);

int bnxt_alloc_all_hwrm_stat_ctxs(struct bnxt *bp);
int bnxt_clear_all_hwrm_stat_ctxs(struct bnxt *bp);
int bnxt_free_all_hwrm_stat_ctxs(struct bnxt *bp);
int bnxt_free_all_hwrm_rings(struct bnxt *bp);
int bnxt_free_all_hwrm_ring_grps(struct bnxt *bp);
int bnxt_alloc_all_hwrm_ring_grps(struct bnxt *bp);
int bnxt_set_hwrm_vnic_filters(struct bnxt *bp, struct bnxt_vnic_info *vnic);
int bnxt_clear_hwrm_vnic_filters(struct bnxt *bp, struct bnxt_vnic_info *vnic);
void bnxt_free_all_hwrm_resources(struct bnxt *bp);
void bnxt_free_hwrm_resources(struct bnxt *bp);
int bnxt_alloc_hwrm_resources(struct bnxt *bp);
int bnxt_get_hwrm_link_config(struct bnxt *bp, struct rte_eth_link *link);
int bnxt_set_hwrm_link_config(struct bnxt *bp, bool link_up);
int bnxt_hwrm_func_qcfg(struct bnxt *bp);
int bnxt_hwrm_func_resc_qcaps(struct bnxt *bp);
int bnxt_hwrm_func_reserve_vf_resc(struct bnxt *bp);
int bnxt_hwrm_allocate_pf_only(struct bnxt *bp);
int bnxt_hwrm_allocate_vfs(struct bnxt *bp, int num_vfs);
int bnxt_hwrm_func_vf_mac(struct bnxt *bp, uint16_t vf,
			  const uint8_t *mac_addr);
int bnxt_hwrm_pf_evb_mode(struct bnxt *bp);
int bnxt_hwrm_func_bw_cfg(struct bnxt *bp, uint16_t vf,
			uint16_t max_bw, uint16_t enables);
int bnxt_hwrm_set_vf_vlan(struct bnxt *bp, int vf);
int bnxt_hwrm_func_qcfg_vf_default_mac(struct bnxt *bp, uint16_t vf,
				       struct ether_addr *mac);
int bnxt_hwrm_func_qcfg_current_vf_vlan(struct bnxt *bp, int vf);
int bnxt_hwrm_tunnel_dst_port_alloc(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type);
int bnxt_hwrm_tunnel_dst_port_free(struct bnxt *bp, uint16_t port,
				uint8_t tunnel_type);
void bnxt_free_tunnel_ports(struct bnxt *bp);
int bnxt_hwrm_set_default_vlan(struct bnxt *bp, int vf, uint8_t is_vf);
int bnxt_hwrm_port_qstats(struct bnxt *bp);
int bnxt_hwrm_port_clr_stats(struct bnxt *bp);
int bnxt_hwrm_port_led_cfg(struct bnxt *bp, bool led_on);
int bnxt_hwrm_port_led_qcaps(struct bnxt *bp);
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
#endif
