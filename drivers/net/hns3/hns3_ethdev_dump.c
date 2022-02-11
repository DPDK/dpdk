/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 HiSilicon Limited
 */

#include <rte_kvargs.h>
#include <rte_bus_pci.h>
#include <ethdev_pci.h>
#include <rte_pci.h>

#include "hns3_common.h"
#include "hns3_logs.h"
#include "hns3_regs.h"
#include "hns3_rxtx.h"

static const char *
get_adapter_state_name(uint32_t state)
{
	static const char * const state_name[] = {
		"UNINITIALIZED",
		"INITIALIZED",
		"CONFIGURING",
		"CONFIGURED",
		"STARTING",
		"STARTED",
		"STOPPING",
		"CLOSING",
		"CLOSED",
		"REMOVED",
		"NSTATES"
	};

	if (state < RTE_DIM(state_name))
		return state_name[state];
	else
		return "unknown";
}

static const char *
get_io_func_hint_name(uint32_t hint)
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
get_dev_mac_info(FILE *file, struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_pf *pf = &hns->pf;

	fprintf(file, "  - MAC Info:\n");
	fprintf(file,
		"\t  -- query_type=%u\n"
		"\t  -- supported_speed=0x%x\n"
		"\t  -- advertising=0x%x\n"
		"\t  -- lp_advertising=0x%x\n"
		"\t  -- support_autoneg=%s\n"
		"\t  -- support_fc_autoneg=%s\n",
		hw->mac.query_type,
		hw->mac.supported_speed,
		hw->mac.advertising,
		hw->mac.lp_advertising,
		hw->mac.support_autoneg != 0 ? "Yes" : "No",
		pf->support_fc_autoneg ? "Yes" : "No");
}

static void
get_dev_feature_capability(FILE *file, struct hns3_hw *hw)
{
	const char * const caps_name[] = {
		"DCB",
		"COPPER",
		"FD QUEUE REGION",
		"PTP",
		"TX PUSH",
		"INDEP TXRX",
		"STASH",
		"SIMPLE BD",
		"RXD Advanced Layout",
		"OUTER UDP CKSUM",
		"RAS IMP",
		"TM",
		"VF VLAN FILTER MOD",
	};
	uint32_t i;

	fprintf(file, "  - Dev Capability:\n");
	for (i = 0; i < RTE_DIM(caps_name); i++)
		fprintf(file, "\t  -- support %s: %s\n", caps_name[i],
			hw->capability & BIT(i) ? "yes" : "no");
}

static const char *
get_fdir_tuple_name(uint32_t index)
{
	static const char * const tuple_name[] = {
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
get_fdir_basic_info(FILE *file, struct hns3_pf *pf)
{
#define TMPBUF_SIZE		2048
#define PERLINE_TUPLE_NAMES	4
	struct hns3_fd_cfg *fdcfg = &pf->fdir.fd_cfg;
	char tmpbuf[TMPBUF_SIZE] = {0};
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
		if (count % PERLINE_TUPLE_NAMES == 0)
			fprintf(file, "\t      ");
		fprintf(file, " %s", get_fdir_tuple_name(i));
		count++;
		if (count % PERLINE_TUPLE_NAMES == 0)
			fprintf(file, "\n");
	}
	if (count % PERLINE_TUPLE_NAMES)
		fprintf(file, "\n");

	fprintf(file, "%s", tmpbuf);
}

static void
get_device_basic_info(FILE *file, struct rte_eth_dev *dev)
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
		get_adapter_state_name(hw->adapter_state),
		dev->data->nb_rx_queues, dev->data->nb_tx_queues,
		hw->total_tqps_num, hw->tqps_num, hw->intr_tqps_num,
		hw->rss_size_max, hw->alloc_rss_size, hw->tx_qnum_per_tc,
		hw->min_tx_pkt_len, hw->intr.mapping_mode, hw->vlan_mode,
		hw->tso_mode, hw->max_non_tso_bd_num,
		hw->max_tm_rate,
		hw->set_link_down ? "Yes" : "No",
		get_io_func_hint_name(hns->rx_func_hint),
		get_io_func_hint_name(hns->tx_func_hint),
		!!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC),
		dev->data->dev_conf.intr_conf.lsc,
		dev->data->dev_conf.intr_conf.rxq);
}

