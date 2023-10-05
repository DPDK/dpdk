/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 HiSilicon Limited
 */

#include <rte_malloc.h>

#include "hns3_common.h"
#include "hns3_logs.h"
#include "hns3_regs.h"
#include "hns3_rxtx.h"
#include "hns3_dump.h"

#define HNS3_BD_DW_NUM 8
#define HNS3_BD_ADDRESS_LAST_DW 2

static const char *
hns3_get_adapter_state_name(enum hns3_adapter_state state)
{
	const struct {
		enum hns3_adapter_state state;
		const char *name;
	} adapter_state_name[] = {
		{HNS3_NIC_UNINITIALIZED, "UNINITIALIZED"},
		{HNS3_NIC_INITIALIZED, "INITIALIZED"},
		{HNS3_NIC_CONFIGURING, "CONFIGURING"},
		{HNS3_NIC_CONFIGURED, "CONFIGURED"},
		{HNS3_NIC_STARTING, "STARTING"},
		{HNS3_NIC_STARTED, "STARTED"},
		{HNS3_NIC_STOPPING, "STOPPING"},
		{HNS3_NIC_CLOSING, "CLOSING"},
		{HNS3_NIC_CLOSED, "CLOSED"},
		{HNS3_NIC_REMOVED, "REMOVED"},
		{HNS3_NIC_NSTATES, "NSTATES"},
	};
	uint32_t i;

	for (i = 0; i < RTE_DIM(adapter_state_name); i++)
		if (state == adapter_state_name[i].state)
			return adapter_state_name[i].name;

	return "Unknown";
}

static const char *
hns3_get_io_func_hint_name(uint32_t hint)
{
	switch (hint) {
	case HNS3_IO_FUNC_HINT_NONE:
		return "none";
	case HNS3_IO_FUNC_HINT_VEC:
		return "vec";
	case HNS3_IO_FUNC_HINT_SVE:
		return "sve";
	case HNS3_IO_FUNC_HINT_SIMPLE:
		return "simple";
	case HNS3_IO_FUNC_HINT_COMMON:
		return "common";
	default:
		return "unknown";
	}
}

static void
hns3_get_dev_mac_info(FILE *file, struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;

	fprintf(file, "  - MAC Info:\n");
	fprintf(file,
		"\t  -- media_type=%s\n"
		"\t  -- query_type=%u\n"
		"\t  -- supported_speed=0x%x\n"
		"\t  -- advertising=0x%x\n"
		"\t  -- lp_advertising=0x%x\n"
		"\t  -- support_autoneg=%s\n"
		"\t  -- support_fc_autoneg=%s\n",
		hns3_get_media_type_name(hw->mac.media_type),
		hw->mac.query_type,
		hw->mac.supported_speed,
		hw->mac.advertising,
		hw->mac.lp_advertising,
		hw->mac.support_autoneg != 0 ? "Yes" : "No",
		pf->support_fc_autoneg ? "Yes" : "No");
}

static void
hns3_get_dev_feature_capability(FILE *file, struct hns3_hw *hw)
{
	const struct {
		enum hns3_dev_cap cap;
		const char *name;
	} caps_name[] = {
		{HNS3_DEV_SUPPORT_DCB_B, "DCB"},
		{HNS3_DEV_SUPPORT_COPPER_B, "COPPER"},
		{HNS3_DEV_SUPPORT_FD_QUEUE_REGION_B, "FD QUEUE REGION"},
		{HNS3_DEV_SUPPORT_PTP_B, "PTP"},
		{HNS3_DEV_SUPPORT_TX_PUSH_B, "TX PUSH"},
		{HNS3_DEV_SUPPORT_INDEP_TXRX_B, "INDEP TXRX"},
		{HNS3_DEV_SUPPORT_STASH_B, "STASH"},
		{HNS3_DEV_SUPPORT_SIMPLE_BD_B, "SIMPLE BD"},
		{HNS3_DEV_SUPPORT_RXD_ADV_LAYOUT_B, "RXD Advanced Layout"},
		{HNS3_DEV_SUPPORT_OUTER_UDP_CKSUM_B, "OUTER UDP CKSUM"},
		{HNS3_DEV_SUPPORT_RAS_IMP_B, "RAS IMP"},
		{HNS3_DEV_SUPPORT_TM_B, "TM"},
		{HNS3_DEV_SUPPORT_VF_VLAN_FLT_MOD_B, "VF VLAN FILTER MOD"},
		{HNS3_DEV_SUPPORT_FC_AUTO_B, "FC AUTO"}
	};
	uint32_t i;

	fprintf(file, "  - Dev Capability:\n");
	for (i = 0; i < RTE_DIM(caps_name); i++)
		fprintf(file, "\t  -- support %s: %s\n", caps_name[i].name,
			hns3_get_bit(hw->capability, caps_name[i].cap) ? "Yes" :
									 "No");
}

