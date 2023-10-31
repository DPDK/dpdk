/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <ethdev_driver.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_io.h>
#include <rte_malloc.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"

#define HNS3_VLAN_TAG_TYPE_NONE		0
#define HNS3_VLAN_TAG_TYPE_TAG2		1
#define HNS3_VLAN_TAG_TYPE_TAG1		2
#define HNS3_VLAN_TAG_TYPE_TAG1_2	3

#define HNS3_PF_ID_S			0
#define HNS3_PF_ID_M			GENMASK(2, 0)
#define HNS3_VF_ID_S			3
#define HNS3_VF_ID_M			GENMASK(10, 3)
#define HNS3_PORT_TYPE_B		11
#define HNS3_NETWORK_PORT_ID_S		0
#define HNS3_NETWORK_PORT_ID_M		GENMASK(3, 0)

#define HNS3_FD_EPORT_SW_EN_B		0

#define HNS3_FD_AD_DATA_S		32
#define HNS3_FD_AD_DROP_B		0
#define HNS3_FD_AD_DIRECT_QID_B		1
#define HNS3_FD_AD_QID_S		2
#define HNS3_FD_AD_QID_M		GENMASK(11, 2)
#define HNS3_FD_AD_USE_COUNTER_B	12
#define HNS3_FD_AD_COUNTER_NUM_S	13
#define HNS3_FD_AD_COUNTER_NUM_M	GENMASK(19, 13)
#define HNS3_FD_AD_NXT_STEP_B		20
#define HNS3_FD_AD_NXT_KEY_S		21
#define HNS3_FD_AD_NXT_KEY_M		GENMASK(25, 21)
#define HNS3_FD_AD_WR_RULE_ID_B		0
#define HNS3_FD_AD_RULE_ID_S		1
#define HNS3_FD_AD_RULE_ID_M		GENMASK(12, 1)
#define HNS3_FD_AD_QUEUE_REGION_EN_B	16
#define HNS3_FD_AD_QUEUE_REGION_SIZE_S	17
#define HNS3_FD_AD_QUEUE_REGION_SIZE_M	GENMASK(20, 17)
#define HNS3_FD_AD_COUNTER_HIGH_BIT	7
#define HNS3_FD_AD_COUNTER_HIGH_BIT_B	26
#define HNS3_FD_AD_QUEUE_ID_HIGH_BIT	10
#define HNS3_FD_AD_QUEUE_ID_HIGH_BIT_B	21

enum HNS3_PORT_TYPE {
	HOST_PORT,
	NETWORK_PORT
};

enum HNS3_FD_MODE {
	HNS3_FD_MODE_DEPTH_2K_WIDTH_400B_STAGE_1,
	HNS3_FD_MODE_DEPTH_1K_WIDTH_400B_STAGE_2,
	HNS3_FD_MODE_DEPTH_4K_WIDTH_200B_STAGE_1,
	HNS3_FD_MODE_DEPTH_2K_WIDTH_200B_STAGE_2,
};

enum HNS3_FD_KEY_TYPE {
	HNS3_FD_KEY_BASE_ON_PTYPE,
	HNS3_FD_KEY_BASE_ON_TUPLE,
};

enum HNS3_FD_META_DATA {
	PACKET_TYPE_ID,
	IP_FRAGEMENT,
	ROCE_TYPE,
	NEXT_KEY,
	VLAN_NUMBER,
	SRC_VPORT,
	DST_VPORT,
	TUNNEL_PACKET,
	MAX_META_DATA,
};

struct key_info {
	uint8_t key_type;
	uint8_t key_length;
};

static const struct key_info meta_data_key_info[] = {
	{PACKET_TYPE_ID, 6},
	{IP_FRAGEMENT, 1},
	{ROCE_TYPE, 1},
	{NEXT_KEY, 5},
	{VLAN_NUMBER, 2},
	{SRC_VPORT, 12},
	{DST_VPORT, 12},
	{TUNNEL_PACKET, 1},
};

static const struct key_info tuple_key_info[] = {
	{OUTER_DST_MAC, 48},
	{OUTER_SRC_MAC, 48},
	{OUTER_VLAN_TAG_FST, 16},
	{OUTER_VLAN_TAG_SEC, 16},
	{OUTER_ETH_TYPE, 16},
	{OUTER_L2_RSV, 16},
	{OUTER_IP_TOS, 8},
	{OUTER_IP_PROTO, 8},
	{OUTER_SRC_IP, 32},
	{OUTER_DST_IP, 32},
	{OUTER_L3_RSV, 16},
	{OUTER_SRC_PORT, 16},
	{OUTER_DST_PORT, 16},
	{OUTER_L4_RSV, 32},
	{OUTER_TUN_VNI, 24},
	{OUTER_TUN_FLOW_ID, 8},
	{INNER_DST_MAC, 48},
	{INNER_SRC_MAC, 48},
	{INNER_VLAN_TAG1, 16},
	{INNER_VLAN_TAG2, 16},
	{INNER_ETH_TYPE, 16},
	{INNER_L2_RSV, 16},
	{INNER_IP_TOS, 8},
	{INNER_IP_PROTO, 8},
	{INNER_SRC_IP, 32},
	{INNER_DST_IP, 32},
	{INNER_L3_RSV, 16},
	{INNER_SRC_PORT, 16},
	{INNER_DST_PORT, 16},
	{INNER_SCTP_TAG, 32},
};

#define MAX_KEY_LENGTH		400
#define MAX_200B_KEY_LENGTH	200
#define MAX_META_DATA_LENGTH	16
#define MAX_KEY_DWORDS	DIV_ROUND_UP(MAX_KEY_LENGTH / HNS3_BITS_PER_BYTE, 4)
#define MAX_KEY_BYTES	(MAX_KEY_DWORDS * 4)