/*
 * Note: caller must make sure queue_id < nb_queues
 *       nb_queues = RTE_MAX(eth_dev->data->nb_rx_queues,
 *                           eth_dev->data->nb_tx_queues)
 */
static struct hns3_rx_queue *
get_rx_queue(struct rte_eth_dev *dev, unsigned int queue_id)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	unsigned int offset;
	void **rx_queues;

	if (queue_id < dev->data->nb_rx_queues) {
		rx_queues = dev->data->rx_queues;
		offset = queue_id;
	} else {
		/*
		 * For kunpeng930, fake queue is not exist. But since the queues
		 * are usually accessd in pairs, this branch may still exist.
		 */
		if (hns3_dev_get_support(hw, INDEP_TXRX))
			return NULL;

		rx_queues = hw->fkq_data.rx_queues;
		offset = queue_id - dev->data->nb_rx_queues;
	}

	if (rx_queues != NULL && rx_queues[offset] != NULL)
		return rx_queues[offset];

	hns3_err(hw, "Detect rx_queues is NULL!\n");
	return NULL;
}

/*
 * Note: caller must make sure queue_id < nb_queues
 *       nb_queues = RTE_MAX(eth_dev->data->nb_rx_queues,
 *                           eth_dev->data->nb_tx_queues)
 */
static struct hns3_tx_queue *
get_tx_queue(struct rte_eth_dev *dev, unsigned int queue_id)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	unsigned int offset;
	void **tx_queues;

	if (queue_id < dev->data->nb_tx_queues) {
		tx_queues = dev->data->tx_queues;
		offset = queue_id;
	} else {
		/*
		 * For kunpeng930, fake queue is not exist. But since the queues
		 * are usually accessd in pairs, this branch may still exist.
		 */
		if (hns3_dev_get_support(hw, INDEP_TXRX))
			return NULL;
		tx_queues = hw->fkq_data.tx_queues;
		offset = queue_id - dev->data->nb_tx_queues;
	}

	if (tx_queues != NULL && tx_queues[offset] != NULL)
		return tx_queues[offset];

	hns3_err(hw, "Detect tx_queues is NULL!\n");
	return NULL;
}

static void
get_rxtx_fake_queue_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(hns);
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	unsigned int queue_id;

	if (dev->data->nb_rx_queues != dev->data->nb_tx_queues &&
	    !hns3_dev_get_support(hw, INDEP_TXRX)) {
		queue_id = RTE_MIN(dev->data->nb_rx_queues,
				   dev->data->nb_tx_queues);
		rxq = get_rx_queue(dev, queue_id);
		if (rxq == NULL)
			return;
		txq = get_tx_queue(dev, queue_id);
		if (txq == NULL)
			return;
		fprintf(file,
			"\t  -- first fake_queue rxtx info:\n"
			"\t       rx: port=%u nb_desc=%u free_thresh=%u\n"
			"\t       tx: port=%u nb_desc=%u\n",
			rxq->port_id, rxq->nb_rx_desc, rxq->rx_free_thresh,
			txq->port_id, txq->nb_tx_desc);
	}
}

static void
get_queue_enable_state(struct hns3_hw *hw, uint32_t *queue_state,
			uint32_t nb_queues, bool is_rxq)
{
#define STATE_SIZE (sizeof(*queue_state) * CHAR_BIT)
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
		hns3_set_bit(queue_state[i / STATE_SIZE],
				i % STATE_SIZE, state);
	}
}