static const char *
hns3_get_fdir_tuple_name(uint32_t index)
{
	const char * const tuple_name[] = {
		"outer_dst_mac",
		"outer_src_mac",
		"outer_vlan_1st_tag",
		"outer_vlan_2nd_tag",
		"outer_eth_type",
		"outer_l2_rsv",
		"outer_ip_tos",
		"outer_ip_proto",
		"outer_src_ip",
		"outer_dst_ip",
		"outer_l3_rsv",
		"outer_src_port",
		"outer_dst_port",
		"outer_l4_rsv",
		"outer_tun_vni",
		"outer_tun_flow_id",
		"inner_dst_mac",
		"inner_src_mac",
		"inner_vlan_tag1",
		"inner_vlan_tag2",
		"inner_eth_type",
		"inner_l2_rsv",
		"inner_ip_tos",
		"inner_ip_proto",
		"inner_src_ip",
		"inner_dst_ip",
		"inner_l3_rsv",
		"inner_src_port",
		"inner_dst_port",
		"inner_sctp_tag",
	};
	if (index < RTE_DIM(tuple_name))
		return tuple_name[index];
	else
		return "unknown";
}

static void
hns3_get_fdir_basic_info(FILE *file, struct hns3_pf *pf)
{
#define HNS3_PERLINE_TUPLE_NAME_LEN	4
	struct hns3_fd_cfg *fdcfg = &pf->fdir.fd_cfg;
	uint32_t i, count = 0;

	fprintf(file, "  - Fdir Info:\n");
	fprintf(file,
		"\t  -- mode=%u max_key_len=%u rule_num:%u cnt_num:%u\n"
		"\t  -- key_sel=%u tuple_active=0x%x meta_data_active=0x%x\n"
		"\t  -- ipv6_word_en: in_s=%u in_d=%u out_s=%u out_d=%u\n"
		"\t  -- active_tuples:\n",
		fdcfg->fd_mode, fdcfg->max_key_length,
		fdcfg->rule_num[HNS3_FD_STAGE_1],
		fdcfg->cnt_num[HNS3_FD_STAGE_1],
		fdcfg->key_cfg[HNS3_FD_STAGE_1].key_sel,
		fdcfg->key_cfg[HNS3_FD_STAGE_1].tuple_active,
		fdcfg->key_cfg[HNS3_FD_STAGE_1].meta_data_active,
		fdcfg->key_cfg[HNS3_FD_STAGE_1].inner_sipv6_word_en,
		fdcfg->key_cfg[HNS3_FD_STAGE_1].inner_dipv6_word_en,
		fdcfg->key_cfg[HNS3_FD_STAGE_1].outer_sipv6_word_en,
		fdcfg->key_cfg[HNS3_FD_STAGE_1].outer_dipv6_word_en);

	for (i = 0; i < MAX_TUPLE; i++) {
		if (!(fdcfg->key_cfg[HNS3_FD_STAGE_1].tuple_active & BIT(i)))
			continue;
		if (count % HNS3_PERLINE_TUPLE_NAME_LEN == 0)
			fprintf(file, "\t      ");
		fprintf(file, " %s", hns3_get_fdir_tuple_name(i));
		count++;
		if (count % HNS3_PERLINE_TUPLE_NAME_LEN == 0)
			fprintf(file, "\n");
	}
	if (count % HNS3_PERLINE_TUPLE_NAME_LEN)
		fprintf(file, "\n");
}

static void
hns3_get_device_basic_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	fprintf(file,
		"  - Device Base Info:\n"
		"\t  -- name: %s\n"
		"\t  -- adapter_state=%s\n"
		"\t  -- nb_rx_queues=%u nb_tx_queues=%u\n"
		"\t  -- total_tqps_num=%u tqps_num=%u intr_tqps_num=%u\n"
		"\t  -- rss_size_max=%u alloc_rss_size=%u tx_qnum_per_tc=%u\n"
		"\t  -- min_tx_pkt_len=%u intr_mapping_mode=%u vlan_mode=%u\n"
		"\t  -- tso_mode=%u max_non_tso_bd_num=%u\n"
		"\t  -- max_tm_rate=%u Mbps\n"
		"\t  -- set link down: %s\n"
		"\t  -- rx_func_hint=%s tx_func_hint=%s\n"
		"\t  -- dev_flags: lsc=%d\n"
		"\t  -- intr_conf: lsc=%u rxq=%u\n",
		dev->data->name,
		hns3_get_adapter_state_name(hw->adapter_state),
		dev->data->nb_rx_queues, dev->data->nb_tx_queues,
		hw->total_tqps_num, hw->tqps_num, hw->intr_tqps_num,
		hw->rss_size_max, hw->alloc_rss_size, hw->tx_qnum_per_tc,
		hw->min_tx_pkt_len, hw->intr.mapping_mode, hw->vlan_mode,
		hw->tso_mode, hw->max_non_tso_bd_num,
		hw->max_tm_rate,
		hw->set_link_down ? "Yes" : "No",
		hns3_get_io_func_hint_name(hns->rx_func_hint),
		hns3_get_io_func_hint_name(hns->tx_func_hint),
		!!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC),
		dev->data->dev_conf.intr_conf.lsc,
		dev->data->dev_conf.intr_conf.rxq);
}

