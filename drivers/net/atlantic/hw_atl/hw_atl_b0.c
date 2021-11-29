// SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
/* Copyright (C) 2014-2017 aQuantia Corporation. */

/* File hw_atl_b0.c: Definition of Atlantic hardware specific functions. */

#include "../atl_types.h"
#include "hw_atl_b0.h"

#include "../atl_hw_regs.h"
#include "hw_atl_utils.h"
#include "hw_atl_llh.h"
#include "hw_atl_b0_internal.h"
#include "hw_atl_llh_internal.h"
#include "../atl_logs.h"

int hw_atl_b0_hw_reset(struct aq_hw_s *self)
{
	int err = 0;

	err = hw_atl_utils_soft_reset(self);
	if (err)
		return err;

	self->aq_fw_ops->set_state(self, MPI_RESET);

	return err;
}

int hw_atl_b0_set_fc(struct aq_hw_s *self, u32 fc, u32 tc)
{
	hw_atl_rpb_rx_xoff_en_per_tc_set(self, !!(fc & AQ_NIC_FC_RX), tc);
	return 0;
}

static int hw_atl_b0_hw_qos_set(struct aq_hw_s *self)
{
	u32 tc = 0U;
	u32 buff_size = 0U;
	unsigned int i_priority = 0U;

	/* TPS Descriptor rate init */
	hw_atl_tps_tx_pkt_shed_desc_rate_curr_time_res_set(self, 0x0U);
	hw_atl_tps_tx_pkt_shed_desc_rate_lim_set(self, 0xA);

	/* TPS VM init */
	hw_atl_tps_tx_pkt_shed_desc_vm_arb_mode_set(self, 0U);

	/* TPS TC credits init */
	hw_atl_tps_tx_pkt_shed_desc_tc_arb_mode_set(self, 0U);
	hw_atl_tps_tx_pkt_shed_data_arb_mode_set(self, 0U);

	hw_atl_tps_tx_pkt_shed_tc_data_max_credit_set(self, 0xFFF, 0U);
	hw_atl_tps_tx_pkt_shed_tc_data_weight_set(self, 0x64, 0U);
	hw_atl_tps_tx_pkt_shed_desc_tc_max_credit_set(self, 0x50, 0U);
	hw_atl_tps_tx_pkt_shed_desc_tc_weight_set(self, 0x1E, 0U);

	/* Tx buf size */
	buff_size = HW_ATL_B0_TXBUF_MAX;

	hw_atl_tpb_tx_pkt_buff_size_per_tc_set(self, buff_size, tc);
	hw_atl_tpb_tx_buff_hi_threshold_per_tc_set(self,
						   (buff_size *
						   (1024 / 32U) * 66U) /
						   100U, tc);
	hw_atl_tpb_tx_buff_lo_threshold_per_tc_set(self,
						   (buff_size *
						   (1024 / 32U) * 50U) /
						   100U, tc);

	/* QoS Rx buf size per TC */
	tc = 0;
	buff_size = HW_ATL_B0_RXBUF_MAX;

	hw_atl_rpb_rx_pkt_buff_size_per_tc_set(self, buff_size, tc);
	hw_atl_rpb_rx_buff_hi_threshold_per_tc_set(self,
						   (buff_size *
						   (1024U / 32U) * 66U) /
						   100U, tc);
	hw_atl_rpb_rx_buff_lo_threshold_per_tc_set(self,
						   (buff_size *
						   (1024U / 32U) * 50U) /
						   100U, tc);
	hw_atl_rpb_rx_xoff_en_per_tc_set(self, 0U, tc);

	/* QoS 802.1p priority -> TC mapping */
	for (i_priority = 8U; i_priority--;)
		hw_atl_rpf_rpb_user_priority_tc_map_set(self, i_priority, 0U);

	return aq_hw_err_from_flags(self);
}

/* calc hash only in IPv4 header, regardless of presence of TCP */
#define pif_rpf_rss_ipv4_hdr_only_i     (1 << 4)
/* calc hash only if TCP header and IPv4 */
#define pif_rpf_rss_ipv4_tcp_hdr_only_i (1 << 3)
/* calc hash only in IPv6 header, regardless of presence of TCP */
#define pif_rpf_rss_ipv6_hdr_only_i     (1 << 2)
/* calc hash only if TCP header and IPv4 */
#define pif_rpf_rss_ipv6_tcp_hdr_only_i (1 << 1)
/* bug 5124 - rss hashing types - FIXME */
#define pif_rpf_rss_dont_use_udp_i      (1 << 0)