enum HNS3_FD_PACKET_TYPE {
	NIC_PACKET,
	ROCE_PACKET,
};

/* For each bit of TCAM entry, it uses a pair of 'x' and
 * 'y' to indicate which value to match, like below:
 * ----------------------------------
 * | bit x | bit y |  search value  |
 * ----------------------------------
 * |   0   |   0   |   always hit   |
 * ----------------------------------
 * |   1   |   0   |   match '0'    |
 * ----------------------------------
 * |   0   |   1   |   match '1'    |
 * ----------------------------------
 * |   1   |   1   |   invalid      |
 * ----------------------------------
 * Then for input key(k) and mask(v), we can calculate the value by
 * the formulae:
 *	x = (~k) & v
 *	y = k & v
 */
#define calc_x(x, k, v) ((x) = (~(k) & (v)))
#define calc_y(y, k, v) ((y) = ((k) & (v)))

struct hns3_fd_tcam_config_1_cmd {
	uint8_t stage;
	uint8_t xy_sel;
	uint8_t port_info;
	uint8_t rsv1[1];
	rte_le32_t index;
	uint8_t entry_vld;
	uint8_t rsv2[7];
	uint8_t tcam_data[8];
};

struct hns3_fd_tcam_config_2_cmd {
	uint8_t tcam_data[24];
};

struct hns3_fd_tcam_config_3_cmd {
	uint8_t tcam_data[20];
	uint8_t rsv[4];
};

struct hns3_get_fd_mode_cmd {
	uint8_t mode;
	uint8_t enable;
	uint8_t rsv[22];
};

struct hns3_get_fd_allocation_cmd {
	rte_le32_t stage1_entry_num;
	rte_le32_t stage2_entry_num;
	rte_le16_t stage1_counter_num;
	rte_le16_t stage2_counter_num;
	uint8_t rsv[12];
};

struct hns3_set_fd_key_config_cmd {
	uint8_t stage;
	uint8_t key_select;
	uint8_t inner_sipv6_word_en;
	uint8_t inner_dipv6_word_en;
	uint8_t outer_sipv6_word_en;
	uint8_t outer_dipv6_word_en;
	uint8_t rsv1[2];
	rte_le32_t tuple_mask;
	rte_le32_t meta_data_mask;
	uint8_t rsv2[8];
};

struct hns3_fd_ad_config_cmd {
	uint8_t stage;
	uint8_t rsv1[3];
	rte_le32_t index;
	rte_le64_t ad_data;
	uint8_t rsv2[8];
};

struct hns3_fd_get_cnt_cmd {
	uint8_t stage;
	uint8_t rsv1[3];
	rte_le16_t index;
	uint8_t rsv2[2];
	rte_le64_t value;
	uint8_t rsv3[8];
};

static int hns3_get_fd_mode(struct hns3_hw *hw, uint8_t *fd_mode)
{
	struct hns3_get_fd_mode_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_FD_MODE_CTRL, true);

	req = (struct hns3_get_fd_mode_cmd *)desc.data;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Get fd mode fail, ret=%d", ret);
		return ret;
	}

	*fd_mode = req->mode;

	return ret;
}

static int hns3_get_fd_allocation(struct hns3_hw *hw,
				  uint32_t *stage1_entry_num,
				  uint32_t *stage2_entry_num,
				  uint16_t *stage1_counter_num,
				  uint16_t *stage2_counter_num)
{
	struct hns3_get_fd_allocation_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_FD_GET_ALLOCATION, true);

	req = (struct hns3_get_fd_allocation_cmd *)desc.data;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Query fd allocation fail, ret=%d", ret);
		return ret;
	}

	*stage1_entry_num = rte_le_to_cpu_32(req->stage1_entry_num);
	*stage2_entry_num = rte_le_to_cpu_32(req->stage2_entry_num);
	*stage1_counter_num = rte_le_to_cpu_16(req->stage1_counter_num);
	*stage2_counter_num = rte_le_to_cpu_16(req->stage2_counter_num);

	return ret;
}

static int hns3_set_fd_key_config(struct hns3_adapter *hns)
{
	struct hns3_set_fd_key_config_cmd *req;
	struct hns3_fd_key_cfg *key_cfg;
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_FD_KEY_CONFIG, false);

	req = (struct hns3_set_fd_key_config_cmd *)desc.data;
	key_cfg = &pf->fdir.fd_cfg.key_cfg[HNS3_FD_STAGE_1];
	req->stage = HNS3_FD_STAGE_1;
	req->key_select = key_cfg->key_sel;
	req->inner_sipv6_word_en = key_cfg->inner_sipv6_word_en;
	req->inner_dipv6_word_en = key_cfg->inner_dipv6_word_en;
	req->outer_sipv6_word_en = key_cfg->outer_sipv6_word_en;
	req->outer_dipv6_word_en = key_cfg->outer_dipv6_word_en;
	req->tuple_mask = rte_cpu_to_le_32(~key_cfg->tuple_active);
	req->meta_data_mask = rte_cpu_to_le_32(~key_cfg->meta_data_active);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Set fd key fail, ret=%d", ret);

	return ret;
}