static struct hns3_rx_queue *
hns3_get_rx_queue(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_rx_queue *rxq;
	uint32_t queue_id;
	void **rx_queues;

	for (queue_id = 0; queue_id < dev->data->nb_rx_queues; queue_id++) {
		rx_queues = dev->data->rx_queues;
		if (rx_queues == NULL || rx_queues[queue_id] == NULL) {
			hns3_err(hw, "detect rx_queues is NULL!\n");
			return NULL;
		}

		rxq = (struct hns3_rx_queue *)rx_queues[queue_id];
		if (rxq->rx_deferred_start)
			continue;

		return rx_queues[queue_id];
	}

	return NULL;
}

static struct hns3_tx_queue *
hns3_get_tx_queue(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tx_queue *txq;
	uint32_t queue_id;
	void **tx_queues;

	for (queue_id = 0; queue_id < dev->data->nb_tx_queues; queue_id++) {
		tx_queues = dev->data->tx_queues;
		if (tx_queues == NULL || tx_queues[queue_id] == NULL) {
			hns3_err(hw, "detect tx_queues is NULL!\n");
			return NULL;
		}

		txq = (struct hns3_tx_queue *)tx_queues[queue_id];
		if (txq->tx_deferred_start)
			continue;

		return tx_queues[queue_id];
	}

	return NULL;
}

static void
hns3_get_rxtx_fake_queue_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	uint32_t queue_id = 0;
	void **rx_queues;
	void **tx_queues;

	if (hns3_dev_get_support(hw, INDEP_TXRX))
		return;

	if (dev->data->nb_rx_queues < dev->data->nb_tx_queues) {
		rx_queues = hw->fkq_data.rx_queues;
		if (rx_queues == NULL || rx_queues[queue_id] == NULL) {
			hns3_err(hw, "detect rx_queues is NULL!\n");
			return;
		}
		rxq = (struct hns3_rx_queue *)rx_queues[queue_id];

		fprintf(file,
			"\t  -- first fake_queue info:\n"
			"\t       Rx: port=%u nb_desc=%u free_thresh=%u\n",
			rxq->port_id, rxq->nb_rx_desc, rxq->rx_free_thresh);
	} else if (dev->data->nb_rx_queues > dev->data->nb_tx_queues) {
		tx_queues = hw->fkq_data.tx_queues;
		queue_id = 0;

		if (tx_queues == NULL || tx_queues[queue_id] == NULL) {
			hns3_err(hw, "detect tx_queues is NULL!\n");
			return;
		}
		txq = (struct hns3_tx_queue *)tx_queues[queue_id];

		fprintf(file,
			"\t  -- first fake_queue info:\n"
			"\t	  Tx: port=%u nb_desc=%u\n",
			txq->port_id, txq->nb_tx_desc);
	}
}

static void
hns3_get_queue_enable_state(struct hns3_hw *hw, uint32_t *queue_state,
			    uint32_t nb_queues, bool is_rxq)
{
#define HNS3_QUEUE_NUM_PER_STATS (sizeof(*queue_state) * HNS3_UINT8_BIT)
	uint32_t queue_en_reg;
	uint32_t reg_offset;
	uint32_t state;
	uint32_t i;

	queue_en_reg = is_rxq ? HNS3_RING_RX_EN_REG : HNS3_RING_TX_EN_REG;
	for (i = 0; i < nb_queues; i++) {
		reg_offset = hns3_get_tqp_reg_offset(i);
		state = hns3_read_dev(hw, reg_offset + HNS3_RING_EN_REG);
		if (hns3_dev_get_support(hw, INDEP_TXRX))
			state = state && hns3_read_dev(hw, reg_offset +
						       queue_en_reg);
		hns3_set_bit(queue_state[i / HNS3_QUEUE_NUM_PER_STATS],
				i % HNS3_QUEUE_NUM_PER_STATS, state);
	}
}

static void
hns3_print_queue_state_perline(FILE *file, const uint32_t *queue_state,
			       uint32_t nb_queues, uint32_t line_num)
{
#define HNS3_NUM_QUEUE_PER_LINE (sizeof(uint32_t) * HNS3_UINT8_BIT)
	uint32_t id = line_num * HNS3_NUM_QUEUE_PER_LINE;
	uint32_t i;

	for (i = 0; i < HNS3_NUM_QUEUE_PER_LINE; i++) {
		fprintf(file, "%1lx", hns3_get_bit(queue_state[line_num], i));

		if (id % HNS3_UINT8_BIT == HNS3_UINT8_BIT - 1) {
			fprintf(file, "%s",
				i == HNS3_NUM_QUEUE_PER_LINE - 1 ? "\n" : ":");
		}
		id++;
		if (id >= nb_queues) {
			fprintf(file, "\n");
			break;
		}
	}
}