static void
print_queue_state_perline(FILE *file, const uint32_t *queue_state,
			  uint32_t nb_queues, uint32_t line_num)
{
#define NUM_QUEUE_PER_LINE (sizeof(*queue_state) * CHAR_BIT)
	uint32_t qid = line_num * NUM_QUEUE_PER_LINE;
	uint32_t j;

	for (j = 0; j < NUM_QUEUE_PER_LINE; j++) {
		fprintf(file, "%1lx", hns3_get_bit(queue_state[line_num], j));

		if (qid % CHAR_BIT == CHAR_BIT - 1) {
			fprintf(file, "%s",
				j == NUM_QUEUE_PER_LINE - 1 ? "\n" : ":");
		}
		qid++;
		if (qid >= nb_queues) {
			fprintf(file, "\n");
			break;
		}
	}
}

static void
display_queue_enable_state(FILE *file, const uint32_t *queue_state,
			   uint32_t nb_queues, bool is_rxq)
{
#define NUM_QUEUE_PER_LINE (sizeof(*queue_state) * CHAR_BIT)
	uint32_t i;

	if (nb_queues == 0) {
		fprintf(file, "\t       %s queue number is 0\n",
				is_rxq ? "Rx" : "Tx");
		return;
	}

	fprintf(file, "\t       %s queue id | enable state bitMap\n",
			is_rxq ? "rx" : "tx");

	for (i = 0; i < (nb_queues - 1) / NUM_QUEUE_PER_LINE + 1; i++) {
		uint32_t line_end = (i + 1) * NUM_QUEUE_PER_LINE - 1;
		uint32_t line_start = i * NUM_QUEUE_PER_LINE;
		fprintf(file, "\t       %04u - %04u | ", line_start,
			nb_queues - 1 > line_end ? line_end : nb_queues - 1);


		print_queue_state_perline(file, queue_state, nb_queues, i);
	}
}

static void
get_rxtx_queue_enable_state(FILE *file, struct rte_eth_dev *dev)
{
#define MAX_TQP_NUM 1280
#define QUEUE_BITMAP_SIZE (MAX_TQP_NUM / 32)
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t rx_queue_state[QUEUE_BITMAP_SIZE] = {0};
	uint32_t tx_queue_state[QUEUE_BITMAP_SIZE] = {0};
	uint32_t nb_rx_queues;
	uint32_t nb_tx_queues;

	nb_rx_queues = dev->data->nb_rx_queues;
	nb_tx_queues = dev->data->nb_tx_queues;

	fprintf(file, "\t  -- enable state:\n");
	get_queue_enable_state(hw, rx_queue_state, nb_rx_queues, true);
	display_queue_enable_state(file, rx_queue_state, nb_rx_queues,
					 true);

	get_queue_enable_state(hw, tx_queue_state, nb_tx_queues, false);
	display_queue_enable_state(file, tx_queue_state, nb_tx_queues,
					 false);
}

static void
get_rxtx_queue_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_rx_queue *rxq;
	struct hns3_tx_queue *txq;
	unsigned int queue_id = 0;

	rxq = get_rx_queue(dev, queue_id);
	if (rxq == NULL)
		return;
	txq = get_tx_queue(dev, queue_id);
	if (txq == NULL)
		return;
	fprintf(file, "  - Rx/Tx Queue Info:\n");
	fprintf(file,
		"\t  -- nb_rx_queues=%u nb_tx_queues=%u, "
		"first queue rxtx info:\n"
		"\t       rx: port=%u nb_desc=%u free_thresh=%u\n"
		"\t       tx: port=%u nb_desc=%u\n"
		"\t  -- tx push: %s\n",
		dev->data->nb_rx_queues,
		dev->data->nb_tx_queues,
		rxq->port_id, rxq->nb_rx_desc, rxq->rx_free_thresh,
		txq->port_id, txq->nb_tx_desc,
		txq->tx_push_enable ? "enabled" : "disabled");

	get_rxtx_fake_queue_info(file, dev);
	get_rxtx_queue_enable_state(file, dev);
}