int hns3_init_fd_config(struct hns3_adapter *hns)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_fd_key_cfg *key_cfg;
	int ret;

	ret = hns3_get_fd_mode(hw, &pf->fdir.fd_cfg.fd_mode);
	if (ret)
		return ret;

	switch (pf->fdir.fd_cfg.fd_mode) {
	case HNS3_FD_MODE_DEPTH_2K_WIDTH_400B_STAGE_1:
		pf->fdir.fd_cfg.max_key_length = MAX_KEY_LENGTH;
		break;
	case HNS3_FD_MODE_DEPTH_4K_WIDTH_200B_STAGE_1:
		pf->fdir.fd_cfg.max_key_length = MAX_200B_KEY_LENGTH;
		hns3_warn(hw, "Unsupported tunnel filter in 4K*200Bit");
		break;
	default:
		hns3_err(hw, "Unsupported flow director mode %u",
			 pf->fdir.fd_cfg.fd_mode);
		return -EOPNOTSUPP;
	}

	key_cfg = &pf->fdir.fd_cfg.key_cfg[HNS3_FD_STAGE_1];
	key_cfg->key_sel = HNS3_FD_KEY_BASE_ON_TUPLE;
	key_cfg->inner_sipv6_word_en = IPV6_ADDR_WORD_MASK;
	key_cfg->inner_dipv6_word_en = IPV6_ADDR_WORD_MASK;
	key_cfg->outer_sipv6_word_en = 0;
	key_cfg->outer_dipv6_word_en = 0;

	key_cfg->tuple_active = BIT(INNER_VLAN_TAG1) | BIT(INNER_ETH_TYPE) |
	    BIT(INNER_IP_PROTO) | BIT(INNER_IP_TOS) |
	    BIT(INNER_SRC_IP) | BIT(INNER_DST_IP) |
	    BIT(INNER_SRC_PORT) | BIT(INNER_DST_PORT);
	hns3_dbg(hw, "fdir tuple: inner<vlan_tag1 eth_type ip_src ip_dst "
		 "ip_proto ip_tos l4_src_port l4_dst_port>");

	/* If use max 400bit key, we can support tuples for ether type */
	if (pf->fdir.fd_cfg.max_key_length == MAX_KEY_LENGTH) {
		key_cfg->tuple_active |=
		    BIT(INNER_DST_MAC) | BIT(INNER_SRC_MAC) |
		    BIT(OUTER_SRC_PORT) | BIT(INNER_SCTP_TAG) |
		    BIT(OUTER_DST_PORT) | BIT(INNER_VLAN_TAG2) |
		    BIT(OUTER_TUN_VNI) | BIT(OUTER_TUN_FLOW_ID) |
		    BIT(OUTER_ETH_TYPE) | BIT(OUTER_IP_PROTO);
		hns3_dbg(hw, "fdir tuple more: inner<dst_mac src_mac "
			 "vlan_tag2 sctp_tag> outer<eth_type ip_proto "
			 "l4_src_port l4_dst_port tun_vni tun_flow_id>");
	}

	/* roce_type is used to filter roce frames
	 * dst_vport is used to specify the rule
	 */
	key_cfg->meta_data_active = BIT(DST_VPORT) | BIT(TUNNEL_PACKET);
	if (pf->fdir.vlan_match_mode)
		key_cfg->meta_data_active |= BIT(VLAN_NUMBER);

	hns3_dbg(hw, "fdir meta data: dst_vport tunnel_packet %s",
		 (pf->fdir.vlan_match_mode == HNS3_FDIR_VLAN_STRICT_MATCH) ?
		 "vlan_number" : "");

	ret = hns3_get_fd_allocation(hw,
				     &pf->fdir.fd_cfg.rule_num[HNS3_FD_STAGE_1],
				     &pf->fdir.fd_cfg.rule_num[HNS3_FD_STAGE_2],
				     &pf->fdir.fd_cfg.cnt_num[HNS3_FD_STAGE_1],
				     &pf->fdir.fd_cfg.cnt_num[HNS3_FD_STAGE_2]);
	if (ret)
		return ret;

	hns3_dbg(hw, "fdir: stage1<rules-%u counters-%u> stage2<rules-%u counters=%u>",
		 pf->fdir.fd_cfg.rule_num[HNS3_FD_STAGE_1],
		 pf->fdir.fd_cfg.cnt_num[HNS3_FD_STAGE_1],
		 pf->fdir.fd_cfg.rule_num[HNS3_FD_STAGE_2],
		 pf->fdir.fd_cfg.cnt_num[HNS3_FD_STAGE_2]);

	return hns3_set_fd_key_config(hns);
}

static int hns3_fd_tcam_config(struct hns3_hw *hw, bool sel_x, int loc,
			       uint8_t *key, bool is_add)
{
#define	FD_TCAM_CMD_NUM 3
	struct hns3_fd_tcam_config_1_cmd *req1;
	struct hns3_fd_tcam_config_2_cmd *req2;
	struct hns3_fd_tcam_config_3_cmd *req3;
	struct hns3_cmd_desc desc[FD_TCAM_CMD_NUM];
	int len;
	int ret;

	hns3_cmd_setup_basic_desc(&desc[0], HNS3_OPC_FD_TCAM_OP, false);
	desc[0].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[1], HNS3_OPC_FD_TCAM_OP, false);
	desc[1].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	hns3_cmd_setup_basic_desc(&desc[2], HNS3_OPC_FD_TCAM_OP, false);

	req1 = (struct hns3_fd_tcam_config_1_cmd *)desc[0].data;
	req2 = (struct hns3_fd_tcam_config_2_cmd *)desc[1].data;
	req3 = (struct hns3_fd_tcam_config_3_cmd *)desc[2].data;

	req1->stage = HNS3_FD_STAGE_1;
	req1->xy_sel = sel_x ? 1 : 0;
	hns3_set_bit(req1->port_info, HNS3_FD_EPORT_SW_EN_B, 0);
	req1->index = rte_cpu_to_le_32(loc);
	req1->entry_vld = sel_x ? is_add : 0;

	if (key) {
		len = sizeof(req1->tcam_data);
		memcpy(req1->tcam_data, key, len);
		key += len;

		len = sizeof(req2->tcam_data);
		memcpy(req2->tcam_data, key, len);
		key += len;

		len = sizeof(req3->tcam_data);
		memcpy(req3->tcam_data, key, len);
	}

	ret = hns3_cmd_send(hw, desc, FD_TCAM_CMD_NUM);
	if (ret)
		hns3_err(hw, "Config tcam key fail, ret=%d loc=%d add=%d",
			 ret, loc, is_add);
	return ret;
}