static void
hns3_display_queue_enable_state(FILE *file, const uint32_t *queue_state,
				uint32_t nb_queues, bool is_rxq)
{
#define HNS3_NUM_QUEUE_PER_LINE (sizeof(uint32_t) * HNS3_UINT8_BIT)
	uint32_t i;

	fprintf(file, "\t       %s queue id | enable state bitMap\n",
			is_rxq ? "Rx" : "Tx");

	for (i = 0; i < (nb_queues - 1) / HNS3_NUM_QUEUE_PER_LINE + 1; i++) {
		uint32_t line_end = (i + 1) * HNS3_NUM_QUEUE_PER_LINE - 1;
		uint32_t line_start = i * HNS3_NUM_QUEUE_PER_LINE;
		fprintf(file, "\t       %04u - %04u | ", line_start,
			nb_queues - 1 > line_end ? line_end : nb_queues - 1);

		hns3_print_queue_state_perline(file, queue_state, nb_queues, i);
	}
}

static void
hns3_get_rxtx_queue_enable_state(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t *rx_queue_state;
	uint32_t *tx_queue_state;
	uint32_t nb_rx_queues;
	uint32_t nb_tx_queues;
	uint32_t bitmap_size;

	nb_rx_queues = dev->data->nb_rx_queues;
	nb_tx_queues = dev->data->nb_tx_queues;
	if (nb_rx_queues == 0) {
		fprintf(file, "\t  -- Rx queue number is 0\n");
		return;
	}
	if (nb_tx_queues == 0) {
		fprintf(file, "\t  -- Tx queue number is 0\n");
		return;
	}

	bitmap_size = (hw->tqps_num * sizeof(uint32_t) + HNS3_UINT32_BIT) /
			HNS3_UINT32_BIT;
	rx_queue_state = (uint32_t *)rte_zmalloc(NULL, bitmap_size, 0);
	if (rx_queue_state == NULL) {
		hns3_err(hw, "Failed to allocate memory for rx queue state!");
		return;
	}

	tx_queue_state = (uint32_t *)rte_zmalloc(NULL, bitmap_size, 0);
	if (tx_queue_state == NULL) {
		hns3_err(hw, "Failed to allocate memory for tx queue state!");
		rte_free(rx_queue_state);
		return;
	}

	fprintf(file, "\t  -- enable state:\n");
	hns3_get_queue_enable_state(hw, rx_queue_state, nb_rx_queues, true);
	hns3_display_queue_enable_state(file, rx_queue_state, nb_rx_queues,
					 true);

	hns3_get_queue_enable_state(hw, tx_queue_state, nb_tx_queues, false);
	hns3_display_queue_enable_state(file, tx_queue_state, nb_tx_queues,
					 false);
	rte_free(rx_queue_state);
	rte_free(tx_queue_state);
}

static void
hns3_get_rxtx_queue_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;

	rxq = hns3_get_rx_queue(dev);
	if (rxq == NULL)
		return;
	txq = hns3_get_tx_queue(dev);
	if (txq == NULL)
		return;
	fprintf(file, "  - Rx/Tx Queue Info:\n");
	fprintf(file,
		"\t  -- first queue rxtx info:\n"
		"\t       Rx: port=%u nb_desc=%u free_thresh=%u\n"
		"\t       Tx: port=%u nb_desc=%u\n"
		"\t  -- tx push: %s\n",
		rxq->port_id, rxq->nb_rx_desc, rxq->rx_free_thresh,
		txq->port_id, txq->nb_tx_desc,
		txq->tx_push_enable ? "enabled" : "disabled");

	hns3_get_rxtx_fake_queue_info(file, dev);
	hns3_get_rxtx_queue_enable_state(file, dev);
}