static int hw_atl_b0_hw_rss_hash_type_set(struct aq_hw_s *self)
{
	/* misc */
	unsigned int control_reg_val =
		IS_CHIP_FEATURE(RPF2) ? 0x000F0000U : 0x00000000U;

	/* RSS hash type set for IP/TCP */
	control_reg_val |= pif_rpf_rss_ipv4_hdr_only_i;//0x1EU;

	aq_hw_write_reg(self, 0x5040U, control_reg_val);

	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_rss_hash_set(struct aq_hw_s *self,
				     struct aq_rss_parameters *rss_params)
{
	struct aq_hw_cfg_s *cfg = self->aq_nic_cfg;
	int err = 0;
	unsigned int i = 0U;
	unsigned int addr = 0U;

	for (i = 10, addr = 0U; i--; ++addr) {
		u32 key_data = cfg->is_rss ?
			htonl(rss_params->hash_secret_key[i]) : 0U;
		hw_atl_rpf_rss_key_wr_data_set(self, key_data);
		hw_atl_rpf_rss_key_addr_set(self, addr);
		hw_atl_rpf_rss_key_wr_en_set(self, 1U);
		AQ_HW_WAIT_FOR(hw_atl_rpf_rss_key_wr_en_get(self) == 0,
			       1000U, 10U);
		if (err < 0)
			goto err_exit;
	}

	/* RSS Ring selection */
	hw_atl_reg_rx_flr_rss_control1set(self,
				cfg->is_rss ? 0xB3333333U : 0x00000000U);
	hw_atl_b0_hw_rss_hash_type_set(self);

	err = aq_hw_err_from_flags(self);

err_exit:
	return err;
}


int hw_atl_b0_hw_rss_set(struct aq_hw_s *self,
			struct aq_rss_parameters *rss_params)
{
	u8 *indirection_table = rss_params->indirection_table;
	u32 num_rss_queues = max(1U, self->aq_nic_cfg->num_rss_queues);
	u32 i = 0;
	u32 addr = 0;
	u32 val = 0;
	u32 shift = 0;
	int err = 0;

	for (i = 0; i < HW_ATL_B0_RSS_REDIRECTION_MAX; i++) {
		val |= (u32)(indirection_table[i] % num_rss_queues) << shift;
		shift += 3;

		if (shift < 16)
			continue;

		hw_atl_rpf_rss_redir_tbl_wr_data_set(self, val & 0xffff);
		hw_atl_rpf_rss_redir_tbl_addr_set(self, addr);

		hw_atl_rpf_rss_redir_wr_en_set(self, 1U);
		AQ_HW_WAIT_FOR(hw_atl_rpf_rss_redir_wr_en_get(self) == 0,
			1000U, 10U);

		if (err < 0)
			goto err_exit;

		shift -= 16;
		val >>= 16;
		addr++;
	}

err_exit:
	return err;
}

static int hw_atl_b0_hw_offload_set(struct aq_hw_s *self)
				    /*struct aq_nic_cfg_s *aq_nic_cfg)*/
{
	unsigned int i;

	/* TX checksums offloads*/
	hw_atl_tpo_ipv4header_crc_offload_en_set(self, 1);
	hw_atl_tpo_tcp_udp_crc_offload_en_set(self, 1);

	/* RX checksums offloads*/
	hw_atl_rpo_ipv4header_crc_offload_en_set(self, 1);
	hw_atl_rpo_tcp_udp_crc_offload_en_set(self, 1);

	/* LSO offloads*/
	hw_atl_tdm_large_send_offload_en_set(self, 0xFFFFFFFFU);

/* LRO offloads */
	{
		unsigned int val = (8U < HW_ATL_B0_LRO_RXD_MAX) ? 0x3U :
			((4U < HW_ATL_B0_LRO_RXD_MAX) ? 0x2U :
			((2U < HW_ATL_B0_LRO_RXD_MAX) ? 0x1U : 0x0));

		for (i = 0; i < HW_ATL_B0_RINGS_MAX; i++)
			hw_atl_rpo_lro_max_num_of_descriptors_set(self, val, i);

		hw_atl_rpo_lro_time_base_divider_set(self, 0x61AU);
		hw_atl_rpo_lro_inactive_interval_set(self, 0);
		hw_atl_rpo_lro_max_coalescing_interval_set(self, 2);

		hw_atl_rpo_lro_qsessions_lim_set(self, 1U);

		hw_atl_rpo_lro_total_desc_lim_set(self, 2U);

		hw_atl_rpo_lro_patch_optimization_en_set(self, 0U);

		hw_atl_rpo_lro_min_pay_of_first_pkt_set(self, 10U);

		hw_atl_rpo_lro_pkt_lim_set(self, 1U);

		hw_atl_rpo_lro_en_set(self,
				self->aq_nic_cfg->is_lro ? 0xFFFFFFFFU : 0U);
	}
	return aq_hw_err_from_flags(self);
}

static
int hw_atl_b0_hw_init_tx_path(struct aq_hw_s *self)
{
	/* Tx TC/RSS number config */
	hw_atl_rpb_tps_tx_tc_mode_set(self, 1U);

	hw_atl_thm_lso_tcp_flag_of_first_pkt_set(self, 0x0FF6U);
	hw_atl_thm_lso_tcp_flag_of_middle_pkt_set(self, 0x0FF6U);
	hw_atl_thm_lso_tcp_flag_of_last_pkt_set(self, 0x0F7FU);

	/* Tx interrupts */
	hw_atl_tdm_tx_desc_wr_wb_irq_en_set(self, 0U);

	/* misc */
	aq_hw_write_reg(self, 0x00007040U, IS_CHIP_FEATURE(TPO2) ?
			0x00010000U : 0x00000000U);
	hw_atl_tdm_tx_dca_en_set(self, 0U);
	hw_atl_tdm_tx_dca_mode_set(self, 0U);

	hw_atl_tpb_tx_path_scp_ins_en_set(self, 1U);

	return aq_hw_err_from_flags(self);
}

static
int hw_atl_b0_hw_init_rx_path(struct aq_hw_s *self)
{
	struct aq_hw_cfg_s *cfg = self->aq_nic_cfg;
	int i;

	/* Rx TC/RSS number config */
	hw_atl_rpb_rpf_rx_traf_class_mode_set(self, 1U); /* 1: 4TC/8Queues */

	/* Rx flow control */
	hw_atl_rpb_rx_flow_ctl_mode_set(self, 1U);

	/* RSS Ring selection */
	hw_atl_reg_rx_flr_rss_control1set(self, cfg->is_rss ?
					0xB3333333U : 0x00000000U);

	/* Multicast filters */
	for (i = HW_ATL_B0_MAC_MAX; i--;) {
		hw_atl_rpfl2_uc_flr_en_set(self, (i == 0U) ? 1U : 0U, i);
		hw_atl_rpfl2unicast_flr_act_set(self, 1U, i);
	}

	hw_atl_reg_rx_flr_mcst_flr_msk_set(self, 0x00000000U);
	hw_atl_reg_rx_flr_mcst_flr_set(self, 0x00010FFFU, 0U);

	/* Vlan filters */
	hw_atl_rpf_vlan_outer_etht_set(self, 0x88A8U);
	hw_atl_rpf_vlan_inner_etht_set(self, 0x8100U);

	/* VLAN promisc by default */
	hw_atl_rpf_vlan_prom_mode_en_set(self, 1);

	/* Rx Interrupts */
	hw_atl_rdm_rx_desc_wr_wb_irq_en_set(self, 0U);

	hw_atl_b0_hw_rss_hash_type_set(self);

	hw_atl_rpfl2broadcast_flr_act_set(self, 1U);
	hw_atl_rpfl2broadcast_count_threshold_set(self, 0xFFFFU & (~0U / 256U));

	hw_atl_rpfl2broadcast_en_set(self, 1U);

	hw_atl_rdm_rx_dca_en_set(self, 0U);
	hw_atl_rdm_rx_dca_mode_set(self, 0U);

	return aq_hw_err_from_flags(self);
}

static int hw_atl_b0_hw_mac_addr_set(struct aq_hw_s *self, u8 *mac_addr)
{
	int err = 0;
	unsigned int h = 0U;
	unsigned int l = 0U;

	if (!mac_addr) {
		err = -EINVAL;
		goto err_exit;
	}
	h = (mac_addr[0] << 8) | (mac_addr[1]);
	l = (mac_addr[2] << 24) | (mac_addr[3] << 16) |
		(mac_addr[4] << 8) | mac_addr[5];

	hw_atl_rpfl2_uc_flr_en_set(self, 0U, HW_ATL_B0_MAC);
	hw_atl_rpfl2unicast_dest_addresslsw_set(self, l, HW_ATL_B0_MAC);
	hw_atl_rpfl2unicast_dest_addressmsw_set(self, h, HW_ATL_B0_MAC);
	hw_atl_rpfl2_uc_flr_en_set(self, 1U, HW_ATL_B0_MAC);

	err = aq_hw_err_from_flags(self);

err_exit:
	return err;
}

int hw_atl_b0_hw_init(struct aq_hw_s *self, u8 *mac_addr)
{
	static u32 aq_hw_atl_igcr_table_[4][2] = {
		{ 0x20000080U, 0x20000080U }, /* AQ_IRQ_INVALID */
		{ 0x20000080U, 0x20000080U }, /* AQ_IRQ_LEGACY */
		{ 0x20000021U, 0x20000025U }, /* AQ_IRQ_MSI */
		{ 0x200000A2U, 0x200000A6U }  /* AQ_IRQ_MSIX */
	};

	int err = 0;
	u32 val;

	struct aq_hw_cfg_s *aq_nic_cfg = self->aq_nic_cfg;

	hw_atl_b0_hw_init_tx_path(self);
	hw_atl_b0_hw_init_rx_path(self);

	hw_atl_b0_hw_mac_addr_set(self, mac_addr);

	self->aq_fw_ops->set_link_speed(self, aq_nic_cfg->link_speed_msk);
	self->aq_fw_ops->set_state(self, MPI_INIT);

	hw_atl_b0_hw_qos_set(self);
	hw_atl_b0_hw_rss_set(self, &aq_nic_cfg->aq_rss);
	hw_atl_b0_hw_rss_hash_set(self, &aq_nic_cfg->aq_rss);

	/* Force limit MRRS on RDM/TDM to 2K */
	val = aq_hw_read_reg(self, HW_ATL_PCI_REG_CONTROL6_ADR);
	aq_hw_write_reg(self, HW_ATL_PCI_REG_CONTROL6_ADR,
			(val & ~0x707) | 0x404);

	/* TX DMA total request limit. B0 hardware is not capable to
	 * handle more than (8K-MRRS) incoming DMA data.
	 * Value 24 in 256byte units
	 */
	aq_hw_write_reg(self, HW_ATL_TX_DMA_TOTAL_REQ_LIMIT_ADR, 24);

	/* Reset link status and read out initial hardware counters */
	self->aq_link_status.mbps = 0;
	self->aq_fw_ops->update_stats(self);

	err = aq_hw_err_from_flags(self);
	if (err < 0)
		goto err_exit;

	/* Interrupts */
	hw_atl_reg_irq_glb_ctl_set(self,
				   aq_hw_atl_igcr_table_[aq_nic_cfg->irq_type]
					 [(aq_nic_cfg->vecs > 1U) ?
					 1 : 0]);

	hw_atl_itr_irq_auto_masklsw_set(self, 0xffffffff);

	/* Interrupts */
	hw_atl_reg_gen_irq_map_set(self, 0, 0);
	hw_atl_reg_gen_irq_map_set(self, 0x80 | ATL_IRQ_CAUSE_LINK, 3);

	hw_atl_b0_hw_offload_set(self);

err_exit:
	return err;
}

int hw_atl_b0_hw_ring_tx_start(struct aq_hw_s *self, int index)
{
	hw_atl_tdm_tx_desc_en_set(self, 1, index);
	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_ring_rx_start(struct aq_hw_s *self, int index)
{
	hw_atl_rdm_rx_desc_en_set(self, 1, index);
	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_start(struct aq_hw_s *self)
{
	hw_atl_tpb_tx_buff_en_set(self, 1);
	hw_atl_rpb_rx_buff_en_set(self, 1);
	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_tx_ring_tail_update(struct aq_hw_s *self, int tail, int index)
{
	hw_atl_reg_tx_dma_desc_tail_ptr_set(self, tail, index);
	return 0;
}

int hw_atl_b0_hw_ring_rx_init(struct aq_hw_s *self, uint64_t base_addr,
		int index, int size, int buff_size, int cpu, int vec)
{
	u32 dma_desc_addr_lsw = (u32)base_addr;
	u32 dma_desc_addr_msw = (u32)(base_addr >> 32);

	hw_atl_rdm_rx_desc_en_set(self, false, index);

	hw_atl_rdm_rx_desc_head_splitting_set(self, 0U, index);

	hw_atl_reg_rx_dma_desc_base_addresslswset(self, dma_desc_addr_lsw,
						  index);

	hw_atl_reg_rx_dma_desc_base_addressmswset(self, dma_desc_addr_msw,
						  index);

	hw_atl_rdm_rx_desc_len_set(self, size / 8U, index);

	hw_atl_rdm_rx_desc_data_buff_size_set(self, buff_size / 1024U, index);

	hw_atl_rdm_rx_desc_head_buff_size_set(self, 0U, index);
	hw_atl_rdm_rx_desc_head_splitting_set(self, 0U, index);
	hw_atl_rpo_rx_desc_vlan_stripping_set(self, 0U, index);

	/* Rx ring set mode */

	/* Mapping interrupt vector */
	hw_atl_itr_irq_map_rx_set(self, vec, index);
	hw_atl_itr_irq_map_en_rx_set(self, true, index);

	hw_atl_rdm_cpu_id_set(self, cpu, index);
	hw_atl_rdm_rx_desc_dca_en_set(self, 0U, index);
	hw_atl_rdm_rx_head_dca_en_set(self, 0U, index);
	hw_atl_rdm_rx_pld_dca_en_set(self, 0U, index);

	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_ring_tx_init(struct aq_hw_s *self, uint64_t base_addr,
			      int index, int size, int cpu, int vec)
{
	u32 dma_desc_lsw_addr = (u32)base_addr;
	u32 dma_desc_msw_addr = (u32)(base_addr >> 32);

	hw_atl_reg_tx_dma_desc_base_addresslswset(self, dma_desc_lsw_addr,
						  index);

	hw_atl_reg_tx_dma_desc_base_addressmswset(self, dma_desc_msw_addr,
						  index);

	hw_atl_tdm_tx_desc_len_set(self, size / 8U, index);

	hw_atl_b0_hw_tx_ring_tail_update(self, 0, index);

	/* Set Tx threshold */
	hw_atl_tdm_tx_desc_wr_wb_threshold_set(self, 0U, index);

	/* Mapping interrupt vector */
	hw_atl_itr_irq_map_tx_set(self, vec, index);
	hw_atl_itr_irq_map_en_tx_set(self, true, index);

	hw_atl_tdm_cpu_id_set(self, cpu, index);
	hw_atl_tdm_tx_desc_dca_en_set(self, 0U, index);

	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_irq_enable(struct aq_hw_s *self, u64 mask)
{
	hw_atl_itr_irq_msk_setlsw_set(self, LODWORD(mask));
	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_irq_disable(struct aq_hw_s *self, u64 mask)
{
	hw_atl_itr_irq_msk_clearlsw_set(self, LODWORD(mask));
	hw_atl_itr_irq_status_clearlsw_set(self, LODWORD(mask));

	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_irq_read(struct aq_hw_s *self, u64 *mask)
{
	*mask = hw_atl_itr_irq_statuslsw_get(self);
	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_ring_tx_stop(struct aq_hw_s *self, int index)
{
	hw_atl_tdm_tx_desc_en_set(self, 0U, index);
	return aq_hw_err_from_flags(self);
}

int hw_atl_b0_hw_ring_rx_stop(struct aq_hw_s *self, int index)
{
	hw_atl_rdm_rx_desc_en_set(self, 0U, index);
	return aq_hw_err_from_flags(self);
}