static int hns3_fd_ad_config(struct hns3_hw *hw, int loc,
			     struct hns3_fd_ad_data *action)
{
	struct hns3_fd_ad_config_cmd *req;
	struct hns3_cmd_desc desc;
	uint64_t ad_data = 0;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_FD_AD_OP, false);

	req = (struct hns3_fd_ad_config_cmd *)desc.data;
	req->index = rte_cpu_to_le_32(loc);
	req->stage = HNS3_FD_STAGE_1;

	hns3_set_bit(ad_data, HNS3_FD_AD_WR_RULE_ID_B,
		     action->write_rule_id_to_bd);
	hns3_set_field(ad_data, HNS3_FD_AD_RULE_ID_M, HNS3_FD_AD_RULE_ID_S,
		       action->rule_id);
	if (action->nb_queues > 1) {
		hns3_set_bit(ad_data, HNS3_FD_AD_QUEUE_REGION_EN_B, 1);
		hns3_set_field(ad_data, HNS3_FD_AD_QUEUE_REGION_SIZE_M,
			       HNS3_FD_AD_QUEUE_REGION_SIZE_S,
			       rte_log2_u32(action->nb_queues));
	}
	/* set extend bit if counter_id is in [128 ~ 255] */
	if (action->counter_id & BIT(HNS3_FD_AD_COUNTER_HIGH_BIT))
		hns3_set_bit(ad_data, HNS3_FD_AD_COUNTER_HIGH_BIT_B, 1);
	/* set extend bit if queue id > 1024 */
	if (action->queue_id & BIT(HNS3_FD_AD_QUEUE_ID_HIGH_BIT))
		hns3_set_bit(ad_data, HNS3_FD_AD_QUEUE_ID_HIGH_BIT_B, 1);
	ad_data <<= HNS3_FD_AD_DATA_S;
	hns3_set_bit(ad_data, HNS3_FD_AD_DROP_B, action->drop_packet);
	if (action->nb_queues == 1)
		hns3_set_bit(ad_data, HNS3_FD_AD_DIRECT_QID_B, 1);
	hns3_set_field(ad_data, HNS3_FD_AD_QID_M, HNS3_FD_AD_QID_S,
		       action->queue_id);
	hns3_set_bit(ad_data, HNS3_FD_AD_USE_COUNTER_B, action->use_counter);
	hns3_set_field(ad_data, HNS3_FD_AD_COUNTER_NUM_M,
		       HNS3_FD_AD_COUNTER_NUM_S, action->counter_id);
	hns3_set_bit(ad_data, HNS3_FD_AD_NXT_STEP_B, action->use_next_stage);
	hns3_set_field(ad_data, HNS3_FD_AD_NXT_KEY_M, HNS3_FD_AD_NXT_KEY_S,
		       action->next_input_key);

	req->ad_data = rte_cpu_to_le_64(ad_data);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Config fd ad fail, ret=%d loc=%d", ret, loc);

	return ret;
}

static inline void hns3_fd_convert_mac(uint8_t *key, uint8_t *mask,
				       uint8_t *mac_x, uint8_t *mac_y)
{
	uint8_t tmp;
	int i;

	for (i = 0; i < RTE_ETHER_ADDR_LEN; i++) {
		tmp = RTE_ETHER_ADDR_LEN - 1 - i;
		calc_x(mac_x[tmp], key[i], mask[i]);
		calc_y(mac_y[tmp], key[i], mask[i]);
	}
}

static void hns3_fd_convert_int16(uint32_t tuple, struct hns3_fdir_rule *rule,
				  uint8_t *val_x, uint8_t *val_y)
{
	uint16_t tmp_x_s;
	uint16_t tmp_y_s;
	uint16_t mask;
	uint16_t key;

	switch (tuple) {
	case OUTER_SRC_PORT:
		key = rule->key_conf.spec.outer_src_port;
		mask = rule->key_conf.mask.outer_src_port;
		break;
	case OUTER_DST_PORT:
		key = rule->key_conf.spec.tunnel_type;
		mask = rule->key_conf.mask.tunnel_type;
		break;
	case OUTER_ETH_TYPE:
		key = rule->key_conf.spec.outer_ether_type;
		mask = rule->key_conf.mask.outer_ether_type;
		break;
	case INNER_SRC_PORT:
		key = rule->key_conf.spec.src_port;
		mask = rule->key_conf.mask.src_port;
		break;
	case INNER_DST_PORT:
		key = rule->key_conf.spec.dst_port;
		mask = rule->key_conf.mask.dst_port;
		break;
	case INNER_VLAN_TAG1:
		key = rule->key_conf.spec.vlan_tag1;
		mask = rule->key_conf.mask.vlan_tag1;
		break;
	case INNER_VLAN_TAG2:
		key = rule->key_conf.spec.vlan_tag2;
		mask = rule->key_conf.mask.vlan_tag2;
		break;
	default:
		/*  INNER_ETH_TYPE: */
		key = rule->key_conf.spec.ether_type;
		mask = rule->key_conf.mask.ether_type;
		break;
	}
	calc_x(tmp_x_s, key, mask);
	calc_y(tmp_y_s, key, mask);
	val_x[0] = rte_cpu_to_le_16(tmp_x_s) & 0xFF;
	val_x[1] = rte_cpu_to_le_16(tmp_x_s) >> HNS3_BITS_PER_BYTE;
	val_y[0] = rte_cpu_to_le_16(tmp_y_s) & 0xFF;
	val_y[1] = rte_cpu_to_le_16(tmp_y_s) >> HNS3_BITS_PER_BYTE;
}