static int
hns3_get_vlan_filter_cfg(FILE *file, struct hns3_hw *hw)
{
#define HNS3_FILTER_TYPE_VF		0
#define HNS3_FILTER_TYPE_PORT		1
#define HNS3_FILTER_FE_NIC_INGRESS_B	BIT(0)
#define HNS3_FILTER_FE_NIC_EGRESS_B	BIT(1)
	struct hns3_vlan_filter_ctrl_cmd *req;
	struct hns3_cmd_desc desc;
	uint8_t i;
	int ret;

	static const uint32_t vlan_filter_type[] = {
		HNS3_FILTER_TYPE_PORT,
		HNS3_FILTER_TYPE_VF
	};

	for (i = 0; i < RTE_DIM(vlan_filter_type); i++) {
		hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_VLAN_FILTER_CTRL,
						true);
		req = (struct hns3_vlan_filter_ctrl_cmd *)desc.data;
		req->vlan_type = vlan_filter_type[i];
		req->vf_id = HNS3_PF_FUNC_ID;
		ret = hns3_cmd_send(hw, &desc, 1);
		if (ret != 0) {
			hns3_err(hw,
				"NIC IMP exec ret=%d desc_num=%d optcode=0x%x!",
				ret, 1, rte_le_to_cpu_16(desc.opcode));
			return ret;
		}
		fprintf(file,
			"\t  -- %s VLAN filter configuration\n"
			"\t       nic_ingress           :%s\n"
			"\t       nic_egress            :%s\n",
			req->vlan_type == HNS3_FILTER_TYPE_PORT ?
			"Port" : "VF",
			req->vlan_fe & HNS3_FILTER_FE_NIC_INGRESS_B ?
			"Enable" : "Disable",
			req->vlan_fe & HNS3_FILTER_FE_NIC_EGRESS_B ?
			"Enable" : "Disable");
	}

	return 0;
}

static int
hns3_get_vlan_rx_offload_cfg(FILE *file, struct hns3_hw *hw)
{
	struct hns3_vport_vtag_rx_cfg_cmd *req;
	struct hns3_cmd_desc desc;
	uint16_t vport_id;
	uint8_t bitmap;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_VLAN_PORT_RX_CFG, true);
	req = (struct hns3_vport_vtag_rx_cfg_cmd *)desc.data;
	vport_id = HNS3_PF_FUNC_ID;
	req->vf_offset = vport_id / HNS3_VF_NUM_PER_CMD;
	bitmap = 1 << (vport_id % HNS3_VF_NUM_PER_BYTE);
	req->vf_bitmap[req->vf_offset] = bitmap;

	/*
	 * current version VF is not supported when PF is driven by DPDK driver,
	 * just need to configure rx parameters for PF vport.
	 */
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret != 0) {
		hns3_err(hw,
			 "NIC firmware exec ret=%d optcode=0x%x!", ret,
			 rte_le_to_cpu_16(desc.opcode));
		return ret;
	}

	fprintf(file,
		"\t  -- RX VLAN configuration\n"
		"\t       vlan1_strip_en        :%s\n"
		"\t       vlan2_strip_en        :%s\n"
		"\t       vlan1_vlan_prionly    :%s\n"
		"\t       vlan2_vlan_prionly    :%s\n"
		"\t       vlan1_strip_discard   :%s\n"
		"\t       vlan2_strip_discard   :%s\n",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_REM_TAG1_EN_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_REM_TAG2_EN_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_SHOW_TAG1_EN_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_SHOW_TAG2_EN_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_DISCARD_TAG1_EN_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_DISCARD_TAG2_EN_B) ? "Enable" : "Disable");

	return 0;
}

static void
hns3_parse_tx_vlan_cfg(FILE *file, struct hns3_vport_vtag_tx_cfg_cmd *req)
{
#define VLAN_VID_MASK 0x0fff
#define VLAN_PRIO_SHIFT 13

	fprintf(file,
		"\t  -- TX VLAN configuration\n"
		"\t       accept_tag1           :%s\n"
		"\t       accept_untag1         :%s\n"
		"\t       insert_tag1_en        :%s\n"
		"\t       default_vlan_tag1 = %d, qos = %d\n"
		"\t       accept_tag2           :%s\n"
		"\t       accept_untag2         :%s\n"
		"\t       insert_tag2_en        :%s\n"
		"\t       default_vlan_tag2 = %d, qos = %d\n"
		"\t       vlan_shift_mode       :%s\n",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_ACCEPT_TAG1_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_ACCEPT_UNTAG1_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_PORT_INS_TAG1_EN_B) ? "Enable" : "Disable",
		req->def_vlan_tag1 & VLAN_VID_MASK,
		req->def_vlan_tag1 >> VLAN_PRIO_SHIFT,
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_ACCEPT_TAG2_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_ACCEPT_UNTAG2_B) ? "Enable" : "Disable",
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_PORT_INS_TAG2_EN_B) ? "Enable" : "Disable",
		req->def_vlan_tag2 & VLAN_VID_MASK,
		req->def_vlan_tag2 >> VLAN_PRIO_SHIFT,
		hns3_get_bit(req->vport_vlan_cfg,
			HNS3_TAG_SHIFT_MODE_EN_B) ? "Enable" :
			"Disable");
}