static int
get_vlan_filter_cfg(FILE *file, struct hns3_hw *hw)
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
get_vlan_rx_offload_cfg(FILE *file, struct hns3_hw *hw)
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
			"NIC IMP exec ret=%d desc_num=%d optcode=0x%x!",
			ret, 1, rte_le_to_cpu_16(desc.opcode));
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
parse_tx_vlan_cfg(FILE *file, struct hns3_vport_vtag_tx_cfg_cmd *req)
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
get_vlan_tx_offload_cfg(FILE *file, struct hns3_hw *hw)
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
			"NIC IMP exec ret=%d desc_num=%d optcode=0x%x!",
			ret, 1, rte_le_to_cpu_16(desc.opcode));
		return ret;
	}

	parse_tx_vlan_cfg(file, req);

	return 0;
}

static void
get_port_pvid_info(FILE *file, struct hns3_hw *hw)
{
	fprintf(file, "\t  -- pvid status: %s\n",
		hw->port_base_vlan_cfg.state ? "on" : "off");
}

static void
get_vlan_config_info(FILE *file, struct hns3_hw *hw)
{
	int ret;

	fprintf(file, "  - VLAN Config Info:\n");
	ret = get_vlan_filter_cfg(file, hw);
	if (ret < 0)
		return;

	ret = get_vlan_rx_offload_cfg(file, hw);
	if (ret < 0)
		return;

	ret = get_vlan_tx_offload_cfg(file, hw);
	if (ret < 0)
		return;

	get_port_pvid_info(file, hw);
}

static void
get_tm_conf_shaper_info(FILE *file, struct hns3_tm_conf *conf)
{
	struct hns3_shaper_profile_list *shaper_profile_list =
		&conf->shaper_profile_list;
	struct hns3_tm_shaper_profile *shaper_profile;

	if (!conf->nb_shaper_profile)
		return;

	fprintf(file, "  shaper_profile:\n");
	TAILQ_FOREACH(shaper_profile, shaper_profile_list, node) {
		fprintf(file,
			"    id=%u reference_count=%u peak_rate=%" PRIu64 "Bps\n",
			shaper_profile->shaper_profile_id,
			shaper_profile->reference_count,
			shaper_profile->profile.peak.rate);
	}
}

static void
get_tm_conf_port_node_info(FILE *file, struct hns3_tm_conf *conf)
{
	if (!conf->root)
		return;

	fprintf(file,
		"  port_node:\n"
		"    node_id=%u reference_count=%u shaper_profile_id=%d\n",
		conf->root->id, conf->root->reference_count,
		conf->root->shaper_profile ?
		(int)conf->root->shaper_profile->shaper_profile_id : -1);
}

static void
get_tm_conf_tc_node_info(FILE *file, struct hns3_tm_conf *conf)
{
	struct hns3_tm_node_list *tc_list = &conf->tc_list;
	struct hns3_tm_node *tc_node[HNS3_MAX_TC_NUM];
	struct hns3_tm_node *tm_node;
	uint32_t tidx;

	if (!conf->nb_tc_node)
		return;

	fprintf(file, "  tc_node:\n");
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
			"    id=%u TC%u reference_count=%u parent_id=%d "
			"shaper_profile_id=%d\n",
			tm_node->id, hns3_tm_calc_node_tc_no(conf, tm_node->id),
			tm_node->reference_count,
			tm_node->parent ? (int)tm_node->parent->id : -1,
			tm_node->shaper_profile ?
			(int)tm_node->shaper_profile->shaper_profile_id : -1);
	}
}

static void
get_tm_conf_queue_format_info(FILE *file, struct hns3_tm_node **queue_node,
			      uint32_t *queue_node_tc,  uint32_t nb_tx_queues)
{
#define PERLINE_QUEUES	32
#define PERLINE_STRIDE	8
#define LINE_BUF_SIZE	1024
	uint32_t i, j, line_num, start_queue, end_queue;
	char tmpbuf[LINE_BUF_SIZE] = {0};

	line_num = (nb_tx_queues + PERLINE_QUEUES - 1) / PERLINE_QUEUES;
	for (i = 0; i < line_num; i++) {
		start_queue = i * PERLINE_QUEUES;
		end_queue = (i + 1) * PERLINE_QUEUES - 1;
		if (end_queue > nb_tx_queues - 1)
			end_queue = nb_tx_queues - 1;
		fprintf(file, "    %04u - %04u | ", start_queue, end_queue);
		for (j = start_queue; j < nb_tx_queues; j++) {
			if (j >= end_queue + 1)
				break;
			if (j > start_queue && j % PERLINE_STRIDE == 0)
				fprintf(file, ":");
			fprintf(file, "%u",
				queue_node[j] ? queue_node_tc[j] :
				HNS3_MAX_TC_NUM);
		}
		fprintf(file, "%s\n", tmpbuf);
	}
}