static inline void hns3_fd_convert_int32(uint32_t key, uint32_t mask,
					 uint8_t *val_x, uint8_t *val_y)
{
	uint32_t tmp_x_l;
	uint32_t tmp_y_l;

	calc_x(tmp_x_l, key, mask);
	calc_y(tmp_y_l, key, mask);
	memcpy(val_x, &tmp_x_l, sizeof(tmp_x_l));
	memcpy(val_y, &tmp_y_l, sizeof(tmp_y_l));
}

static bool hns3_fd_convert_tuple(struct hns3_hw *hw,
				  uint32_t tuple, uint8_t *key_x,
				  uint8_t *key_y, struct hns3_fdir_rule *rule)
{
	struct hns3_fdir_key_conf *key_conf;
	int tmp;
	int i;

	if ((rule->input_set & BIT(tuple)) == 0)
		return true;

	key_conf = &rule->key_conf;
	switch (tuple) {
	case INNER_DST_MAC:
		hns3_fd_convert_mac(key_conf->spec.dst_mac,
				    key_conf->mask.dst_mac, key_x, key_y);
		break;
	case INNER_SRC_MAC:
		hns3_fd_convert_mac(key_conf->spec.src_mac,
				    key_conf->mask.src_mac, key_x, key_y);
		break;
	case OUTER_SRC_PORT:
	case OUTER_DST_PORT:
	case OUTER_ETH_TYPE:
	case INNER_SRC_PORT:
	case INNER_DST_PORT:
	case INNER_VLAN_TAG1:
	case INNER_VLAN_TAG2:
	case INNER_ETH_TYPE:
		hns3_fd_convert_int16(tuple, rule, key_x, key_y);
		break;
	case INNER_SRC_IP:
		hns3_fd_convert_int32(key_conf->spec.src_ip[IP_ADDR_KEY_ID],
				      key_conf->mask.src_ip[IP_ADDR_KEY_ID],
				      key_x, key_y);
		break;
	case INNER_DST_IP:
		hns3_fd_convert_int32(key_conf->spec.dst_ip[IP_ADDR_KEY_ID],
				      key_conf->mask.dst_ip[IP_ADDR_KEY_ID],
				      key_x, key_y);
		break;
	case INNER_SCTP_TAG:
		hns3_fd_convert_int32(key_conf->spec.sctp_tag,
				      key_conf->mask.sctp_tag, key_x, key_y);
		break;
	case OUTER_TUN_VNI:
		for (i = 0; i < VNI_OR_TNI_LEN; i++) {
			tmp = VNI_OR_TNI_LEN - 1 - i;
			calc_x(key_x[tmp],
			       key_conf->spec.outer_tun_vni[i],
			       key_conf->mask.outer_tun_vni[i]);
			calc_y(key_y[tmp],
			       key_conf->spec.outer_tun_vni[i],
			       key_conf->mask.outer_tun_vni[i]);
		}
		break;
	case OUTER_TUN_FLOW_ID:
		calc_x(*key_x, key_conf->spec.outer_tun_flow_id,
		       key_conf->mask.outer_tun_flow_id);
		calc_y(*key_y, key_conf->spec.outer_tun_flow_id,
		       key_conf->mask.outer_tun_flow_id);
		break;
	case INNER_IP_TOS:
		calc_x(*key_x, key_conf->spec.ip_tos, key_conf->mask.ip_tos);
		calc_y(*key_y, key_conf->spec.ip_tos, key_conf->mask.ip_tos);
		break;
	case OUTER_IP_PROTO:
		calc_x(*key_x, key_conf->spec.outer_proto,
		       key_conf->mask.outer_proto);
		calc_y(*key_y, key_conf->spec.outer_proto,
		       key_conf->mask.outer_proto);
		break;
	case INNER_IP_PROTO:
		calc_x(*key_x, key_conf->spec.ip_proto,
		       key_conf->mask.ip_proto);
		calc_y(*key_y, key_conf->spec.ip_proto,
		       key_conf->mask.ip_proto);
		break;
	default:
		hns3_warn(hw, "not support tuple of (%u)", tuple);
		return false;
	}
	return true;
}

static uint32_t hns3_get_port_number(uint8_t pf_id, uint8_t vf_id)
{
	uint32_t port_number = 0;

	hns3_set_field(port_number, HNS3_PF_ID_M, HNS3_PF_ID_S, pf_id);
	hns3_set_field(port_number, HNS3_VF_ID_M, HNS3_VF_ID_S, vf_id);
	hns3_set_bit(port_number, HNS3_PORT_TYPE_B, HOST_PORT);

	return port_number;
}