static int
hns3_get_vlan_tx_offload_cfg(FILE *file, struct hns3_hw *hw)
{
	struct hns3_vport_vtag_tx_cfg_cmd *req;
	struct hns3_cmd_desc desc;
	uint16_t vport_id;
	uint8_t bitmap;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_VLAN_PORT_TX_CFG, true);
	req = (struct hns3_vport_vtag_tx_cfg_cmd *)desc.data;
	vport_id = HNS3_PF_FUNC_ID;
	req->vf_offset = vport_id / HNS3_VF_NUM_PER_CMD;
	bitmap = 1 << (vport_id % HNS3_VF_NUM_PER_BYTE);
	req->vf_bitmap[req->vf_offset] = bitmap;
	/*
	 * current version VF is not supported when PF is driven by DPDK driver,
	 * just need to configure tx parameters for PF vport.
	 */
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret != 0) {
		hns3_err(hw,
			"NIC firmware exec ret=%d desc_num=%d optcode=0x%x!",
			ret, 1, rte_le_to_cpu_16(desc.opcode));
		return ret;
	}

	hns3_parse_tx_vlan_cfg(file, req);

	return 0;
}

static void
hns3_get_port_pvid_info(FILE *file, struct hns3_hw *hw)
{
	fprintf(file, "  - pvid status: %s\n",
		hw->port_base_vlan_cfg.state ? "On" : "Off");
}

static void
hns3_get_vlan_config_info(FILE *file, struct hns3_hw *hw)
{
	int ret;

	fprintf(file, "  - VLAN Config Info:\n");
	ret = hns3_get_vlan_filter_cfg(file, hw);
	if (ret < 0)
		return;

	ret = hns3_get_vlan_rx_offload_cfg(file, hw);
	if (ret < 0)
		return;

	ret = hns3_get_vlan_tx_offload_cfg(file, hw);
	if (ret < 0)
		return;
}

static void
hns3_get_tm_conf_shaper_info(FILE *file, struct hns3_tm_conf *conf)
{
	struct hns3_shaper_profile_list *shaper_profile_list =
		&conf->shaper_profile_list;
	struct hns3_tm_shaper_profile *shaper_profile;

	if (conf->nb_shaper_profile == 0)
		return;

	fprintf(file, "\t  -- shaper_profile:\n");
	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		fprintf(file,
			"\t       id=%u reference_count=%u peak_rate=%" PRIu64 "Bps\n",
			shaper_profile->shaper_profile_id,
			shaper_profile->reference_count,
			shaper_profile->profile.peak.rate);
	}
}

static void
hns3_get_tm_conf_port_node_info(FILE *file, struct hns3_tm_conf *conf)
{
	if (conf->root == NULL)
		return;

	fprintf(file,
		"\t  -- port_node:\n"
		"\t       node_id=%u reference_count=%u shaper_profile_id=%d\n",
		conf->root->id, conf->root->reference_count,
		conf->root->shaper_profile ?
		(int)conf->root->shaper_profile->shaper_profile_id : -1);
}

static void
hns3_get_tm_conf_tc_node_info(FILE *file, struct hns3_tm_conf *conf)
{
	struct hns3_tm_node_list *tc_list = &conf->tc_list;
	struct hns3_tm_node *tc_node[HNS3_MAX_TC_NUM];
	struct hns3_tm_node *tm_node;
	uint32_t tidx;

	if (conf->nb_tc_node == 0)
		return;

	fprintf(file, "\t  -- tc_node:\n");
	memset(tc_node, 0, sizeof(tc_node));
	TAILQ_FOREACH(tm_node, tc_list, node) {
		tidx = hns3_tm_calc_node_tc_no(conf, tm_node->id);
		if (tidx < HNS3_MAX_TC_NUM)
			tc_node[tidx] = tm_node;
	}

	for (tidx = 0; tidx < HNS3_MAX_TC_NUM; tidx++) {
		tm_node = tc_node[tidx];
		if (tm_node == NULL)
			continue;
		fprintf(file,
			"\t       id=%u TC%u reference_count=%u parent_id=%d "
			"shaper_profile_id=%d\n",
			tm_node->id, hns3_tm_calc_node_tc_no(conf, tm_node->id),
			tm_node->reference_count,
			tm_node->parent ? (int)tm_node->parent->id : -1,
			tm_node->shaper_profile ?
			(int)tm_node->shaper_profile->shaper_profile_id : -1);
	}
}