static void
get_tm_conf_queue_node_info(FILE *file, struct hns3_tm_conf *conf,
			    uint32_t nb_tx_queues)
{
	struct hns3_tm_node_list *queue_list = &conf->queue_list;
	uint32_t nb_queue_node = conf->nb_leaf_nodes_max + 1;
	struct hns3_tm_node *queue_node[nb_queue_node];
	uint32_t queue_node_tc[nb_queue_node];
	struct hns3_tm_node *tm_node;

	if (!conf->nb_queue_node)
		return;

	fprintf(file,
		"  queue_node:\n"
		"    tx queue id | mapped tc (8 mean node not exist)\n");

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

	get_tm_conf_queue_format_info(file, queue_node, queue_node_tc,
				      nb_tx_queues);
}

static void
get_tm_conf_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_pf *pf = HNS3_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct hns3_tm_conf *conf = &pf->tm_conf;

	fprintf(file, "  - TM config info:\n");
	fprintf(file,
		"\t  -- nb_leaf_nodes_max=%u nb_nodes_max=%u\n"
		"\t  -- nb_shaper_profile=%u nb_tc_node=%u nb_queue_node=%u\n"
		"\t  -- committed=%u\n",
		conf->nb_leaf_nodes_max, conf->nb_nodes_max,
		conf->nb_shaper_profile, conf->nb_tc_node, conf->nb_queue_node,
		conf->committed);

	get_tm_conf_shaper_info(file, conf);
	get_tm_conf_port_node_info(file, conf);
	get_tm_conf_tc_node_info(file, conf);
	get_tm_conf_queue_node_info(file, conf, dev->data->nb_tx_queues);
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
is_link_fc_mode(struct hns3_adapter *hns)
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
get_link_fc_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_fc_conf fc_conf;
	bool rx_pause1;
	bool tx_pause1;
	bool rx_pause2;
	bool tx_pause2;
	int ret;

	if (!is_link_fc_mode(hns))
		return;

	ret = hns3_flow_ctrl_get(dev, &fc_conf);
	if (ret)  {
		fprintf(file, "get device flow control info fail!\n");
		return;
	}

	hns3_fc_mode_to_rxtx_pause(hw->requested_fc_mode,
				   &rx_pause1, &tx_pause1);
	hns3_fc_mode_to_rxtx_pause((enum hns3_fc_mode)fc_conf.mode,
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
		fc_conf.autoneg == RTE_ETH_LINK_AUTONEG ? "On" : "Off",
		fc_conf.pause_time);
}

static void
get_flow_ctrl_info(FILE *file, struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	fprintf(file, "  - Flow Ctrl Info:\n");
	fprintf(file,
		"\t  -- fc_common_info:\n"
		"\t       current_fc_status=%u\n"
		"\t       requested_fc_mode=%u\n",
		hw->current_fc_status,
		hw->requested_fc_mode);

	get_link_fc_info(file, dev);
}

int
hns3_eth_dev_priv_dump(struct rte_eth_dev *dev, FILE *file)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	get_device_basic_info(file, dev);
	get_dev_feature_capability(file, hw);

	/* VF only supports dumping basic info and feaure capability */
	if (hns->is_vf)
		return 0;

	get_dev_mac_info(file, hns);
	get_rxtx_queue_info(file, dev);
	get_vlan_config_info(file, hw);
	get_fdir_basic_info(file, &hns->pf);
	get_tm_conf_info(file, dev);
	get_flow_ctrl_info(file, dev);

	return 0;
}