static void hns3_fd_convert_meta_data(struct hns3_fd_key_cfg *cfg,
				      uint8_t vf_id,
				      struct hns3_fdir_rule *rule,
				      uint8_t *key_x, uint8_t *key_y)
{
	uint16_t meta_data = 0;
	uint32_t port_number;
	uint8_t cur_pos = 0;
	uint8_t tuple_size;
	uint8_t shift_bits;
	uint32_t tmp_x;
	uint32_t tmp_y;
	uint8_t i;

	for (i = 0; i < MAX_META_DATA; i++) {
		if ((cfg->meta_data_active & BIT(i)) == 0)
			continue;

		tuple_size = meta_data_key_info[i].key_length;
		if (i == TUNNEL_PACKET) {
			hns3_set_bit(meta_data, cur_pos,
				     rule->key_conf.spec.tunnel_type ? 1 : 0);
			cur_pos += tuple_size;
		} else if (i == VLAN_NUMBER) {
			uint32_t vlan_tag;
			uint8_t vlan_num;

			if (rule->key_conf.spec.tunnel_type == 0)
				vlan_num = rule->key_conf.vlan_num;
			else
				vlan_num = rule->key_conf.outer_vlan_num;
			if (vlan_num == 1)
				vlan_tag = HNS3_VLAN_TAG_TYPE_TAG1;
			else if (vlan_num == VLAN_TAG_NUM_MAX)
				vlan_tag = HNS3_VLAN_TAG_TYPE_TAG1_2;
			else
				vlan_tag = HNS3_VLAN_TAG_TYPE_NONE;
			hns3_set_field(meta_data,
				       GENMASK(cur_pos + tuple_size,
					       cur_pos), cur_pos, vlan_tag);
			cur_pos += tuple_size;
		} else if (i == DST_VPORT) {
			port_number = hns3_get_port_number(0, vf_id);
			hns3_set_field(meta_data,
				       GENMASK(cur_pos + tuple_size, cur_pos),
				       cur_pos, port_number);
			cur_pos += tuple_size;
		}
	}

	calc_x(tmp_x, meta_data, 0xFFFF);
	calc_y(tmp_y, meta_data, 0xFFFF);
	shift_bits = sizeof(meta_data) * HNS3_BITS_PER_BYTE - cur_pos;

	tmp_x = rte_cpu_to_le_32(tmp_x << shift_bits);
	tmp_y = rte_cpu_to_le_32(tmp_y << shift_bits);
	key_x[0] = tmp_x & 0xFF;
	key_x[1] = (tmp_x >> HNS3_BITS_PER_BYTE) & 0xFF;
	key_y[0] = tmp_y & 0xFF;
	key_y[1] = (tmp_y >> HNS3_BITS_PER_BYTE) & 0xFF;
}

/* A complete key is combined with meta data key and tuple key.
 * Meta data key is stored at the MSB region, and tuple key is stored at
 * the LSB region, unused bits will be filled 0.
 */
static int hns3_config_key(struct hns3_adapter *hns,
			   struct hns3_fdir_rule *rule)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_fd_key_cfg *key_cfg;
	uint8_t *cur_key_x;
	uint8_t *cur_key_y;
	uint8_t key_x[MAX_KEY_BYTES] __rte_aligned(4);
	uint8_t key_y[MAX_KEY_BYTES] __rte_aligned(4);
	uint8_t vf_id = rule->vf_id;
	uint8_t meta_data_region;
	uint8_t tuple_size;
	uint8_t i;
	int ret;

	memset(key_x, 0, sizeof(key_x));
	memset(key_y, 0, sizeof(key_y));
	cur_key_x = key_x;
	cur_key_y = key_y;

	key_cfg = &pf->fdir.fd_cfg.key_cfg[HNS3_FD_STAGE_1];
	for (i = 0; i < MAX_TUPLE; i++) {
		bool tuple_valid;

		tuple_size = tuple_key_info[i].key_length / HNS3_BITS_PER_BYTE;
		if (key_cfg->tuple_active & BIT(i)) {
			tuple_valid = hns3_fd_convert_tuple(hw, i, cur_key_x,
							    cur_key_y, rule);
			if (tuple_valid) {
				cur_key_x += tuple_size;
				cur_key_y += tuple_size;
			}
		}
	}

	meta_data_region = pf->fdir.fd_cfg.max_key_length / HNS3_BITS_PER_BYTE -
	    MAX_META_DATA_LENGTH / HNS3_BITS_PER_BYTE;

	hns3_fd_convert_meta_data(key_cfg, vf_id, rule,
				  key_x + meta_data_region,
				  key_y + meta_data_region);

	ret = hns3_fd_tcam_config(hw, false, rule->location, key_y, true);
	if (ret) {
		hns3_err(hw, "Config fd key_y fail, loc=%u, ret=%d",
			 rule->queue_id, ret);
		return ret;
	}

	ret = hns3_fd_tcam_config(hw, true, rule->location, key_x, true);
	if (ret)
		hns3_err(hw, "Config fd key_x fail, loc=%u, ret=%d",
			 rule->queue_id, ret);
	return ret;
}

static int hns3_config_action(struct hns3_hw *hw, struct hns3_fdir_rule *rule)
{
	struct hns3_fd_ad_data ad_data;

	ad_data.ad_id = rule->location;

	if (rule->action == HNS3_FD_ACTION_DROP_PACKET) {
		ad_data.drop_packet = true;
		ad_data.queue_id = 0;
		ad_data.nb_queues = 0;
	} else {
		ad_data.drop_packet = false;
		ad_data.queue_id = rule->queue_id;
		ad_data.nb_queues = rule->nb_queues;
	}

	if (unlikely(rule->flags & HNS3_RULE_FLAG_COUNTER)) {
		ad_data.use_counter = true;
		ad_data.counter_id = rule->act_cnt.id;
	} else {
		ad_data.use_counter = false;
		ad_data.counter_id = 0;
	}

	if (unlikely(rule->flags & HNS3_RULE_FLAG_FDID))
		ad_data.rule_id = rule->fd_id;
	else
		ad_data.rule_id = rule->location;

	ad_data.use_next_stage = false;
	ad_data.next_input_key = 0;

	ad_data.write_rule_id_to_bd = true;

	return hns3_fd_ad_config(hw, ad_data.ad_id, &ad_data);
}