static void
hns3_get_tm_conf_queue_format_info(FILE *file, struct hns3_tm_node **queue_node,
				   uint32_t *queue_node_tc,
				   uint32_t nb_tx_queues)
{
#define HNS3_PERLINE_QUEUES	32
#define HNS3_PERLINE_STRIDE	8
	uint32_t i, j, line_num, start_queue_id, end_queue_id;

	line_num = (nb_tx_queues + HNS3_PERLINE_QUEUES - 1) /
		HNS3_PERLINE_QUEUES;
	for (i = 0; i < line_num; i++) {
		start_queue_id = i * HNS3_PERLINE_QUEUES;
		end_queue_id = (i + 1) * HNS3_PERLINE_QUEUES - 1;
		if (end_queue_id > nb_tx_queues - 1)
			end_queue_id = nb_tx_queues - 1;
		fprintf(file, "\t       %04u - %04u | ", start_queue_id,
			end_queue_id);
		for (j = start_queue_id; j < nb_tx_queues; j++) {
			if (j >= end_queue_id + 1)
				break;
			if (j > start_queue_id && j % HNS3_PERLINE_STRIDE == 0)
				fprintf(file, ":");
			fprintf(file, "%u",
				queue_node[j] ? queue_node_tc[j] :
				HNS3_MAX_TC_NUM);
		}
		fprintf(file, "\n");
	}
}

static void
hns3_get_tm_conf_queue_node_info(FILE *file, struct hns3_tm_conf *conf,
				 uint32_t nb_tx_queues)
{
	struct hns3_tm_node_list *queue_list = &conf->queue_list;
	uint32_t nb_queue_node = conf->nb_leaf_nodes_max + 1;
	struct hns3_tm_node *queue_node[nb_queue_node];
	uint32_t queue_node_tc[nb_queue_node];
	struct hns3_tm_node *tm_node;

	if (conf->nb_queue_node == 0)
		return;

	fprintf(file,
		"\t  -- queue_node:\n"
		"\t       tx queue id | mapped tc (8 mean node not exist)\n");

	memset(queue_node, 0, sizeof(queue_node));
	memset(queue_node_tc, 0, sizeof(queue_node_tc));
	nb_tx_queues = RTE_MIN(nb_tx_queues, nb_queue_node);
	TAILQ_FOREACH(tm_node, queue_list, node) {
		if (tm_node->id >= nb_queue_node)
			continue;
		queue_node[tm_node->id] = tm_node;
		queue_node_tc[tm_node->id] = tm_node->parent ?
			hns3_tm_calc_node_tc_no(conf, tm_node->parent->id) : 0;
		nb_tx_queues = RTE_MAX(nb_tx_queues, tm_node->id + 1);
	}

	hns3_get_tm_conf_queue_format_info(file, queue_node, queue_node_tc,
				      nb_tx_queues);
}

static void
hns3_get_tm_conf_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_conf *conf = &pf->tm_conf;

	if (!hns3_dev_get_support(hw, TM))
		return;

	fprintf(file, "  - TM config info:\n");
	fprintf(file,
		"\t  -- nb_leaf_nodes_max=%u nb_nodes_max=%u\n"
		"\t  -- nb_shaper_profile=%u nb_tc_node=%u nb_queue_node=%u\n"
		"\t  -- committed=%u\n",
		conf->nb_leaf_nodes_max, conf->nb_nodes_max,
		conf->nb_shaper_profile, conf->nb_tc_node, conf->nb_queue_node,
		conf->committed);

	hns3_get_tm_conf_shaper_info(file, conf);
	hns3_get_tm_conf_port_node_info(file, conf);
	hns3_get_tm_conf_tc_node_info(file, conf);
	hns3_get_tm_conf_queue_node_info(file, conf, dev->data->nb_tx_queues);
}

static void
hns3_fc_mode_to_rxtx_pause(enum hns3_fc_mode fc_mode, bool *rx_pause,
			   bool *tx_pause)
{
	switch (fc_mode) {
	case HNS3_FC_NONE:
		*tx_pause = false;
		*rx_pause = false;
		break;
	case HNS3_FC_RX_PAUSE:
		*rx_pause = true;
		*tx_pause = false;
		break;
	case HNS3_FC_TX_PAUSE:
		*rx_pause = false;
		*tx_pause = true;
		break;
	case HNS3_FC_FULL:
		*rx_pause = true;
		*tx_pause = true;
		break;
	default:
		*rx_pause = false;
		*tx_pause = false;
		break;
	}
}

static bool
hns3_is_link_fc_mode(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;

	if (hw->current_fc_status == HNS3_FC_STATUS_PFC)
		return false;

	if (hw->num_tc > 1 && !pf->support_multi_tc_pause)
		return false;

	return true;
}

static void
hns3_get_link_fc_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_fc_conf cur_fc_conf;
	bool rx_pause1;
	bool tx_pause1;
	bool rx_pause2;
	bool tx_pause2;
	int ret;

	if (!hns3_is_link_fc_mode(hns))
		return;

	ret = hns3_flow_ctrl_get(dev, &cur_fc_conf);
	if (ret)  {
		fprintf(file, "get device flow control info fail!\n");
		return;
	}

	hns3_fc_mode_to_rxtx_pause(hw->requested_fc_mode,
				   &rx_pause1, &tx_pause1);
	hns3_fc_mode_to_rxtx_pause((enum hns3_fc_mode)cur_fc_conf.mode,
				   &rx_pause2, &tx_pause2);

	fprintf(file,
		"\t  -- link_fc_info:\n"
		"\t       Requested fc:\n"
		"\t         Rx:	%s\n"
		"\t         Tx:	%s\n"
		"\t       Current fc:\n"
		"\t         Rx:	%s\n"
		"\t         Tx:	%s\n"
		"\t       Autonegotiate: %s\n"
		"\t       Pause time:	0x%x\n",
		rx_pause1 ? "On" : "Off", tx_pause1 ? "On" : "Off",
		rx_pause2 ? "On" : "Off", tx_pause2 ? "On" : "Off",
		cur_fc_conf.autoneg == RTE_ETH_LINK_AUTONEG ? "On" : "Off",
		cur_fc_conf.pause_time);
}

static void
hns3_get_flow_ctrl_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	fprintf(file, "  - Flow Ctrl Info:\n");
	fprintf(file,
		"\t  -- fc_common_info:\n"
		"\t       current_fc_status=%u\n"
		"\t       requested_fc_mode=%u\n",
		hw->current_fc_status,
		hw->requested_fc_mode);

	hns3_get_link_fc_info(file, dev);
}

int
hns3_eth_dev_priv_dump(struct rte_eth_dev *dev, FILE *file)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	rte_spinlock_lock(&hw->lock);

	hns3_get_device_basic_info(file, dev);
	hns3_get_dev_feature_capability(file, hw);
	hns3_get_rxtx_queue_info(file, dev);
	hns3_get_port_pvid_info(file, hw);

	/*
	 * VF only supports dumping basic info, feature capability and queue
	 * info.
	 */
	if (hns->is_vf) {
		rte_spinlock_unlock(&hw->lock);
		return 0;
	}

	hns3_get_dev_mac_info(file, hns);
	hns3_get_vlan_config_info(file, hw);
	hns3_get_fdir_basic_info(file, &hns->pf);
	hns3_get_tm_conf_info(file, dev);
	hns3_get_flow_ctrl_info(file, dev);

	rte_spinlock_unlock(&hw->lock);

	return 0;
}

int
hns3_rx_descriptor_dump(const struct rte_eth_dev *dev, uint16_t queue_id,
			uint16_t offset, uint16_t num, FILE *file)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_rx_queue *rxq = dev->data->rx_queues[queue_id];
	uint32_t *bd_data;
	uint16_t count = 0;
	uint16_t desc_id;
	int i;

	if (offset >= rxq->nb_rx_desc)
		return -EINVAL;

	if (num > rxq->nb_rx_desc) {
		hns3_err(hw, "Invalid BD num=%u\n", num);
		return -EINVAL;
	}

	while (count < num) {
		desc_id = (rxq->next_to_use + offset + count) % rxq->nb_rx_desc;
		bd_data = (uint32_t *)(&rxq->rx_ring[desc_id]);
		fprintf(file, "Rx queue id:%u BD id:%u\n", queue_id, desc_id);
		for (i = 0; i < HNS3_BD_DW_NUM; i++) {
			/*
			 * For the sake of security, first 8 bytes of BD which
			 * stands for physical address of packet should not be
			 * shown.
			 */
			if (i < HNS3_BD_ADDRESS_LAST_DW) {
				fprintf(file, "RX BD WORD[%d]:0x%08x\n", i, 0);
				continue;
			}
			fprintf(file, "RX BD WORD[%d]:0x%08x\n", i,
				*(bd_data + i));
		}
		count++;
	}

	return 0;
}

int
hns3_tx_descriptor_dump(const struct rte_eth_dev *dev, uint16_t queue_id,
			uint16_t offset, uint16_t num, FILE *file)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_tx_queue *txq = dev->data->tx_queues[queue_id];
	uint32_t *bd_data;
	uint16_t count = 0;
	uint16_t desc_id;
	int i;

	if (offset >= txq->nb_tx_desc)
		return -EINVAL;

	if (num > txq->nb_tx_desc) {
		hns3_err(hw, "Invalid BD num=%u\n", num);
		return -EINVAL;
	}

	while (count < num) {
		desc_id = (txq->next_to_use + offset + count) % txq->nb_tx_desc;
		bd_data = (uint32_t *)(&txq->tx_ring[desc_id]);
		fprintf(file, "Tx queue id:%u BD id:%u\n", queue_id, desc_id);
		for (i = 0; i < HNS3_BD_DW_NUM; i++) {
			/*
			 * For the sake of security, first 8 bytes of BD which
			 * stands for physical address of packet should not be
			 * shown.
			 */
			if (i < HNS3_BD_ADDRESS_LAST_DW) {
				fprintf(file, "TX BD WORD[%d]:0x%08x\n", i, 0);
				continue;
			}

			fprintf(file, "Tx BD WORD[%d]:0x%08x\n", i,
				*(bd_data + i));
		}
		count++;
	}

	return 0;
}