static int hns3_fd_clear_all_rules(struct hns3_hw *hw, uint32_t rule_num)
{
	uint32_t i;
	int ret;

	for (i = 0; i < rule_num; i++) {
		ret = hns3_fd_tcam_config(hw, true, i, NULL, false);
		if (ret)
			return ret;
	}

	return 0;
}

int hns3_fdir_filter_init(struct hns3_adapter *hns)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_fdir_info *fdir_info = &pf->fdir;
	uint32_t rule_num = fdir_info->fd_cfg.rule_num[HNS3_FD_STAGE_1];
	char fdir_hash_name[RTE_HASH_NAMESIZE];
	struct rte_hash_parameters fdir_hash_params = {
		.name = fdir_hash_name,
		.entries = rule_num,
		.key_len = sizeof(struct hns3_fdir_key_conf),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
	};
	int ret;

	ret = hns3_fd_clear_all_rules(&hns->hw, rule_num);
	if (ret) {
		PMD_INIT_LOG(ERR, "Clear all fd rules fail! ret = %d", ret);
		return ret;
	}

	fdir_hash_params.socket_id = rte_socket_id();
	TAILQ_INIT(&fdir_info->fdir_list);
	snprintf(fdir_hash_name, RTE_HASH_NAMESIZE, "%s", hns->hw.data->name);
	fdir_info->hash_handle = rte_hash_create(&fdir_hash_params);
	if (fdir_info->hash_handle == NULL) {
		PMD_INIT_LOG(ERR, "Create FDIR hash handle fail!");
		return -EINVAL;
	}
	fdir_info->hash_map = rte_zmalloc("hns3 FDIR hash",
					  rule_num *
					  sizeof(struct hns3_fdir_rule_ele *),
					  0);
	if (fdir_info->hash_map == NULL) {
		PMD_INIT_LOG(ERR, "Allocate memory for FDIR hash map fail!");
		rte_hash_free(fdir_info->hash_handle);
		return -ENOMEM;
	}

	return 0;
}

void hns3_fdir_filter_uninit(struct hns3_adapter *hns)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_fdir_info *fdir_info = &pf->fdir;
	struct hns3_fdir_rule_ele *fdir_filter;

	if (fdir_info->hash_map) {
		rte_free(fdir_info->hash_map);
		fdir_info->hash_map = NULL;
	}
	if (fdir_info->hash_handle) {
		rte_hash_free(fdir_info->hash_handle);
		fdir_info->hash_handle = NULL;
	}

	fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list);
	while (fdir_filter) {
		TAILQ_REMOVE(&fdir_info->fdir_list, fdir_filter, entries);
		hns3_fd_tcam_config(&hns->hw, true,
				    fdir_filter->fdir_conf.location, NULL,
				    false);
		rte_free(fdir_filter);
		fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list);
	}
}

/*
 * Find a key in the hash table.
 * @return
 *   - Zero and positive values are key location.
 *   - -EINVAL if the parameters are invalid.
 *   - -ENOENT if the key is not found.
 */
static int hns3_fdir_filter_lookup(struct hns3_fdir_info *fdir_info,
				    struct hns3_fdir_key_conf *key)
{
	hash_sig_t sig;
	int ret;

	sig = rte_hash_crc(key, sizeof(*key), 0);
	ret = rte_hash_lookup_with_hash(fdir_info->hash_handle, key, sig);

	return ret;
}

static int hns3_insert_fdir_filter(struct hns3_hw *hw,
				   struct hns3_fdir_info *fdir_info,
				   struct hns3_fdir_rule_ele *fdir_filter)
{
	struct hns3_fdir_key_conf *key;
	hash_sig_t sig;
	int ret;

	key = &fdir_filter->fdir_conf.key_conf;
	sig = rte_hash_crc(key, sizeof(*key), 0);
	ret = rte_hash_add_key_with_hash(fdir_info->hash_handle, key, sig);
	if (ret < 0) {
		hns3_err(hw, "Hash table full? err:%d!", ret);
		return ret;
	}

	fdir_info->hash_map[ret] = fdir_filter;
	TAILQ_INSERT_TAIL(&fdir_info->fdir_list, fdir_filter, entries);

	return ret;
}

static int hns3_remove_fdir_filter(struct hns3_hw *hw,
				   struct hns3_fdir_info *fdir_info,
				   struct hns3_fdir_key_conf *key)
{
	struct hns3_fdir_rule_ele *fdir_filter;
	hash_sig_t sig;
	int ret;

	sig = rte_hash_crc(key, sizeof(*key), 0);
	ret = rte_hash_del_key_with_hash(fdir_info->hash_handle, key, sig);
	if (ret < 0) {
		hns3_err(hw, "Delete hash key fail ret=%d", ret);
		return ret;
	}

	fdir_filter = fdir_info->hash_map[ret];
	fdir_info->hash_map[ret] = NULL;
	TAILQ_REMOVE(&fdir_info->fdir_list, fdir_filter, entries);

	rte_free(fdir_filter);

	return 0;
}

int hns3_fdir_filter_program(struct hns3_adapter *hns,
			     struct hns3_fdir_rule *rule, bool del)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_fdir_info *fdir_info = &pf->fdir;
	struct hns3_fdir_rule_ele *node;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (del) {
		ret = hns3_fd_tcam_config(hw, true, rule->location, NULL,
					  false);
		if (ret)
			hns3_err(hw, "Failed to delete fdir: %u src_ip:%x "
				 "dst_ip:%x src_port:%u dst_port:%u ret = %d",
				 rule->location,
				 rule->key_conf.spec.src_ip[IP_ADDR_KEY_ID],
				 rule->key_conf.spec.dst_ip[IP_ADDR_KEY_ID],
				 rule->key_conf.spec.src_port,
				 rule->key_conf.spec.dst_port, ret);
		else
			ret = hns3_remove_fdir_filter(hw, fdir_info, &rule->key_conf);

		return ret;
	}

	ret = hns3_fdir_filter_lookup(fdir_info, &rule->key_conf);
	if (ret >= 0) {
		hns3_err(hw, "Conflict with existing fdir loc: %d", ret);
		return -EINVAL;
	}

	node = rte_zmalloc("hns3 fdir rule", sizeof(struct hns3_fdir_rule_ele),
			   0);
	if (node == NULL) {
		hns3_err(hw, "Failed to allocate fdir_rule memory");
		return -ENOMEM;
	}

	rte_memcpy(&node->fdir_conf, rule, sizeof(struct hns3_fdir_rule));
	ret = hns3_insert_fdir_filter(hw, fdir_info, node);
	if (ret < 0) {
		rte_free(node);
		return ret;
	}
	rule->location = ret;
	node->fdir_conf.location = ret;

	ret = hns3_config_action(hw, rule);
	if (!ret)
		ret = hns3_config_key(hns, rule);
	if (ret) {
		hns3_err(hw, "Failed to config fdir: %u src_ip:%x dst_ip:%x "
			 "src_port:%u dst_port:%u ret = %d",
			 rule->location,
			 rule->key_conf.spec.src_ip[IP_ADDR_KEY_ID],
			 rule->key_conf.spec.dst_ip[IP_ADDR_KEY_ID],
			 rule->key_conf.spec.src_port,
			 rule->key_conf.spec.dst_port, ret);
		(void)hns3_remove_fdir_filter(hw, fdir_info, &rule->key_conf);
	}

	return ret;
}

/* remove all the flow director filters */
int hns3_clear_all_fdir_filter(struct hns3_adapter *hns)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_fdir_info *fdir_info = &pf->fdir;
	struct hns3_fdir_rule_ele *fdir_filter;
	struct hns3_hw *hw = &hns->hw;
	int succ_cnt = 0;
	int fail_cnt = 0;
	int ret = 0;

	/* flush flow director */
	rte_hash_reset(fdir_info->hash_handle);

	memset(fdir_info->hash_map, 0,
	       sizeof(struct hns3_fdir_rule_ele *) *
	       fdir_info->fd_cfg.rule_num[HNS3_FD_STAGE_1]);

	fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list);
	while (fdir_filter) {
		TAILQ_REMOVE(&fdir_info->fdir_list, fdir_filter, entries);
		ret = hns3_fd_tcam_config(hw, true,
					  fdir_filter->fdir_conf.location,
					  NULL, false);
		if (ret == 0)
			succ_cnt++;
		else
			fail_cnt++;
		rte_free(fdir_filter);
		fdir_filter = TAILQ_FIRST(&fdir_info->fdir_list);
	}

	if (fail_cnt > 0) {
		hns3_err(hw, "fail to delete all FDIR filter, success num = %d "
			 "fail num = %d", succ_cnt, fail_cnt);
		ret = -EIO;
	}

	return ret;
}

int hns3_restore_all_fdir_filter(struct hns3_adapter *hns)
{
	struct hns3_pf *pf = &hns->pf;
	struct hns3_fdir_info *fdir_info = &pf->fdir;
	struct hns3_fdir_rule_ele *fdir_filter;
	struct hns3_hw *hw = &hns->hw;
	bool err = false;
	int ret;

	if (hns->is_vf)
		return 0;

	/*
	 * This API is called in the reset recovery process, the parent function
	 * must hold hw->lock.
	 * There maybe deadlock if acquire hw->flows_lock directly because rte
	 * flow driver ops first acquire hw->flows_lock and then may acquire
	 * hw->lock.
	 * So here first release the hw->lock and then acquire the
	 * hw->flows_lock to avoid deadlock.
	 */
	rte_spinlock_unlock(&hw->lock);
	pthread_mutex_lock(&hw->flows_lock);
	TAILQ_FOREACH(fdir_filter, &fdir_info->fdir_list, entries) {
		ret = hns3_config_action(hw, &fdir_filter->fdir_conf);
		if (!ret)
			ret = hns3_config_key(hns, &fdir_filter->fdir_conf);
		if (ret) {
			err = true;
			if (ret == -EBUSY)
				break;
		}
	}
	pthread_mutex_unlock(&hw->flows_lock);
	rte_spinlock_lock(&hw->lock);

	if (err) {
		hns3_err(hw, "Fail to restore FDIR filter, ret = %d", ret);
		return -EIO;
	}
	return 0;
}

int hns3_fd_get_count(struct hns3_hw *hw, uint32_t id, uint64_t *value)
{
	struct hns3_fd_get_cnt_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_FD_COUNTER_OP, true);

	req = (struct hns3_fd_get_cnt_cmd *)desc.data;
	req->stage = HNS3_FD_STAGE_1;
	req->index = rte_cpu_to_le_32(id);

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "Read counter fail, ret=%d", ret);
		return ret;
	}

	*value = req->value;

	return ret;
}
