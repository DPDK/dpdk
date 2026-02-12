/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2022 NXP
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/mman.h>

#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_flow_driver.h>
#include <rte_tailq.h>

#include <fsl_dpni.h>
#include <fsl_dpkg.h>

#include <dpaa2_ethdev.h>
#include <dpaa2_pmd_logs.h>

static char *dpaa2_flow_control_log;
static uint16_t dpaa2_flow_miss_flow_id; /* Default miss flow id is 0. */
static int dpaa2_sp_loaded = -1;

enum dpaa2_flow_entry_size {
	DPAA2_FLOW_ENTRY_MIN_SIZE = (DPNI_MAX_KEY_SIZE / 2),
	DPAA2_FLOW_ENTRY_MAX_SIZE = DPNI_MAX_KEY_SIZE
};

enum dpaa2_flow_dist_type {
	DPAA2_FLOW_QOS_TYPE = 1 << 0,
	DPAA2_FLOW_FS_TYPE = 1 << 1
};

#define DPAA2_FLOW_RAW_OFFSET_FIELD_SHIFT	16
#define DPAA2_FLOW_MAX_KEY_SIZE			16
#define DPAA2_PROT_FIELD_STRING_SIZE		16
#define VXLAN_HF_VNI 0x08

struct dpaa2_dev_flow {
	LIST_ENTRY(dpaa2_dev_flow) next;
	struct dpni_rule_cfg qos_rule;
	uint8_t *qos_key_addr;
	uint8_t *qos_mask_addr;
	uint16_t qos_rule_size;
	struct dpni_rule_cfg fs_rule;
	uint8_t qos_real_key_size;
	uint8_t fs_real_key_size;
	uint8_t *fs_key_addr;
	uint8_t *fs_mask_addr;
	uint16_t fs_rule_size;
	uint8_t tc_id; /** Traffic Class ID. */
	uint8_t tc_index; /** index within this Traffic Class. */
	enum rte_flow_action_type action_type;
	struct dpni_fs_action_cfg fs_action_cfg;
};

struct rte_dpaa2_flow_item {
	struct rte_flow_item generic_item;
	int in_tunnel;
};

static const
enum rte_flow_item_type dpaa2_hp_supported_pattern_type[] = {
	RTE_FLOW_ITEM_TYPE_END,
	RTE_FLOW_ITEM_TYPE_ETH,
	RTE_FLOW_ITEM_TYPE_VLAN,
	RTE_FLOW_ITEM_TYPE_IPV4,
	RTE_FLOW_ITEM_TYPE_IPV6,
	RTE_FLOW_ITEM_TYPE_ICMP,
	RTE_FLOW_ITEM_TYPE_UDP,
	RTE_FLOW_ITEM_TYPE_TCP,
	RTE_FLOW_ITEM_TYPE_SCTP,
	RTE_FLOW_ITEM_TYPE_GRE,
	RTE_FLOW_ITEM_TYPE_GTP,
	RTE_FLOW_ITEM_TYPE_ESP,
	RTE_FLOW_ITEM_TYPE_AH,
	RTE_FLOW_ITEM_TYPE_RAW
};

static const
enum rte_flow_item_type dpaa2_sp_supported_pattern_type[] = {
	RTE_FLOW_ITEM_TYPE_VXLAN,
	RTE_FLOW_ITEM_TYPE_ECPRI
};

static const
enum rte_flow_action_type dpaa2_supported_action_type[] = {
	RTE_FLOW_ACTION_TYPE_END,
	RTE_FLOW_ACTION_TYPE_QUEUE,
	RTE_FLOW_ACTION_TYPE_PORT_ID,
	RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT,
	RTE_FLOW_ACTION_TYPE_RSS
};

#ifndef __cplusplus
static const struct rte_flow_item_eth dpaa2_flow_item_eth_mask = {
	.hdr.dst_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	.hdr.src_addr.addr_bytes = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff },
	.hdr.ether_type = RTE_BE16(0xffff),
};

static const struct rte_flow_item_vlan dpaa2_flow_item_vlan_mask = {
	.hdr.vlan_tci = RTE_BE16(0xffff),
};

static const struct rte_flow_item_ipv4 dpaa2_flow_item_ipv4_mask = {
	.hdr.src_addr = RTE_BE32(0xffffffff),
	.hdr.dst_addr = RTE_BE32(0xffffffff),
	.hdr.next_proto_id = 0xff,
};

static const struct rte_flow_item_ipv6 dpaa2_flow_item_ipv6_mask = {
	.hdr = {
		.src_addr = RTE_IPV6_MASK_FULL,
		.dst_addr = RTE_IPV6_MASK_FULL,
		.proto = 0xff
	},
};

static const struct rte_flow_item_icmp dpaa2_flow_item_icmp_mask = {
	.hdr.icmp_type = 0xff,
	.hdr.icmp_code = 0xff,
};

static const struct rte_flow_item_udp dpaa2_flow_item_udp_mask = {
	.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};

static const struct rte_flow_item_tcp dpaa2_flow_item_tcp_mask = {
	.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};

static const struct rte_flow_item_sctp dpaa2_flow_item_sctp_mask = {
	.hdr = {
		.src_port = RTE_BE16(0xffff),
		.dst_port = RTE_BE16(0xffff),
	},
};

static const struct rte_flow_item_esp dpaa2_flow_item_esp_mask = {
	.hdr = {
		.spi = RTE_BE32(0xffffffff),
		.seq = RTE_BE32(0xffffffff),
	},
};

static const struct rte_flow_item_ah dpaa2_flow_item_ah_mask = {
	.spi = RTE_BE32(0xffffffff),
};

static const struct rte_flow_item_gre dpaa2_flow_item_gre_mask = {
	.protocol = RTE_BE16(0xffff),
};

static const struct rte_flow_item_vxlan dpaa2_flow_item_vxlan_mask = {
	.flags = 0xff,
	.vni = { 0xff, 0xff, 0xff },
};

static const struct rte_flow_item_ecpri dpaa2_flow_item_ecpri_mask = {
	.hdr.common.type = 0xff,
	.hdr.dummy[0] = RTE_BE32(0xffffffff),
	.hdr.dummy[1] = RTE_BE32(0xffffffff),
	.hdr.dummy[2] = RTE_BE32(0xffffffff),
};

static const struct rte_flow_item_gtp dpaa2_flow_item_gtp_mask = {
	.teid = RTE_BE32(0xffffffff),
};

#endif

#define DPAA2_FLOW_DUMP printf

static inline void
dpaa2_prot_field_string(uint32_t prot, uint32_t field,
	char *string)
{
	if (!dpaa2_flow_control_log)
		return;

	if (prot == NET_PROT_ETH) {
		strcpy(string, "eth");
		if (field == NH_FLD_ETH_DA)
			strcat(string, ".dst");
		else if (field == NH_FLD_ETH_SA)
			strcat(string, ".src");
		else if (field == NH_FLD_ETH_TYPE)
			strcat(string, ".type");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_VLAN) {
		strcpy(string, "vlan");
		if (field == NH_FLD_VLAN_TCI)
			strcat(string, ".tci");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_IP) {
		strcpy(string, "ip");
		if (field == NH_FLD_IP_SRC)
			strcat(string, ".src");
		else if (field == NH_FLD_IP_DST)
			strcat(string, ".dst");
		else if (field == NH_FLD_IP_PROTO)
			strcat(string, ".proto");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_TCP) {
		strcpy(string, "tcp");
		if (field == NH_FLD_TCP_PORT_SRC)
			strcat(string, ".src");
		else if (field == NH_FLD_TCP_PORT_DST)
			strcat(string, ".dst");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_UDP) {
		strcpy(string, "udp");
		if (field == NH_FLD_UDP_PORT_SRC)
			strcat(string, ".src");
		else if (field == NH_FLD_UDP_PORT_DST)
			strcat(string, ".dst");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_ICMP) {
		strcpy(string, "icmp");
		if (field == NH_FLD_ICMP_TYPE)
			strcat(string, ".type");
		else if (field == NH_FLD_ICMP_CODE)
			strcat(string, ".code");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_SCTP) {
		strcpy(string, "sctp");
		if (field == NH_FLD_SCTP_PORT_SRC)
			strcat(string, ".src");
		else if (field == NH_FLD_SCTP_PORT_DST)
			strcat(string, ".dst");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_GRE) {
		strcpy(string, "gre");
		if (field == NH_FLD_GRE_TYPE)
			strcat(string, ".type");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_GTP) {
		rte_strscpy(string, "gtp", DPAA2_PROT_FIELD_STRING_SIZE);
		if (field == NH_FLD_GTP_TEID)
			strcat(string, ".teid");
		else
			strcat(string, ".unknown field");
	} else if (prot == NET_PROT_IPSEC_ESP) {
		rte_strscpy(string, "esp", DPAA2_PROT_FIELD_STRING_SIZE);
		if (field == NH_FLD_IPSEC_ESP_SPI)
			strcat(string, ".spi");
		else if (field == NH_FLD_IPSEC_ESP_SEQUENCE_NUM)
			strcat(string, ".seq");
		else
			strcat(string, ".unknown field");
	} else {
		sprintf(string, "unknown protocol(%d)", prot);
	}
}

static inline void
dpaa2_flow_qos_extracts_log(const struct dpaa2_dev_priv *priv)
{
	int idx;
	char string[32];
	const struct dpkg_profile_cfg *dpkg =
		&priv->extract.qos_key_extract.dpkg;
	const struct dpkg_extract *extract;
	enum dpkg_extract_type type;
	enum net_prot prot;
	uint32_t field;

	if (!dpaa2_flow_control_log)
		return;

	DPAA2_FLOW_DUMP("QoS table: %d extracts\r\n",
		dpkg->num_extracts);
	for (idx = 0; idx < dpkg->num_extracts; idx++) {
		extract = &dpkg->extracts[idx];
		type = extract->type;
		if (type == DPKG_EXTRACT_FROM_HDR) {
			prot = extract->extract.from_hdr.prot;
			field = extract->extract.from_hdr.field;
			dpaa2_prot_field_string(prot, field,
				string);
		} else if (type == DPKG_EXTRACT_FROM_DATA) {
			sprintf(string, "raw offset/len: %d/%d",
				extract->extract.from_data.offset,
				extract->extract.from_data.size);
		} else if (type == DPKG_EXTRACT_FROM_PARSE) {
			sprintf(string, "parse offset/len: %d/%d",
				extract->extract.from_parse.offset,
				extract->extract.from_parse.size);
		}
		DPAA2_FLOW_DUMP("%s", string);
		if ((idx + 1) < dpkg->num_extracts)
			DPAA2_FLOW_DUMP(" / ");
	}
	DPAA2_FLOW_DUMP("\r\n");
}

static inline void
dpaa2_flow_fs_extracts_log(const struct dpaa2_dev_priv *priv,
	int tc_id)
{
	int idx;
	char string[32];
	const struct dpkg_profile_cfg *dpkg =
		&priv->extract.tc_key_extract[tc_id].dpkg;
	const struct dpkg_extract *extract;
	enum dpkg_extract_type type;
	enum net_prot prot;
	uint32_t field;

	if (!dpaa2_flow_control_log)
		return;

	DPAA2_FLOW_DUMP("FS table: %d extracts in TC[%d]\r\n",
		dpkg->num_extracts, tc_id);
	for (idx = 0; idx < dpkg->num_extracts; idx++) {
		extract = &dpkg->extracts[idx];
		type = extract->type;
		if (type == DPKG_EXTRACT_FROM_HDR) {
			prot = extract->extract.from_hdr.prot;
			field = extract->extract.from_hdr.field;
			dpaa2_prot_field_string(prot, field,
				string);
		} else if (type == DPKG_EXTRACT_FROM_DATA) {
			sprintf(string, "raw offset/len: %d/%d",
				extract->extract.from_data.offset,
				extract->extract.from_data.size);
		} else if (type == DPKG_EXTRACT_FROM_PARSE) {
			sprintf(string, "parse offset/len: %d/%d",
				extract->extract.from_parse.offset,
				extract->extract.from_parse.size);
		}
		DPAA2_FLOW_DUMP("%s", string);
		if ((idx + 1) < dpkg->num_extracts)
			DPAA2_FLOW_DUMP(" / ");
	}
	DPAA2_FLOW_DUMP("\r\n");
}

static inline void
dpaa2_flow_qos_entry_log(const char *log_info,
	const struct dpaa2_dev_flow *flow, int qos_index)
{
	int idx;
	uint8_t *key, *mask;

	if (!dpaa2_flow_control_log)
		return;

	if (qos_index >= 0) {
		DPAA2_FLOW_DUMP("%s QoS entry[%d](size %d/%d) for TC[%d]\r\n",
			log_info, qos_index, flow->qos_rule_size,
			flow->qos_rule.key_size,
			flow->tc_id);
	} else {
		DPAA2_FLOW_DUMP("%s QoS entry(size %d/%d) for TC[%d]\r\n",
			log_info, flow->qos_rule_size,
			flow->qos_rule.key_size,
			flow->tc_id);
	}

	key = flow->qos_key_addr;
	mask = flow->qos_mask_addr;

	DPAA2_FLOW_DUMP("key:\r\n");
	for (idx = 0; idx < flow->qos_rule_size; idx++)
		DPAA2_FLOW_DUMP("%02x ", key[idx]);

	DPAA2_FLOW_DUMP("\r\nmask:\r\n");
	for (idx = 0; idx < flow->qos_rule_size; idx++)
		DPAA2_FLOW_DUMP("%02x ", mask[idx]);
	DPAA2_FLOW_DUMP("\r\n");
}

static inline void
dpaa2_flow_fs_entry_log(const char *log_info,
	const struct dpaa2_dev_flow *flow)
{
	int idx;
	uint8_t *key, *mask;

	if (!dpaa2_flow_control_log)
		return;

	DPAA2_FLOW_DUMP("%s FS/TC entry[%d](size %d/%d) of TC[%d]\r\n",
		log_info, flow->tc_index,
		flow->fs_rule_size, flow->fs_rule.key_size,
		flow->tc_id);

	key = flow->fs_key_addr;
	mask = flow->fs_mask_addr;

	DPAA2_FLOW_DUMP("key:\r\n");
	for (idx = 0; idx < flow->fs_rule_size; idx++)
		DPAA2_FLOW_DUMP("%02x ", key[idx]);

	DPAA2_FLOW_DUMP("\r\nmask:\r\n");
	for (idx = 0; idx < flow->fs_rule_size; idx++)
		DPAA2_FLOW_DUMP("%02x ", mask[idx]);
	DPAA2_FLOW_DUMP("\r\n");
}

/** For LX2160A, LS2088A and LS1088A*/
#define WRIOP_CCSR_BASE 0x8b80000
#define WRIOP_CCSR_CTLU_OFFSET 0
#define WRIOP_CCSR_CTLU_PARSER_OFFSET 0
#define WRIOP_CCSR_CTLU_PARSER_INGRESS_OFFSET 0

#define WRIOP_INGRESS_PARSER_PHY \
	(WRIOP_CCSR_BASE + WRIOP_CCSR_CTLU_OFFSET + \
	WRIOP_CCSR_CTLU_PARSER_OFFSET + \
	WRIOP_CCSR_CTLU_PARSER_INGRESS_OFFSET)

struct dpaa2_parser_ccsr {
	uint32_t psr_cfg;
	uint32_t psr_idle;
	uint32_t psr_pclm;
	uint8_t psr_ver_min;
	uint8_t psr_ver_maj;
	uint8_t psr_id1_l;
	uint8_t psr_id1_h;
	uint32_t psr_rev2;
	uint8_t rsv[0x2c];
	uint8_t sp_ins[4032];
};

int
dpaa2_soft_parser_loaded(void)
{
	int fd, i, ret = 0;
	struct dpaa2_parser_ccsr *parser_ccsr = NULL;

	dpaa2_flow_control_log = getenv("DPAA2_FLOW_CONTROL_LOG");

	if (dpaa2_sp_loaded >= 0)
		return dpaa2_sp_loaded;

	fd = open("/dev/mem", O_RDWR | O_SYNC);
	if (fd < 0) {
		DPAA2_PMD_ERR("open \"/dev/mem\" ERROR(%d)", fd);
		ret = fd;
		goto exit;
	}

	parser_ccsr = mmap(NULL, sizeof(struct dpaa2_parser_ccsr),
		PROT_READ | PROT_WRITE, MAP_SHARED, fd,
		WRIOP_INGRESS_PARSER_PHY);
	if (!parser_ccsr) {
		DPAA2_PMD_ERR("Map 0x%" PRIx64 "(size=0x%x) failed",
			(uint64_t)WRIOP_INGRESS_PARSER_PHY,
			(uint32_t)sizeof(struct dpaa2_parser_ccsr));
		ret = -ENOBUFS;
		goto exit;
	}

	DPAA2_PMD_INFO("Parser ID:0x%02x%02x, Rev:major(%02x), minor(%02x)",
		parser_ccsr->psr_id1_h, parser_ccsr->psr_id1_l,
		parser_ccsr->psr_ver_maj, parser_ccsr->psr_ver_min);

	if (dpaa2_flow_control_log) {
		for (i = 0; i < 64; i++) {
			DPAA2_FLOW_DUMP("%02x ",
				parser_ccsr->sp_ins[i]);
			if (!((i + 1) % 16))
				DPAA2_FLOW_DUMP("\r\n");
		}
	}

	for (i = 0; i < 16; i++) {
		if (parser_ccsr->sp_ins[i]) {
			dpaa2_sp_loaded = 1;
			break;
		}
	}
	if (dpaa2_sp_loaded < 0)
		dpaa2_sp_loaded = 0;

	ret = dpaa2_sp_loaded;

exit:
	if (parser_ccsr)
		munmap(parser_ccsr, sizeof(struct dpaa2_parser_ccsr));
	if (fd >= 0)
		close(fd);

	return ret;
}

static int
dpaa2_flow_ip_address_extract(enum net_prot prot,
	uint32_t field)
{
	if (prot == NET_PROT_IPV4 &&
		(field == NH_FLD_IPV4_SRC_IP ||
		field == NH_FLD_IPV4_DST_IP))
		return true;
	else if (prot == NET_PROT_IPV6 &&
		(field == NH_FLD_IPV6_SRC_IP ||
		field == NH_FLD_IPV6_DST_IP))
		return true;
	else if (prot == NET_PROT_IP &&
		(field == NH_FLD_IP_SRC ||
		field == NH_FLD_IP_DST))
		return true;

	return false;
}

static int
dpaa2_flow_l4_src_port_extract(enum net_prot prot,
	uint32_t field)
{
	if (prot == NET_PROT_TCP &&
		field == NH_FLD_TCP_PORT_SRC)
		return true;
	else if (prot == NET_PROT_UDP &&
		field == NH_FLD_UDP_PORT_SRC)
		return true;
	else if (prot == NET_PROT_SCTP &&
		field == NH_FLD_SCTP_PORT_SRC)
		return true;

	return false;
}

static int
dpaa2_flow_l4_dst_port_extract(enum net_prot prot,
	uint32_t field)
{
	if (prot == NET_PROT_TCP &&
		field == NH_FLD_TCP_PORT_DST)
		return true;
	else if (prot == NET_PROT_UDP &&
		field == NH_FLD_UDP_PORT_DST)
		return true;
	else if (prot == NET_PROT_SCTP &&
		field == NH_FLD_SCTP_PORT_DST)
		return true;

	return false;
}

static int
dpaa2_flow_add_qos_rule(struct dpaa2_dev_priv *priv,
	struct dpaa2_dev_flow *flow)
{
	uint16_t qos_index;
	int ret;
	struct fsl_mc_io *dpni = priv->hw;

	if (priv->num_rx_tc <= 1 &&
		flow->action_type != RTE_FLOW_ACTION_TYPE_RSS) {
		DPAA2_PMD_WARN("No QoS Table for FS");
		return -EINVAL;
	}

	/* QoS entry added is only effective for multiple TCs.*/
	qos_index = flow->tc_id * priv->fs_entries + flow->tc_index;
	if (qos_index >= priv->qos_entries) {
		DPAA2_PMD_ERR("QoS table full(%d >= %d)",
			qos_index, priv->qos_entries);
		return -EINVAL;
	}

	dpaa2_flow_qos_entry_log("Start add", flow, qos_index);

	ret = dpni_add_qos_entry(dpni, CMD_PRI_LOW,
			priv->token, &flow->qos_rule,
			flow->tc_id, qos_index,
			0, 0);
	if (ret < 0) {
		DPAA2_PMD_ERR("Add entry(%d) to table(%d) failed",
			qos_index, flow->tc_id);
		return ret;
	}

	return 0;
}

static int
dpaa2_flow_add_fs_rule(struct dpaa2_dev_priv *priv,
	struct dpaa2_dev_flow *flow)
{
	int ret;
	struct fsl_mc_io *dpni = priv->hw;

	if (flow->tc_index >= priv->fs_entries) {
		DPAA2_PMD_ERR("FS table full(%d >= %d)",
			flow->tc_index, priv->fs_entries);
		return -EINVAL;
	}

	dpaa2_flow_fs_entry_log("Start add", flow);

	ret = dpni_add_fs_entry(dpni, CMD_PRI_LOW,
			priv->token, flow->tc_id,
			flow->tc_index, &flow->fs_rule,
			&flow->fs_action_cfg);
	if (ret < 0) {
		DPAA2_PMD_ERR("Add rule(%d) to FS table(%d) failed",
			flow->tc_index, flow->tc_id);
		return ret;
	}

	return 0;
}

static int
dpaa2_flow_rule_insert_hole(struct dpaa2_dev_flow *flow,
	int offset, int size,
	enum dpaa2_flow_dist_type dist_type)
{
	if (dist_type & DPAA2_FLOW_QOS_TYPE) {
		if (offset < flow->qos_rule_size) {
			memmove(flow->qos_key_addr + offset + size,
					flow->qos_key_addr + offset,
					flow->qos_rule_size - offset);
			memset(flow->qos_key_addr + offset,
					0, size);

			memmove(flow->qos_mask_addr + offset + size,
					flow->qos_mask_addr + offset,
					flow->qos_rule_size - offset);
			memset(flow->qos_mask_addr + offset,
					0, size);
			flow->qos_rule_size += size;
		} else {
			flow->qos_rule_size = offset + size;
		}
	}

	if (dist_type & DPAA2_FLOW_FS_TYPE) {
		if (offset < flow->fs_rule_size) {
			memmove(flow->fs_key_addr + offset + size,
					flow->fs_key_addr + offset,
					flow->fs_rule_size - offset);
			memset(flow->fs_key_addr + offset,
					0, size);

			memmove(flow->fs_mask_addr + offset + size,
					flow->fs_mask_addr + offset,
					flow->fs_rule_size - offset);
			memset(flow->fs_mask_addr + offset,
					0, size);
			flow->fs_rule_size += size;
		} else {
			flow->fs_rule_size = offset + size;
		}
	}

	return 0;
}

static int
dpaa2_flow_rule_add_all(struct dpaa2_dev_priv *priv,
	enum dpaa2_flow_dist_type dist_type,
	uint16_t entry_size, uint8_t tc_id)
{
	struct dpaa2_dev_flow *curr = LIST_FIRST(&priv->flows);
	int ret;

	while (curr) {
		if (dist_type & DPAA2_FLOW_QOS_TYPE) {
			if (priv->num_rx_tc > 1 ||
				curr->action_type ==
				RTE_FLOW_ACTION_TYPE_RSS) {
				curr->qos_rule.key_size = entry_size;
				ret = dpaa2_flow_add_qos_rule(priv, curr);
				if (ret)
					return ret;
			}
		}
		if (dist_type & DPAA2_FLOW_FS_TYPE &&
			curr->tc_id == tc_id) {
			curr->fs_rule.key_size = entry_size;
			ret = dpaa2_flow_add_fs_rule(priv, curr);
			if (ret)
				return ret;
		}
		curr = LIST_NEXT(curr, next);
	}

	return 0;
}

static int
dpaa2_flow_qos_rule_insert_hole(struct dpaa2_dev_priv *priv,
	int offset, int size)
{
	struct dpaa2_dev_flow *curr;
	int ret;

	curr = priv->curr;
	if (!curr) {
		DPAA2_PMD_ERR("Current qos flow insert hole failed.");
		return -EINVAL;
	} else {
		ret = dpaa2_flow_rule_insert_hole(curr, offset, size,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
	}

	curr = LIST_FIRST(&priv->flows);
	while (curr) {
		ret = dpaa2_flow_rule_insert_hole(curr, offset, size,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		curr = LIST_NEXT(curr, next);
	}

	return 0;
}

static int
dpaa2_flow_fs_rule_insert_hole(struct dpaa2_dev_priv *priv,
	int offset, int size, int tc_id)
{
	struct dpaa2_dev_flow *curr;
	int ret;

	curr = priv->curr;
	if (!curr || curr->tc_id != tc_id) {
		DPAA2_PMD_ERR("Current flow insert hole failed.");
		return -EINVAL;
	} else {
		ret = dpaa2_flow_rule_insert_hole(curr, offset, size,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	curr = LIST_FIRST(&priv->flows);

	while (curr) {
		if (curr->tc_id != tc_id) {
			curr = LIST_NEXT(curr, next);
			continue;
		}
		ret = dpaa2_flow_rule_insert_hole(curr, offset, size,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
		curr = LIST_NEXT(curr, next);
	}

	return 0;
}

static int
dpaa2_flow_faf_advance(struct dpaa2_dev_priv *priv,
	int faf_byte, enum dpaa2_flow_dist_type dist_type, int tc_id,
	int *insert_offset)
{
	int offset, ret;
	struct dpaa2_key_profile *key_profile;
	int num, pos;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_profile = &priv->extract.qos_key_extract.key_profile;
	else
		key_profile = &priv->extract.tc_key_extract[tc_id].key_profile;

	num = key_profile->num;

	if (num >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Number of extracts overflows");
		return -EINVAL;
	}

	if (key_profile->ip_addr_type != IP_NONE_ADDR_EXTRACT) {
		offset = key_profile->ip_addr_extract_off;
		pos = key_profile->ip_addr_extract_pos;
		key_profile->ip_addr_extract_pos++;
		key_profile->ip_addr_extract_off++;
		if (dist_type == DPAA2_FLOW_QOS_TYPE) {
			ret = dpaa2_flow_qos_rule_insert_hole(priv,
					offset, 1);
		} else {
			ret = dpaa2_flow_fs_rule_insert_hole(priv,
				offset, 1, tc_id);
		}
		if (ret)
			return ret;
	} else {
		pos = num;
	}

	if (pos > 0) {
		key_profile->key_offset[pos] =
			key_profile->key_offset[pos - 1] +
			key_profile->key_size[pos - 1];
	} else {
		key_profile->key_offset[pos] = 0;
	}

	key_profile->key_size[pos] = 1;
	key_profile->prot_field[pos].type = DPAA2_FAF_KEY;
	key_profile->prot_field[pos].key_field = faf_byte;
	key_profile->num++;

	if (insert_offset)
		*insert_offset = key_profile->key_offset[pos];

	key_profile->key_max_size++;

	return pos;
}

static int
dpaa2_flow_pr_advance(struct dpaa2_dev_priv *priv,
	uint32_t pr_offset, uint32_t pr_size,
	enum dpaa2_flow_dist_type dist_type, int tc_id,
	int *insert_offset)
{
	int offset, ret;
	struct dpaa2_key_profile *key_profile;
	int num, pos;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_profile = &priv->extract.qos_key_extract.key_profile;
	else
		key_profile = &priv->extract.tc_key_extract[tc_id].key_profile;

	num = key_profile->num;

	if (num >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Number of extracts overflows");
		return -EINVAL;
	}

	if (key_profile->ip_addr_type != IP_NONE_ADDR_EXTRACT) {
		offset = key_profile->ip_addr_extract_off;
		pos = key_profile->ip_addr_extract_pos;
		key_profile->ip_addr_extract_pos++;
		key_profile->ip_addr_extract_off += pr_size;
		if (dist_type == DPAA2_FLOW_QOS_TYPE) {
			ret = dpaa2_flow_qos_rule_insert_hole(priv,
					offset, pr_size);
		} else {
			ret = dpaa2_flow_fs_rule_insert_hole(priv,
				offset, pr_size, tc_id);
		}
		if (ret)
			return ret;
	} else {
		pos = num;
	}

	if (pos > 0) {
		key_profile->key_offset[pos] =
			key_profile->key_offset[pos - 1] +
			key_profile->key_size[pos - 1];
	} else {
		key_profile->key_offset[pos] = 0;
	}

	key_profile->key_size[pos] = pr_size;
	key_profile->prot_field[pos].type = DPAA2_PR_KEY;
	key_profile->prot_field[pos].key_field =
		(pr_offset << 16) | pr_size;
	key_profile->num++;

	if (insert_offset)
		*insert_offset = key_profile->key_offset[pos];

	key_profile->key_max_size += pr_size;

	return pos;
}

/* Move IPv4/IPv6 addresses to fill new extract previous IP address.
 * Current MC/WRIOP only support generic IP extract but IP address
 * is not fixed, so we have to put them at end of extracts, otherwise,
 * the extracts position following them can't be identified.
 */
static int
dpaa2_flow_key_profile_advance(enum net_prot prot,
	uint32_t field, uint8_t field_size,
	struct dpaa2_dev_priv *priv,
	enum dpaa2_flow_dist_type dist_type, int tc_id,
	int *insert_offset)
{
	int offset, ret;
	struct dpaa2_key_profile *key_profile;
	int num, pos;

	if (dpaa2_flow_ip_address_extract(prot, field)) {
		DPAA2_PMD_ERR("%s only for none IP address extract",
			__func__);
		return -EINVAL;
	}

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_profile = &priv->extract.qos_key_extract.key_profile;
	else
		key_profile = &priv->extract.tc_key_extract[tc_id].key_profile;

	num = key_profile->num;

	if (num >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Number of extracts overflows");
		return -EINVAL;
	}

	if (key_profile->ip_addr_type != IP_NONE_ADDR_EXTRACT) {
		offset = key_profile->ip_addr_extract_off;
		pos = key_profile->ip_addr_extract_pos;
		key_profile->ip_addr_extract_pos++;
		key_profile->ip_addr_extract_off += field_size;
		if (dist_type == DPAA2_FLOW_QOS_TYPE) {
			ret = dpaa2_flow_qos_rule_insert_hole(priv,
					offset, field_size);
		} else {
			ret = dpaa2_flow_fs_rule_insert_hole(priv,
				offset, field_size, tc_id);
		}
		if (ret)
			return ret;
	} else {
		pos = num;
	}

	if (pos > 0) {
		key_profile->key_offset[pos] =
			key_profile->key_offset[pos - 1] +
			key_profile->key_size[pos - 1];
	} else {
		key_profile->key_offset[pos] = 0;
	}

	key_profile->key_size[pos] = field_size;
	key_profile->prot_field[pos].type = DPAA2_NET_PROT_KEY;
	key_profile->prot_field[pos].prot = prot;
	key_profile->prot_field[pos].key_field = field;
	key_profile->num++;

	if (insert_offset)
		*insert_offset = key_profile->key_offset[pos];

	if (dpaa2_flow_l4_src_port_extract(prot, field)) {
		key_profile->l4_src_port_present = 1;
		key_profile->l4_src_port_pos = pos;
		key_profile->l4_src_port_offset =
			key_profile->key_offset[pos];
	} else if (dpaa2_flow_l4_dst_port_extract(prot, field)) {
		key_profile->l4_dst_port_present = 1;
		key_profile->l4_dst_port_pos = pos;
		key_profile->l4_dst_port_offset =
			key_profile->key_offset[pos];
	}
	key_profile->key_max_size += field_size;

	return pos;
}

static int
dpaa2_flow_faf_add_hdr(int faf_byte,
	struct dpaa2_dev_priv *priv,
	enum dpaa2_flow_dist_type dist_type, int tc_id,
	int *insert_offset)
{
	int pos, i, offset;
	struct dpaa2_key_extract *key_extract;
	struct dpkg_profile_cfg *dpkg;
	struct dpkg_extract *extracts;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_extract = &priv->extract.qos_key_extract;
	else
		key_extract = &priv->extract.tc_key_extract[tc_id];

	dpkg = &key_extract->dpkg;
	extracts = dpkg->extracts;

	if (dpkg->num_extracts >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Number of extracts overflows");
		return -EINVAL;
	}

	pos = dpaa2_flow_faf_advance(priv,
			faf_byte, dist_type, tc_id,
			insert_offset);
	if (pos < 0)
		return pos;

	if (pos != dpkg->num_extracts) {
		/* Not the last pos, must have IP address extract.*/
		for (i = dpkg->num_extracts - 1; i >= pos; i--) {
			extracts[i + 1] = extracts[i];
		}
	}

	offset = DPAA2_FAFE_PSR_OFFSET + faf_byte;

	extracts[pos].type = DPKG_EXTRACT_FROM_PARSE;
	extracts[pos].extract.from_parse.offset = offset;
	extracts[pos].extract.from_parse.size = 1;

	dpkg->num_extracts++;

	return 0;
}

static int
dpaa2_flow_pr_add_hdr(uint32_t pr_offset,
	uint32_t pr_size, struct dpaa2_dev_priv *priv,
	enum dpaa2_flow_dist_type dist_type, int tc_id,
	int *insert_offset)
{
	int pos, i;
	struct dpaa2_key_extract *key_extract;
	struct dpkg_profile_cfg *dpkg;
	struct dpkg_extract *extracts;

	if ((pr_offset + pr_size) > DPAA2_FAPR_SIZE) {
		DPAA2_PMD_ERR("PR extracts(%d:%d) overflow",
			pr_offset, pr_size);
		return -EINVAL;
	}

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_extract = &priv->extract.qos_key_extract;
	else
		key_extract = &priv->extract.tc_key_extract[tc_id];

	dpkg = &key_extract->dpkg;
	extracts = dpkg->extracts;

	if (dpkg->num_extracts >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Number of extracts overflows");
		return -EINVAL;
	}

	pos = dpaa2_flow_pr_advance(priv,
			pr_offset, pr_size, dist_type, tc_id,
			insert_offset);
	if (pos < 0)
		return pos;

	if (pos != dpkg->num_extracts) {
		/* Not the last pos, must have IP address extract.*/
		for (i = dpkg->num_extracts - 1; i >= pos; i--) {
			extracts[i + 1] = extracts[i];
		}
	}

	extracts[pos].type = DPKG_EXTRACT_FROM_PARSE;
	extracts[pos].extract.from_parse.offset = pr_offset;
	extracts[pos].extract.from_parse.size = pr_size;

	dpkg->num_extracts++;

	return 0;
}

static int
dpaa2_flow_extract_add_hdr(enum net_prot prot,
	uint32_t field, uint8_t field_size,
	struct dpaa2_dev_priv *priv,
	enum dpaa2_flow_dist_type dist_type, int tc_id,
	int *insert_offset)
{
	int pos, i;
	struct dpaa2_key_extract *key_extract;
	struct dpkg_profile_cfg *dpkg;
	struct dpkg_extract *extracts;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_extract = &priv->extract.qos_key_extract;
	else
		key_extract = &priv->extract.tc_key_extract[tc_id];

	dpkg = &key_extract->dpkg;
	extracts = dpkg->extracts;

	if (dpaa2_flow_ip_address_extract(prot, field)) {
		DPAA2_PMD_ERR("%s only for none IP address extract",
			__func__);
		return -EINVAL;
	}

	if (dpkg->num_extracts >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Number of extracts overflows");
		return -EINVAL;
	}

	pos = dpaa2_flow_key_profile_advance(prot,
			field, field_size, priv,
			dist_type, tc_id,
			insert_offset);
	if (pos < 0)
		return pos;

	if (pos != dpkg->num_extracts) {
		/* Not the last pos, must have IP address extract.*/
		for (i = dpkg->num_extracts - 1; i >= pos; i--) {
			extracts[i + 1] = extracts[i];
		}
	}

	extracts[pos].type = DPKG_EXTRACT_FROM_HDR;
	extracts[pos].extract.from_hdr.prot = prot;
	extracts[pos].extract.from_hdr.type = DPKG_FULL_FIELD;
	extracts[pos].extract.from_hdr.field = field;

	dpkg->num_extracts++;

	return 0;
}

static int
dpaa2_flow_extract_new_raw(struct dpaa2_dev_priv *priv,
	int offset, int size,
	enum dpaa2_flow_dist_type dist_type, int tc_id)
{
	struct dpaa2_key_extract *key_extract;
	struct dpkg_profile_cfg *dpkg;
	struct dpaa2_key_profile *key_profile;
	int last_extract_size, index, pos, item_size;
	uint8_t num_extracts;
	uint32_t field;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_extract = &priv->extract.qos_key_extract;
	else
		key_extract = &priv->extract.tc_key_extract[tc_id];

	dpkg = &key_extract->dpkg;
	key_profile = &key_extract->key_profile;

	key_profile->raw_region.raw_start = 0;
	key_profile->raw_region.raw_size = 0;

	last_extract_size = (size % DPAA2_FLOW_MAX_KEY_SIZE);
	num_extracts = (size / DPAA2_FLOW_MAX_KEY_SIZE);
	if (last_extract_size)
		num_extracts++;
	else
		last_extract_size = DPAA2_FLOW_MAX_KEY_SIZE;

	for (index = 0; index < num_extracts; index++) {
		if (index == num_extracts - 1)
			item_size = last_extract_size;
		else
			item_size = DPAA2_FLOW_MAX_KEY_SIZE;
		field = offset << DPAA2_FLOW_RAW_OFFSET_FIELD_SHIFT;
		field |= item_size;

		pos = dpaa2_flow_key_profile_advance(NET_PROT_PAYLOAD,
				field, item_size, priv, dist_type,
				tc_id, NULL);
		if (pos < 0)
			return pos;

		dpkg->extracts[pos].type = DPKG_EXTRACT_FROM_DATA;
		dpkg->extracts[pos].extract.from_data.size = item_size;
		dpkg->extracts[pos].extract.from_data.offset = offset;

		if (index == 0) {
			key_profile->raw_extract_pos = pos;
			key_profile->raw_extract_off =
				key_profile->key_offset[pos];
			key_profile->raw_region.raw_start = offset;
		}
		key_profile->raw_extract_num++;
		key_profile->raw_region.raw_size +=
			key_profile->key_size[pos];

		offset += item_size;
		dpkg->num_extracts++;
	}

	return 0;
}

static int
dpaa2_flow_extract_add_raw(struct dpaa2_dev_priv *priv,
	int offset, int size, enum dpaa2_flow_dist_type dist_type,
	int tc_id, int *recfg)
{
	struct dpaa2_key_profile *key_profile;
	struct dpaa2_raw_region *raw_region;
	int end = offset + size, ret = 0, extract_extended, sz_extend;
	int start_cmp, end_cmp, new_size, index, pos, end_pos;
	int last_extract_size, item_size, num_extracts, bk_num = 0;
	struct dpkg_extract extract_bk[DPKG_MAX_NUM_OF_EXTRACTS];
	uint8_t key_offset_bk[DPKG_MAX_NUM_OF_EXTRACTS];
	uint8_t key_size_bk[DPKG_MAX_NUM_OF_EXTRACTS];
	struct key_prot_field prot_field_bk[DPKG_MAX_NUM_OF_EXTRACTS];
	struct dpaa2_raw_region raw_hole;
	struct dpkg_profile_cfg *dpkg;
	enum net_prot prot;
	uint32_t field;

	if (dist_type == DPAA2_FLOW_QOS_TYPE) {
		key_profile = &priv->extract.qos_key_extract.key_profile;
		dpkg = &priv->extract.qos_key_extract.dpkg;
	} else {
		key_profile = &priv->extract.tc_key_extract[tc_id].key_profile;
		dpkg = &priv->extract.tc_key_extract[tc_id].dpkg;
	}

	raw_region = &key_profile->raw_region;
	if (!raw_region->raw_size) {
		/* New RAW region*/
		ret = dpaa2_flow_extract_new_raw(priv, offset, size,
			dist_type, tc_id);
		if (!ret && recfg)
			(*recfg) |= dist_type;

		return ret;
	}
	start_cmp = raw_region->raw_start;
	end_cmp = raw_region->raw_start + raw_region->raw_size;

	if (offset >= start_cmp && end <= end_cmp)
		return 0;

	sz_extend = 0;
	new_size = raw_region->raw_size;
	if (offset < start_cmp) {
		sz_extend += start_cmp - offset;
		new_size += (start_cmp - offset);
	}
	if (end > end_cmp) {
		sz_extend += end - end_cmp;
		new_size += (end - end_cmp);
	}

	last_extract_size = (new_size % DPAA2_FLOW_MAX_KEY_SIZE);
	num_extracts = (new_size / DPAA2_FLOW_MAX_KEY_SIZE);
	if (last_extract_size)
		num_extracts++;
	else
		last_extract_size = DPAA2_FLOW_MAX_KEY_SIZE;

	if ((key_profile->num + num_extracts -
		key_profile->raw_extract_num) >=
		DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("%s Failed to expand raw extracts",
			__func__);
		return -EINVAL;
	}

	if (offset < start_cmp) {
		raw_hole.raw_start = key_profile->raw_extract_off;
		raw_hole.raw_size = start_cmp - offset;
		raw_region->raw_start = offset;
		raw_region->raw_size += start_cmp - offset;

		if (dist_type & DPAA2_FLOW_QOS_TYPE) {
			ret = dpaa2_flow_qos_rule_insert_hole(priv,
					raw_hole.raw_start,
					raw_hole.raw_size);
			if (ret)
				return ret;
		}
		if (dist_type & DPAA2_FLOW_FS_TYPE) {
			ret = dpaa2_flow_fs_rule_insert_hole(priv,
					raw_hole.raw_start,
					raw_hole.raw_size, tc_id);
			if (ret)
				return ret;
		}
	}

	if (end > end_cmp) {
		raw_hole.raw_start =
			key_profile->raw_extract_off +
			raw_region->raw_size;
		raw_hole.raw_size = end - end_cmp;
		raw_region->raw_size += end - end_cmp;

		if (dist_type & DPAA2_FLOW_QOS_TYPE) {
			ret = dpaa2_flow_qos_rule_insert_hole(priv,
					raw_hole.raw_start,
					raw_hole.raw_size);
			if (ret)
				return ret;
		}
		if (dist_type & DPAA2_FLOW_FS_TYPE) {
			ret = dpaa2_flow_fs_rule_insert_hole(priv,
					raw_hole.raw_start,
					raw_hole.raw_size, tc_id);
			if (ret)
				return ret;
		}
	}

	end_pos = key_profile->raw_extract_pos +
		key_profile->raw_extract_num;
	if (key_profile->num > end_pos) {
		bk_num = key_profile->num - end_pos;
		memcpy(extract_bk, &dpkg->extracts[end_pos],
			bk_num * sizeof(struct dpkg_extract));
		memcpy(key_offset_bk, &key_profile->key_offset[end_pos],
			bk_num * sizeof(uint8_t));
		memcpy(key_size_bk, &key_profile->key_size[end_pos],
			bk_num * sizeof(uint8_t));
		memcpy(prot_field_bk, &key_profile->prot_field[end_pos],
			bk_num * sizeof(struct key_prot_field));

		for (index = 0; index < bk_num; index++) {
			key_offset_bk[index] += sz_extend;
			prot = prot_field_bk[index].prot;
			field = prot_field_bk[index].key_field;
			if (dpaa2_flow_l4_src_port_extract(prot,
				field)) {
				key_profile->l4_src_port_present = 1;
				key_profile->l4_src_port_pos = end_pos + index;
				key_profile->l4_src_port_offset =
					key_offset_bk[index];
			} else if (dpaa2_flow_l4_dst_port_extract(prot,
				field)) {
				key_profile->l4_dst_port_present = 1;
				key_profile->l4_dst_port_pos = end_pos + index;
				key_profile->l4_dst_port_offset =
					key_offset_bk[index];
			}
		}
	}

	pos = key_profile->raw_extract_pos;

	for (index = 0; index < num_extracts; index++) {
		if (index == num_extracts - 1)
			item_size = last_extract_size;
		else
			item_size = DPAA2_FLOW_MAX_KEY_SIZE;
		field = offset << DPAA2_FLOW_RAW_OFFSET_FIELD_SHIFT;
		field |= item_size;

		if (pos > 0) {
			key_profile->key_offset[pos] =
				key_profile->key_offset[pos - 1] +
				key_profile->key_size[pos - 1];
		} else {
			key_profile->key_offset[pos] = 0;
		}
		key_profile->key_size[pos] = item_size;
		key_profile->prot_field[pos].type = DPAA2_NET_PROT_KEY;
		key_profile->prot_field[pos].prot = NET_PROT_PAYLOAD;
		key_profile->prot_field[pos].key_field = field;

		dpkg->extracts[pos].type = DPKG_EXTRACT_FROM_DATA;
		dpkg->extracts[pos].extract.from_data.size = item_size;
		dpkg->extracts[pos].extract.from_data.offset = offset;
		offset += item_size;
		pos++;
	}

	if (bk_num) {
		memcpy(&dpkg->extracts[pos], extract_bk,
			bk_num * sizeof(struct dpkg_extract));
		memcpy(&key_profile->key_offset[end_pos],
			key_offset_bk, bk_num * sizeof(uint8_t));
		memcpy(&key_profile->key_size[end_pos],
			key_size_bk, bk_num * sizeof(uint8_t));
		memcpy(&key_profile->prot_field[end_pos],
			prot_field_bk, bk_num * sizeof(struct key_prot_field));
	}

	extract_extended = num_extracts - key_profile->raw_extract_num;
	if (key_profile->ip_addr_type != IP_NONE_ADDR_EXTRACT) {
		key_profile->ip_addr_extract_pos += extract_extended;
		key_profile->ip_addr_extract_off += sz_extend;
	}
	key_profile->raw_extract_num = num_extracts;
	key_profile->num += extract_extended;
	key_profile->key_max_size += sz_extend;

	dpkg->num_extracts += extract_extended;
	if (!ret && recfg)
		(*recfg) |= dist_type;

	return ret;
}

static inline int
dpaa2_flow_extract_search(struct dpaa2_key_profile *key_profile,
	enum key_prot_type type, enum net_prot prot, uint32_t key_field)
{
	int pos;
	struct key_prot_field *prot_field;

	if (dpaa2_flow_ip_address_extract(prot, key_field)) {
		DPAA2_PMD_ERR("%s only for none IP address extract",
			__func__);
		return -EINVAL;
	}

	prot_field = key_profile->prot_field;
	for (pos = 0; pos < key_profile->num; pos++) {
		if (type == DPAA2_NET_PROT_KEY &&
			prot_field[pos].prot == prot &&
			prot_field[pos].key_field == key_field &&
			prot_field[pos].type == type)
			return pos;
		else if (type == DPAA2_FAF_KEY &&
			prot_field[pos].key_field == key_field &&
			prot_field[pos].type == type)
			return pos;
		else if (type == DPAA2_PR_KEY &&
			prot_field[pos].key_field == key_field &&
			prot_field[pos].type == type)
			return pos;
	}

	if (type == DPAA2_NET_PROT_KEY &&
		dpaa2_flow_l4_src_port_extract(prot, key_field)) {
		if (key_profile->l4_src_port_present)
			return key_profile->l4_src_port_pos;
	} else if (type == DPAA2_NET_PROT_KEY &&
		dpaa2_flow_l4_dst_port_extract(prot, key_field)) {
		if (key_profile->l4_dst_port_present)
			return key_profile->l4_dst_port_pos;
	}

	return -ENXIO;
}

static inline int
dpaa2_flow_extract_key_offset(struct dpaa2_key_profile *key_profile,
	enum key_prot_type type, enum net_prot prot, uint32_t key_field)
{
	int i;

	i = dpaa2_flow_extract_search(key_profile, type, prot, key_field);
	if (i >= 0)
		return key_profile->key_offset[i];
	else
		return i;
}

static int
dpaa2_flow_faf_add_rule(struct dpaa2_dev_priv *priv,
	struct dpaa2_dev_flow *flow,
	enum dpaa2_rx_faf_offset faf_bit_off,
	int group,
	enum dpaa2_flow_dist_type dist_type)
{
	int offset;
	uint8_t *key_addr;
	uint8_t *mask_addr;
	struct dpaa2_key_extract *key_extract;
	struct dpaa2_key_profile *key_profile;
	uint8_t faf_byte = faf_bit_off / 8;
	uint8_t faf_bit_in_byte = faf_bit_off % 8;

	faf_bit_in_byte = 7 - faf_bit_in_byte;

	if (dist_type & DPAA2_FLOW_QOS_TYPE) {
		key_extract = &priv->extract.qos_key_extract;
		key_profile = &key_extract->key_profile;

		offset = dpaa2_flow_extract_key_offset(key_profile,
				DPAA2_FAF_KEY, NET_PROT_NONE, faf_byte);
		if (offset < 0) {
			DPAA2_PMD_ERR("%s QoS key extract failed", __func__);
			return -EINVAL;
		}
		key_addr = flow->qos_key_addr + offset;
		mask_addr = flow->qos_mask_addr + offset;

		if (!(*key_addr) &&
			key_profile->ip_addr_type == IP_NONE_ADDR_EXTRACT &&
			offset >= flow->qos_rule_size)
			flow->qos_rule_size = offset + sizeof(uint8_t);

		*key_addr |=  (1 << faf_bit_in_byte);
		*mask_addr |=  (1 << faf_bit_in_byte);
	}

	if (dist_type & DPAA2_FLOW_FS_TYPE) {
		key_extract = &priv->extract.tc_key_extract[group];
		key_profile = &key_extract->key_profile;

		offset = dpaa2_flow_extract_key_offset(key_profile,
				DPAA2_FAF_KEY, NET_PROT_NONE, faf_byte);
		if (offset < 0) {
			DPAA2_PMD_ERR("%s TC[%d] key extract failed",
				__func__, group);
			return -EINVAL;
		}
		key_addr = flow->fs_key_addr + offset;
		mask_addr = flow->fs_mask_addr + offset;

		if (!(*key_addr) &&
			key_profile->ip_addr_type == IP_NONE_ADDR_EXTRACT &&
			offset >= flow->fs_rule_size)
			flow->fs_rule_size = offset + sizeof(uint8_t);

		*key_addr |=  (1 << faf_bit_in_byte);
		*mask_addr |=  (1 << faf_bit_in_byte);
	}

	return 0;
}

static inline int
dpaa2_flow_pr_rule_data_set(struct dpaa2_dev_flow *flow,
	struct dpaa2_key_profile *key_profile,
	uint32_t pr_offset, uint32_t pr_size,
	const void *key, const void *mask,
	enum dpaa2_flow_dist_type dist_type)
{
	int offset;
	uint32_t pr_field = pr_offset << 16 | pr_size;
	char offset_info[64], size_info[64], rule_size_info[64];

	offset = dpaa2_flow_extract_key_offset(key_profile,
			DPAA2_PR_KEY, NET_PROT_NONE, pr_field);
	if (offset < 0) {
		DPAA2_PMD_ERR("PR off(%d)/size(%d) does not exist!",
			pr_offset, pr_size);
		return -EINVAL;
	}
	sprintf(offset_info, "offset(%d)", offset);
	sprintf(size_info, "size(%d)", pr_size);

	if (dist_type & DPAA2_FLOW_QOS_TYPE) {
		sprintf(rule_size_info, "qos rule size(%d)",
			flow->qos_rule_size);
		memcpy((flow->qos_key_addr + offset), key, pr_size);
		memcpy((flow->qos_mask_addr + offset), mask, pr_size);
		if (key_profile->ip_addr_type == IP_NONE_ADDR_EXTRACT) {
			if (offset >= flow->qos_rule_size) {
				flow->qos_rule_size = offset + pr_size;
			} else if ((offset + pr_size) > flow->qos_rule_size) {
				DPAA2_PMD_ERR("%s < %s, but %s + %s > %s",
					offset_info, rule_size_info,
					offset_info, size_info,
					rule_size_info);
				return -EINVAL;
			}
		}
	}

	if (dist_type & DPAA2_FLOW_FS_TYPE) {
		sprintf(rule_size_info, "fs rule size(%d)",
			flow->fs_rule_size);
		memcpy((flow->fs_key_addr + offset), key, pr_size);
		memcpy((flow->fs_mask_addr + offset), mask, pr_size);
		if (key_profile->ip_addr_type == IP_NONE_ADDR_EXTRACT) {
			if (offset >= flow->fs_rule_size) {
				flow->fs_rule_size = offset + pr_size;
			} else if ((offset + pr_size) > flow->fs_rule_size) {
				DPAA2_PMD_ERR("%s < %s, but %s + %s > %s",
					offset_info, rule_size_info,
					offset_info, size_info,
					rule_size_info);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static inline int
dpaa2_flow_hdr_rule_data_set(struct dpaa2_dev_flow *flow,
	struct dpaa2_key_profile *key_profile,
	enum net_prot prot, uint32_t field, int size,
	const void *key, const void *mask,
	enum dpaa2_flow_dist_type dist_type)
{
	int offset;
	char offset_info[64], size_info[64], rule_size_info[64];

	if (dpaa2_flow_ip_address_extract(prot, field)) {
		DPAA2_PMD_ERR("%s only for none IP address extract",
			__func__);
		return -EINVAL;
	}

	offset = dpaa2_flow_extract_key_offset(key_profile,
			DPAA2_NET_PROT_KEY, prot, field);
	if (offset < 0) {
		DPAA2_PMD_ERR("P(%d)/F(%d) does not exist!",
			prot, field);
		return -EINVAL;
	}
	sprintf(offset_info, "offset(%d)", offset);
	sprintf(size_info, "size(%d)", size);

	if (dist_type & DPAA2_FLOW_QOS_TYPE) {
		sprintf(rule_size_info, "qos rule size(%d)",
			flow->qos_rule_size);
		memcpy((flow->qos_key_addr + offset), key, size);
		memcpy((flow->qos_mask_addr + offset), mask, size);
		if (key_profile->ip_addr_type == IP_NONE_ADDR_EXTRACT) {
			if (offset >= flow->qos_rule_size) {
				flow->qos_rule_size = offset + size;
			} else if ((offset + size) > flow->qos_rule_size) {
				DPAA2_PMD_ERR("%s: %s < %s, but %s + %s > %s",
					__func__, offset_info, rule_size_info,
					offset_info, size_info, rule_size_info);
				return -EINVAL;
			}
		}
	}

	if (dist_type & DPAA2_FLOW_FS_TYPE) {
		sprintf(rule_size_info, "fs rule size(%d)",
			flow->fs_rule_size);
		memcpy((flow->fs_key_addr + offset), key, size);
		memcpy((flow->fs_mask_addr + offset), mask, size);
		if (key_profile->ip_addr_type == IP_NONE_ADDR_EXTRACT) {
			if (offset >= flow->fs_rule_size) {
				flow->fs_rule_size = offset + size;
			} else if ((offset + size) > flow->fs_rule_size) {
				DPAA2_PMD_ERR("%s: %s < %s, but %s + %s > %s",
					__func__, offset_info, rule_size_info,
					offset_info, size_info, rule_size_info);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static inline int
dpaa2_flow_raw_rule_data_set(struct dpaa2_dev_flow *flow,
	struct dpaa2_key_profile *key_profile,
	uint32_t extract_offset, int size,
	const void *key, const void *mask,
	enum dpaa2_flow_dist_type dist_type)
{
	int extract_size = size > DPAA2_FLOW_MAX_KEY_SIZE ?
		DPAA2_FLOW_MAX_KEY_SIZE : size;
	int offset, field;
	char offset_info[64], size_info[64], rule_size_info[64];

	field = extract_offset << DPAA2_FLOW_RAW_OFFSET_FIELD_SHIFT;
	field |= extract_size;
	offset = dpaa2_flow_extract_key_offset(key_profile,
			DPAA2_NET_PROT_KEY, NET_PROT_PAYLOAD, field);
	if (offset < 0) {
		DPAA2_PMD_ERR("offset(%d)/size(%d) raw extract failed",
			extract_offset, size);
		return -EINVAL;
	}
	sprintf(offset_info, "offset(%d)", offset);
	sprintf(size_info, "size(%d)", size);

	if (dist_type & DPAA2_FLOW_QOS_TYPE) {
		sprintf(rule_size_info, "qos rule size(%d)",
			flow->qos_rule_size);
		memcpy((flow->qos_key_addr + offset), key, size);
		memcpy((flow->qos_mask_addr + offset), mask, size);
		if (offset >= flow->qos_rule_size) {
			flow->qos_rule_size = offset + size;
		} else if ((offset + size) > flow->qos_rule_size) {
			DPAA2_PMD_ERR("%s: %s < %s, but %s + %s > %s",
				__func__, offset_info, rule_size_info,
				offset_info, size_info, rule_size_info);
			return -EINVAL;
		}
	}

	if (dist_type & DPAA2_FLOW_FS_TYPE) {
		sprintf(rule_size_info, "fs rule size(%d)",
			flow->fs_rule_size);
		memcpy((flow->fs_key_addr + offset), key, size);
		memcpy((flow->fs_mask_addr + offset), mask, size);
		if (offset >= flow->fs_rule_size) {
			flow->fs_rule_size = offset + size;
		} else if ((offset + size) > flow->fs_rule_size) {
			DPAA2_PMD_ERR("%s: %s < %s, but %s + %s > %s",
				__func__, offset_info, rule_size_info,
				offset_info, size_info, rule_size_info);
			return -EINVAL;
		}
	}

	return 0;
}

static int
dpaa2_flow_extract_support(const uint8_t *mask_src,
	enum rte_flow_item_type type)
{
	char mask[64];
	int i, size = 0;
	const char *mask_support = 0;

	switch (type) {
	case RTE_FLOW_ITEM_TYPE_ETH:
		mask_support = (const char *)&dpaa2_flow_item_eth_mask;
		size = sizeof(struct rte_flow_item_eth);
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		mask_support = (const char *)&dpaa2_flow_item_vlan_mask;
		size = sizeof(struct rte_flow_item_vlan);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		mask_support = (const char *)&dpaa2_flow_item_ipv4_mask;
		size = sizeof(struct rte_flow_item_ipv4);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		mask_support = (const char *)&dpaa2_flow_item_ipv6_mask;
		size = sizeof(struct rte_flow_item_ipv6);
		break;
	case RTE_FLOW_ITEM_TYPE_ICMP:
		mask_support = (const char *)&dpaa2_flow_item_icmp_mask;
		size = sizeof(struct rte_flow_item_icmp);
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		mask_support = (const char *)&dpaa2_flow_item_udp_mask;
		size = sizeof(struct rte_flow_item_udp);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		mask_support = (const char *)&dpaa2_flow_item_tcp_mask;
		size = sizeof(struct rte_flow_item_tcp);
		break;
	case RTE_FLOW_ITEM_TYPE_ESP:
		mask_support = (const char *)&dpaa2_flow_item_esp_mask;
		size = sizeof(struct rte_flow_item_esp);
		break;
	case RTE_FLOW_ITEM_TYPE_AH:
		mask_support = (const char *)&dpaa2_flow_item_ah_mask;
		size = sizeof(struct rte_flow_item_ah);
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		mask_support = (const char *)&dpaa2_flow_item_sctp_mask;
		size = sizeof(struct rte_flow_item_sctp);
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		mask_support = (const char *)&dpaa2_flow_item_gre_mask;
		size = sizeof(struct rte_flow_item_gre);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		mask_support = (const char *)&dpaa2_flow_item_vxlan_mask;
		size = sizeof(struct rte_flow_item_vxlan);
		break;
	case RTE_FLOW_ITEM_TYPE_ECPRI:
		mask_support = (const char *)&dpaa2_flow_item_ecpri_mask;
		size = sizeof(struct rte_flow_item_ecpri);
		break;
	case RTE_FLOW_ITEM_TYPE_GTP:
		mask_support = (const char *)&dpaa2_flow_item_gtp_mask;
		size = sizeof(struct rte_flow_item_gtp);
		break;
	default:
		return -EINVAL;
	}

	memcpy(mask, mask_support, size);

	for (i = 0; i < size; i++)
		mask[i] = (mask[i] | mask_src[i]);

	if (memcmp(mask, mask_support, size))
		return -ENOTSUP;

	return 0;
}

static int
dpaa2_flow_identify_by_faf(struct dpaa2_dev_priv *priv,
	struct dpaa2_dev_flow *flow,
	enum dpaa2_rx_faf_offset faf_off,
	enum dpaa2_flow_dist_type dist_type,
	int group, int *recfg)
{
	int ret, index, local_cfg = 0;
	struct dpaa2_key_extract *extract;
	struct dpaa2_key_profile *key_profile;
	uint8_t faf_byte = faf_off / 8;

	if (dist_type & DPAA2_FLOW_QOS_TYPE) {
		extract = &priv->extract.qos_key_extract;
		key_profile = &extract->key_profile;

		index = dpaa2_flow_extract_search(key_profile,
				DPAA2_FAF_KEY, NET_PROT_NONE, faf_byte);
		if (index < 0) {
			ret = dpaa2_flow_faf_add_hdr(faf_byte,
					priv, DPAA2_FLOW_QOS_TYPE, group,
					NULL);
			if (ret) {
				DPAA2_PMD_ERR("QOS faf extract add failed");

				return -EINVAL;
			}
			local_cfg |= DPAA2_FLOW_QOS_TYPE;
		}

		ret = dpaa2_flow_faf_add_rule(priv, flow, faf_off, group,
				DPAA2_FLOW_QOS_TYPE);
		if (ret) {
			DPAA2_PMD_ERR("QoS faf rule set failed");
			return -EINVAL;
		}
	}

	if (dist_type & DPAA2_FLOW_FS_TYPE) {
		extract = &priv->extract.tc_key_extract[group];
		key_profile = &extract->key_profile;

		index = dpaa2_flow_extract_search(key_profile,
				DPAA2_FAF_KEY, NET_PROT_NONE, faf_byte);
		if (index < 0) {
			ret = dpaa2_flow_faf_add_hdr(faf_byte,
					priv, DPAA2_FLOW_FS_TYPE, group,
					NULL);
			if (ret) {
				DPAA2_PMD_ERR("FS[%d] faf extract add failed",
					group);

				return -EINVAL;
			}
			local_cfg |= DPAA2_FLOW_FS_TYPE;
		}

		ret = dpaa2_flow_faf_add_rule(priv, flow, faf_off, group,
				DPAA2_FLOW_FS_TYPE);
		if (ret) {
			DPAA2_PMD_ERR("FS[%d] faf rule set failed",
				group);
			return -EINVAL;
		}
	}

	if (recfg)
		*recfg |= local_cfg;

	return 0;
}

static int
dpaa2_flow_add_pr_extract_rule(struct dpaa2_dev_flow *flow,
	uint32_t pr_offset, uint32_t pr_size,
	const void *key, const void *mask,
	struct dpaa2_dev_priv *priv, int tc_id, int *recfg,
	enum dpaa2_flow_dist_type dist_type)
{
	int index, ret, local_cfg = 0;
	struct dpaa2_key_extract *key_extract;
	struct dpaa2_key_profile *key_profile;
	uint32_t pr_field = pr_offset << 16 | pr_size;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_extract = &priv->extract.qos_key_extract;
	else
		key_extract = &priv->extract.tc_key_extract[tc_id];

	key_profile = &key_extract->key_profile;

	index = dpaa2_flow_extract_search(key_profile,
			DPAA2_PR_KEY, NET_PROT_NONE, pr_field);
	if (index < 0) {
		ret = dpaa2_flow_pr_add_hdr(pr_offset,
				pr_size, priv,
				dist_type, tc_id, NULL);
		if (ret) {
			DPAA2_PMD_ERR("PR add off(%d)/size(%d) failed",
				pr_offset, pr_size);

			return ret;
		}
		local_cfg |= dist_type;
	}

	ret = dpaa2_flow_pr_rule_data_set(flow, key_profile,
			pr_offset, pr_size, key, mask, dist_type);
	if (ret) {
		DPAA2_PMD_ERR("PR off(%d)/size(%d) rule data set failed",
			pr_offset, pr_size);

		return ret;
	}

	if (recfg)
		*recfg |= local_cfg;

	return 0;
}

static int
dpaa2_flow_add_hdr_extract_rule(struct dpaa2_dev_flow *flow,
	enum net_prot prot, uint32_t field,
	const void *key, const void *mask, int size,
	struct dpaa2_dev_priv *priv, int tc_id, int *recfg,
	enum dpaa2_flow_dist_type dist_type)
{
	int index, ret, local_cfg = 0;
	struct dpaa2_key_extract *key_extract;
	struct dpaa2_key_profile *key_profile;

	if (dpaa2_flow_ip_address_extract(prot, field))
		return -EINVAL;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		key_extract = &priv->extract.qos_key_extract;
	else
		key_extract = &priv->extract.tc_key_extract[tc_id];

	key_profile = &key_extract->key_profile;

	index = dpaa2_flow_extract_search(key_profile,
			DPAA2_NET_PROT_KEY, prot, field);
	if (index < 0) {
		ret = dpaa2_flow_extract_add_hdr(prot,
				field, size, priv,
				dist_type, tc_id, NULL);
		if (ret) {
			DPAA2_PMD_ERR("QoS Extract P(%d)/F(%d) failed",
				prot, field);

			return ret;
		}
		local_cfg |= dist_type;
	}

	ret = dpaa2_flow_hdr_rule_data_set(flow, key_profile,
			prot, field, size, key, mask, dist_type);
	if (ret) {
		DPAA2_PMD_ERR("QoS P(%d)/F(%d) rule data set failed",
			prot, field);

		return ret;
	}

	if (recfg)
		*recfg |= local_cfg;

	return 0;
}

static int
dpaa2_flow_add_ipaddr_extract_rule(struct dpaa2_dev_flow *flow,
	enum net_prot prot, uint32_t field,
	const void *key, const void *mask, int size,
	struct dpaa2_dev_priv *priv, int tc_id, int *recfg,
	enum dpaa2_flow_dist_type dist_type)
{
	int local_cfg = 0, num, ipaddr_extract_len = 0;
	struct dpaa2_key_extract *key_extract;
	struct dpaa2_key_profile *key_profile;
	struct dpkg_profile_cfg *dpkg;
	uint8_t *key_addr, *mask_addr;
	union ip_addr_extract_rule *ip_addr_data;
	union ip_addr_extract_rule *ip_addr_mask;
	enum net_prot orig_prot;
	uint32_t orig_field;

	if (prot != NET_PROT_IPV4 && prot != NET_PROT_IPV6)
		return -EINVAL;

	if (prot == NET_PROT_IPV4 && field != NH_FLD_IPV4_SRC_IP &&
		field != NH_FLD_IPV4_DST_IP) {
		return -EINVAL;
	}

	if (prot == NET_PROT_IPV6 && field != NH_FLD_IPV6_SRC_IP &&
		field != NH_FLD_IPV6_DST_IP) {
		return -EINVAL;
	}

	orig_prot = prot;
	orig_field = field;

	if (prot == NET_PROT_IPV4 &&
		field == NH_FLD_IPV4_SRC_IP) {
		prot = NET_PROT_IP;
		field = NH_FLD_IP_SRC;
	} else if (prot == NET_PROT_IPV4 &&
		field == NH_FLD_IPV4_DST_IP) {
		prot = NET_PROT_IP;
		field = NH_FLD_IP_DST;
	} else if (prot == NET_PROT_IPV6 &&
		field == NH_FLD_IPV6_SRC_IP) {
		prot = NET_PROT_IP;
		field = NH_FLD_IP_SRC;
	} else if (prot == NET_PROT_IPV6 &&
		field == NH_FLD_IPV6_DST_IP) {
		prot = NET_PROT_IP;
		field = NH_FLD_IP_DST;
	} else {
		DPAA2_PMD_ERR("Inval P(%d)/F(%d) to extract ip address",
			prot, field);
		return -EINVAL;
	}

	if (dist_type == DPAA2_FLOW_QOS_TYPE) {
		key_extract = &priv->extract.qos_key_extract;
		key_profile = &key_extract->key_profile;
		dpkg = &key_extract->dpkg;
		num = key_profile->num;
		key_addr = flow->qos_key_addr;
		mask_addr = flow->qos_mask_addr;
	} else {
		key_extract = &priv->extract.tc_key_extract[tc_id];
		key_profile = &key_extract->key_profile;
		dpkg = &key_extract->dpkg;
		num = key_profile->num;
		key_addr = flow->fs_key_addr;
		mask_addr = flow->fs_mask_addr;
	}

	if (num >= DPKG_MAX_NUM_OF_EXTRACTS) {
		DPAA2_PMD_ERR("Number of extracts overflows");
		return -EINVAL;
	}

	if (key_profile->ip_addr_type == IP_NONE_ADDR_EXTRACT) {
		if (field == NH_FLD_IP_SRC)
			key_profile->ip_addr_type = IP_SRC_EXTRACT;
		else
			key_profile->ip_addr_type = IP_DST_EXTRACT;
		ipaddr_extract_len = size;

		key_profile->ip_addr_extract_pos = num;
		if (num > 0) {
			key_profile->ip_addr_extract_off =
				key_profile->key_offset[num - 1] +
				key_profile->key_size[num - 1];
		} else {
			key_profile->ip_addr_extract_off = 0;
		}
		key_profile->key_max_size += NH_FLD_IPV6_ADDR_SIZE;
	} else if (key_profile->ip_addr_type == IP_SRC_EXTRACT) {
		if (field == NH_FLD_IP_SRC) {
			ipaddr_extract_len = size;
			goto rule_configure;
		}
		key_profile->ip_addr_type = IP_SRC_DST_EXTRACT;
		ipaddr_extract_len = size * 2;
		key_profile->key_max_size += NH_FLD_IPV6_ADDR_SIZE;
	} else if (key_profile->ip_addr_type == IP_DST_EXTRACT) {
		if (field == NH_FLD_IP_DST) {
			ipaddr_extract_len = size;
			goto rule_configure;
		}
		key_profile->ip_addr_type = IP_DST_SRC_EXTRACT;
		ipaddr_extract_len = size * 2;
		key_profile->key_max_size += NH_FLD_IPV6_ADDR_SIZE;
	}
	key_profile->num++;
	key_profile->prot_field[num].type = DPAA2_NET_PROT_KEY;

	dpkg->extracts[num].extract.from_hdr.prot = prot;
	dpkg->extracts[num].extract.from_hdr.field = field;
	dpkg->extracts[num].extract.from_hdr.type = DPKG_FULL_FIELD;
	dpkg->num_extracts++;

	if (dist_type == DPAA2_FLOW_QOS_TYPE)
		local_cfg = DPAA2_FLOW_QOS_TYPE;
	else
		local_cfg = DPAA2_FLOW_FS_TYPE;

rule_configure:
	key_addr += key_profile->ip_addr_extract_off;
	ip_addr_data = (union ip_addr_extract_rule *)key_addr;
	mask_addr += key_profile->ip_addr_extract_off;
	ip_addr_mask = (union ip_addr_extract_rule *)mask_addr;

	if (orig_prot == NET_PROT_IPV4 &&
		orig_field == NH_FLD_IPV4_SRC_IP) {
		if (key_profile->ip_addr_type == IP_SRC_EXTRACT ||
			key_profile->ip_addr_type == IP_SRC_DST_EXTRACT) {
			memcpy(&ip_addr_data->ipv4_sd_addr.ipv4_src,
				key, size);
			memcpy(&ip_addr_mask->ipv4_sd_addr.ipv4_src,
				mask, size);
		} else {
			memcpy(&ip_addr_data->ipv4_ds_addr.ipv4_src,
				key, size);
			memcpy(&ip_addr_mask->ipv4_ds_addr.ipv4_src,
				mask, size);
		}
	} else if (orig_prot == NET_PROT_IPV4 &&
		orig_field == NH_FLD_IPV4_DST_IP) {
		if (key_profile->ip_addr_type == IP_DST_EXTRACT ||
			key_profile->ip_addr_type == IP_DST_SRC_EXTRACT) {
			memcpy(&ip_addr_data->ipv4_ds_addr.ipv4_dst,
				key, size);
			memcpy(&ip_addr_mask->ipv4_ds_addr.ipv4_dst,
				mask, size);
		} else {
			memcpy(&ip_addr_data->ipv4_sd_addr.ipv4_dst,
				key, size);
			memcpy(&ip_addr_mask->ipv4_sd_addr.ipv4_dst,
				mask, size);
		}
	} else if (orig_prot == NET_PROT_IPV6 &&
		orig_field == NH_FLD_IPV6_SRC_IP) {
		if (key_profile->ip_addr_type == IP_SRC_EXTRACT ||
			key_profile->ip_addr_type == IP_SRC_DST_EXTRACT) {
			memcpy(ip_addr_data->ipv6_sd_addr.ipv6_src,
				key, size);
			memcpy(ip_addr_mask->ipv6_sd_addr.ipv6_src,
				mask, size);
		} else {
			memcpy(ip_addr_data->ipv6_ds_addr.ipv6_src,
				key, size);
			memcpy(ip_addr_mask->ipv6_ds_addr.ipv6_src,
				mask, size);
		}
	} else if (orig_prot == NET_PROT_IPV6 &&
		orig_field == NH_FLD_IPV6_DST_IP) {
		if (key_profile->ip_addr_type == IP_DST_EXTRACT ||
			key_profile->ip_addr_type == IP_DST_SRC_EXTRACT) {
			memcpy(ip_addr_data->ipv6_ds_addr.ipv6_dst,
				key, size);
			memcpy(ip_addr_mask->ipv6_ds_addr.ipv6_dst,
				mask, size);
		} else {
			memcpy(ip_addr_data->ipv6_sd_addr.ipv6_dst,
				key, size);
			memcpy(ip_addr_mask->ipv6_sd_addr.ipv6_dst,
				mask, size);
		}
	}

	if (dist_type == DPAA2_FLOW_QOS_TYPE) {
		flow->qos_rule_size =
			key_profile->ip_addr_extract_off + ipaddr_extract_len;
	} else {
		flow->fs_rule_size =
			key_profile->ip_addr_extract_off + ipaddr_extract_len;
	}

	if (recfg)
		*recfg |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_tunnel_eth(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_flow_item *pattern,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_eth *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const char zero_cmp[RTE_ETHER_ADDR_LEN] = {0};

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
			pattern->mask : &dpaa2_flow_item_eth_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (!spec)
		return 0;

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_ETH);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of ethernet failed");

		return ret;
	}

	if (memcmp((const char *)&mask->src,
		zero_cmp, RTE_ETHER_ADDR_LEN)) {
		/*SRC[0:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR0_OFFSET,
			1, &spec->src.addr_bytes[0],
			&mask->src.addr_bytes[0],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		/*SRC[1:2]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR1_OFFSET,
			2, &spec->src.addr_bytes[1],
			&mask->src.addr_bytes[1],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		/*SRC[3:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR3_OFFSET,
			1, &spec->src.addr_bytes[3],
			&mask->src.addr_bytes[3],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		/*SRC[4:2]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR4_OFFSET,
			2, &spec->src.addr_bytes[4],
			&mask->src.addr_bytes[4],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		/*SRC[0:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR0_OFFSET,
			1, &spec->src.addr_bytes[0],
			&mask->src.addr_bytes[0],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
		/*SRC[1:2]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR1_OFFSET,
			2, &spec->src.addr_bytes[1],
			&mask->src.addr_bytes[1],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
		/*SRC[3:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR3_OFFSET,
			1, &spec->src.addr_bytes[3],
			&mask->src.addr_bytes[3],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
		/*SRC[4:2]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_SADDR4_OFFSET,
			2, &spec->src.addr_bytes[4],
			&mask->src.addr_bytes[4],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (memcmp((const char *)&mask->dst,
		zero_cmp, RTE_ETHER_ADDR_LEN)) {
		/*DST[0:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR0_OFFSET,
			1, &spec->dst.addr_bytes[0],
			&mask->dst.addr_bytes[0],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		/*DST[1:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR1_OFFSET,
			1, &spec->dst.addr_bytes[1],
			&mask->dst.addr_bytes[1],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		/*DST[2:3]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR2_OFFSET,
			3, &spec->dst.addr_bytes[2],
			&mask->dst.addr_bytes[2],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		/*DST[5:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR5_OFFSET,
			1, &spec->dst.addr_bytes[5],
			&mask->dst.addr_bytes[5],
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		/*DST[0:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR0_OFFSET,
			1, &spec->dst.addr_bytes[0],
			&mask->dst.addr_bytes[0],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
		/*DST[1:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR1_OFFSET,
			1, &spec->dst.addr_bytes[1],
			&mask->dst.addr_bytes[1],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
		/*DST[2:3]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR2_OFFSET,
			3, &spec->dst.addr_bytes[2],
			&mask->dst.addr_bytes[2],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
		/*DST[5:1]*/
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_DADDR5_OFFSET,
			1, &spec->dst.addr_bytes[5],
			&mask->dst.addr_bytes[5],
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (memcmp((const char *)&mask->type,
		zero_cmp, sizeof(rte_be16_t))) {
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_TYPE_OFFSET,
			sizeof(rte_be16_t), &spec->type, &mask->type,
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_TYPE_OFFSET,
			sizeof(rte_be16_t), &spec->type, &mask->type,
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_eth(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_eth *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const char zero_cmp[RTE_ETHER_ADDR_LEN] = {0};
	const struct rte_flow_item *pattern =
		&dpaa2_pattern->generic_item;

	if (dpaa2_pattern->in_tunnel) {
		return dpaa2_configure_flow_tunnel_eth(flow,
				dev, attr, pattern, device_configured);
	}

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
			pattern->mask : &dpaa2_flow_item_eth_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_ETH_FRAM, DPAA2_FLOW_QOS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_ETH_FRAM, DPAA2_FLOW_FS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_ETH);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of ethernet failed");

		return ret;
	}

	if (memcmp((const char *)&mask->src,
		zero_cmp, RTE_ETHER_ADDR_LEN)) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ETH,
			NH_FLD_ETH_SA, &spec->src.addr_bytes,
			&mask->src.addr_bytes, RTE_ETHER_ADDR_LEN,
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ETH,
			NH_FLD_ETH_SA, &spec->src.addr_bytes,
			&mask->src.addr_bytes, RTE_ETHER_ADDR_LEN,
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (memcmp((const char *)&mask->dst,
		zero_cmp, RTE_ETHER_ADDR_LEN)) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ETH,
			NH_FLD_ETH_DA, &spec->dst.addr_bytes,
			&mask->dst.addr_bytes, RTE_ETHER_ADDR_LEN,
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ETH,
			NH_FLD_ETH_DA, &spec->dst.addr_bytes,
			&mask->dst.addr_bytes, RTE_ETHER_ADDR_LEN,
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (memcmp((const char *)&mask->type,
		zero_cmp, sizeof(rte_be16_t))) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ETH,
			NH_FLD_ETH_TYPE, &spec->type,
			&mask->type, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ETH,
			NH_FLD_ETH_TYPE, &spec->type,
			&mask->type, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_tunnel_vlan(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_flow_item *pattern,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_vlan *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_vlan_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_VLAN_FRAM,
				DPAA2_FLOW_QOS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_VLAN_FRAM,
				DPAA2_FLOW_FS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_VLAN);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of vlan not support.");

		return ret;
	}

	if (!mask->tci)
		return 0;

	ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_TCI_OFFSET,
			sizeof(rte_be16_t), &spec->tci, &mask->tci,
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
	if (ret)
		return ret;

	ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_IN_TCI_OFFSET,
			sizeof(rte_be16_t), &spec->tci, &mask->tci,
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
	if (ret)
		return ret;

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_vlan(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_vlan *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern =
		&dpaa2_pattern->generic_item;

	if (dpaa2_pattern->in_tunnel) {
		return dpaa2_configure_flow_tunnel_vlan(flow,
				dev, attr, pattern, device_configured);
	}

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ? pattern->mask : &dpaa2_flow_item_vlan_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow, FAF_VLAN_FRAM,
				DPAA2_FLOW_QOS_TYPE, group,
				&local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow, FAF_VLAN_FRAM,
				DPAA2_FLOW_FS_TYPE, group,
				&local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
			RTE_FLOW_ITEM_TYPE_VLAN);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of vlan not support.");
		return ret;
	}

	if (!mask->tci)
		return 0;

	ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_VLAN,
			NH_FLD_VLAN_TCI, &spec->tci,
			&mask->tci, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
	if (ret)
		return ret;

	ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_VLAN,
			NH_FLD_VLAN_TCI, &spec->tci,
			&mask->tci, sizeof(rte_be16_t),
			priv, group, &local_cfg,
			DPAA2_FLOW_FS_TYPE);
	if (ret)
		return ret;

	(*device_configured) |= local_cfg;
	return 0;
}

static int
dpaa2_configure_flow_ipv4(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_ipv4 *spec_ipv4 = 0, *mask_ipv4 = 0;
	const void *key, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	int size;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec_ipv4 = pattern->spec;
	mask_ipv4 = pattern->mask ?
		    pattern->mask : &dpaa2_flow_item_ipv4_mask;

	if (dpaa2_pattern->in_tunnel) {
		if (spec_ipv4) {
			DPAA2_PMD_ERR("Tunnel-IPv4 distribution not support");
			return -ENOTSUP;
		}

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_IPV4_FRAM,
				DPAA2_FLOW_QOS_TYPE, group,
				&local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_IPV4_FRAM,
				DPAA2_FLOW_FS_TYPE, group,
				&local_cfg);
		return ret;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	ret = dpaa2_flow_identify_by_faf(priv, flow, FAF_IPV4_FRAM,
			DPAA2_FLOW_QOS_TYPE, group,
			&local_cfg);
	if (ret)
		return ret;

	ret = dpaa2_flow_identify_by_faf(priv, flow, FAF_IPV4_FRAM,
			DPAA2_FLOW_FS_TYPE, group, &local_cfg);
	if (ret)
		return ret;

	if (!spec_ipv4) {
		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask_ipv4,
			RTE_FLOW_ITEM_TYPE_IPV4);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of IPv4 not support.");
		return ret;
	}

	if (mask_ipv4->hdr.src_addr) {
		key = &spec_ipv4->hdr.src_addr;
		mask = &mask_ipv4->hdr.src_addr;
		size = sizeof(rte_be32_t);

		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV4,
				NH_FLD_IPV4_SRC_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV4,
				NH_FLD_IPV4_SRC_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask_ipv4->hdr.dst_addr) {
		key = &spec_ipv4->hdr.dst_addr;
		mask = &mask_ipv4->hdr.dst_addr;
		size = sizeof(rte_be32_t);

		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV4,
				NH_FLD_IPV4_DST_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;
		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV4,
				NH_FLD_IPV4_DST_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask_ipv4->hdr.next_proto_id) {
		key = &spec_ipv4->hdr.next_proto_id;
		mask = &mask_ipv4->hdr.next_proto_id;
		size = sizeof(uint8_t);

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IP,
				NH_FLD_IP_PROTO, key,
				mask, size, priv, group,
				&local_cfg,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IP,
				NH_FLD_IP_PROTO, key,
				mask, size, priv, group,
				&local_cfg,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;
	return 0;
}

static int
dpaa2_configure_flow_ipv6(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_ipv6 *spec_ipv6 = 0, *mask_ipv6 = 0;
	const void *key, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const char zero_cmp[NH_FLD_IPV6_ADDR_SIZE] = {0};
	int size;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec_ipv6 = pattern->spec;
	mask_ipv6 = pattern->mask ? pattern->mask : &dpaa2_flow_item_ipv6_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		if (spec_ipv6) {
			DPAA2_PMD_ERR("Tunnel-IPv6 distribution not support");
			return -ENOTSUP;
		}

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_IPV6_FRAM,
				DPAA2_FLOW_QOS_TYPE, group,
				&local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_IPV6_FRAM,
				DPAA2_FLOW_FS_TYPE, group,
				&local_cfg);
		return ret;
	}

	ret = dpaa2_flow_identify_by_faf(priv, flow, FAF_IPV6_FRAM,
			DPAA2_FLOW_QOS_TYPE, group,
			&local_cfg);
	if (ret)
		return ret;

	ret = dpaa2_flow_identify_by_faf(priv, flow, FAF_IPV6_FRAM,
			DPAA2_FLOW_FS_TYPE, group, &local_cfg);
	if (ret)
		return ret;

	if (!spec_ipv6) {
		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask_ipv6,
			RTE_FLOW_ITEM_TYPE_IPV6);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of IPv6 not support.");
		return ret;
	}

	if (memcmp((const char *)&mask_ipv6->hdr.src_addr, zero_cmp, NH_FLD_IPV6_ADDR_SIZE)) {
		key = &spec_ipv6->hdr.src_addr;
		mask = &mask_ipv6->hdr.src_addr;
		size = NH_FLD_IPV6_ADDR_SIZE;

		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV6,
				NH_FLD_IPV6_SRC_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV6,
				NH_FLD_IPV6_SRC_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (memcmp((const char *)&mask_ipv6->hdr.dst_addr, zero_cmp, NH_FLD_IPV6_ADDR_SIZE)) {
		key = &spec_ipv6->hdr.dst_addr;
		mask = &mask_ipv6->hdr.dst_addr;
		size = NH_FLD_IPV6_ADDR_SIZE;

		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV6,
				NH_FLD_IPV6_DST_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_ipaddr_extract_rule(flow, NET_PROT_IPV6,
				NH_FLD_IPV6_DST_IP,
				key, mask, size, priv,
				group, &local_cfg,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask_ipv6->hdr.proto) {
		key = &spec_ipv6->hdr.proto;
		mask = &mask_ipv6->hdr.proto;
		size = sizeof(uint8_t);

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IP,
				NH_FLD_IP_PROTO, key,
				mask, size, priv, group,
				&local_cfg,
				DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IP,
				NH_FLD_IP_PROTO, key,
				mask, size, priv, group,
				&local_cfg,
				DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;
	return 0;
}

static int
dpaa2_configure_flow_icmp(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_icmp *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_icmp_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-ICMP distribution not support");
		return -ENOTSUP;
	}

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_ICMP_FRAM, DPAA2_FLOW_QOS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_ICMP_FRAM, DPAA2_FLOW_FS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_ICMP);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of ICMP not support.");

		return ret;
	}

	if (mask->hdr.icmp_type) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ICMP,
			NH_FLD_ICMP_TYPE, &spec->hdr.icmp_type,
			&mask->hdr.icmp_type, sizeof(uint8_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ICMP,
			NH_FLD_ICMP_TYPE, &spec->hdr.icmp_type,
			&mask->hdr.icmp_type, sizeof(uint8_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask->hdr.icmp_code) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ICMP,
			NH_FLD_ICMP_CODE, &spec->hdr.icmp_code,
			&mask->hdr.icmp_code, sizeof(uint8_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_ICMP,
			NH_FLD_ICMP_CODE, &spec->hdr.icmp_code,
			&mask->hdr.icmp_code, sizeof(uint8_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_udp(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_udp *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_udp_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		if (spec) {
			DPAA2_PMD_ERR("Tunnel-UDP distribution not support");
			return -ENOTSUP;
		}

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_UDP_FRAM,
				DPAA2_FLOW_QOS_TYPE, group,
				&local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_UDP_FRAM,
				DPAA2_FLOW_FS_TYPE, group,
				&local_cfg);
		return ret;
	}

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_UDP_FRAM, DPAA2_FLOW_QOS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_UDP_FRAM, DPAA2_FLOW_FS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	if (!spec) {
		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_UDP);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of UDP not support.");

		return ret;
	}

	if (mask->hdr.src_port) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_UDP,
			NH_FLD_UDP_PORT_SRC, &spec->hdr.src_port,
			&mask->hdr.src_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_UDP,
			NH_FLD_UDP_PORT_SRC, &spec->hdr.src_port,
			&mask->hdr.src_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask->hdr.dst_port) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_UDP,
			NH_FLD_UDP_PORT_DST, &spec->hdr.dst_port,
			&mask->hdr.dst_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_UDP,
			NH_FLD_UDP_PORT_DST, &spec->hdr.dst_port,
			&mask->hdr.dst_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_tcp(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_tcp *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_tcp_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		if (spec) {
			DPAA2_PMD_ERR("Tunnel-TCP distribution not support");
			return -ENOTSUP;
		}

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAFE_VXLAN_IN_TCP_FRAM,
				DPAA2_FLOW_QOS_TYPE, group,
				&local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
						 FAFE_VXLAN_IN_TCP_FRAM,
						 DPAA2_FLOW_FS_TYPE, group,
						 &local_cfg);
		return ret;
	}

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_TCP_FRAM, DPAA2_FLOW_QOS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_TCP_FRAM, DPAA2_FLOW_FS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	if (!spec) {
		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_TCP);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of TCP not support.");

		return ret;
	}

	if (mask->hdr.src_port) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_TCP,
			NH_FLD_TCP_PORT_SRC, &spec->hdr.src_port,
			&mask->hdr.src_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_TCP,
			NH_FLD_TCP_PORT_SRC, &spec->hdr.src_port,
			&mask->hdr.src_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask->hdr.dst_port) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_TCP,
			NH_FLD_TCP_PORT_DST, &spec->hdr.dst_port,
			&mask->hdr.dst_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_TCP,
			NH_FLD_TCP_PORT_DST, &spec->hdr.dst_port,
			&mask->hdr.dst_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_esp(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_esp *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern =
		&dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_esp_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-ESP distribution not support");
		return -ENOTSUP;
	}

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_IPSEC_ESP_FRAM, DPAA2_FLOW_QOS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_IPSEC_ESP_FRAM, DPAA2_FLOW_FS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	if (!spec) {
		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_ESP);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of ESP not support.");

		return ret;
	}

	if (mask->hdr.spi) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IPSEC_ESP,
			NH_FLD_IPSEC_ESP_SPI, &spec->hdr.spi,
			&mask->hdr.spi, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IPSEC_ESP,
			NH_FLD_IPSEC_ESP_SPI, &spec->hdr.spi,
			&mask->hdr.spi, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask->hdr.seq) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IPSEC_ESP,
			NH_FLD_IPSEC_ESP_SEQUENCE_NUM, &spec->hdr.seq,
			&mask->hdr.seq, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IPSEC_ESP,
			NH_FLD_IPSEC_ESP_SEQUENCE_NUM, &spec->hdr.seq,
			&mask->hdr.seq, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_ah(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_ah *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern =
		&dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_ah_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-AH distribution not support");
		return -ENOTSUP;
	}

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_IPSEC_AH_FRAM, DPAA2_FLOW_QOS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_IPSEC_AH_FRAM, DPAA2_FLOW_FS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	if (!spec) {
		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_AH);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of AH not support.");

		return ret;
	}

	if (mask->spi) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IPSEC_AH,
			NH_FLD_IPSEC_AH_SPI, &spec->spi,
			&mask->spi, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_IPSEC_AH,
			NH_FLD_IPSEC_AH_SPI, &spec->spi,
			&mask->spi, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask->seq_num) {
		DPAA2_PMD_ERR("AH seq distribution not support");
		return -ENOTSUP;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_sctp(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_sctp *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_sctp_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-SCTP distribution not support");
		return -ENOTSUP;
	}

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_SCTP_FRAM, DPAA2_FLOW_QOS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAF_SCTP_FRAM, DPAA2_FLOW_FS_TYPE,
			group, &local_cfg);
	if (ret)
		return ret;

	if (!spec) {
		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_SCTP);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of SCTP not support.");

		return ret;
	}

	if (mask->hdr.src_port) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_SCTP,
			NH_FLD_SCTP_PORT_SRC, &spec->hdr.src_port,
			&mask->hdr.src_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_SCTP,
			NH_FLD_SCTP_PORT_SRC, &spec->hdr.src_port,
			&mask->hdr.src_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	if (mask->hdr.dst_port) {
		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_SCTP,
			NH_FLD_SCTP_PORT_DST, &spec->hdr.dst_port,
			&mask->hdr.dst_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_SCTP,
			NH_FLD_SCTP_PORT_DST, &spec->hdr.dst_port,
			&mask->hdr.dst_port, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_gre(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_gre *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_gre_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-GRE distribution not support");
		return -ENOTSUP;
	}

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_GRE_FRAM, DPAA2_FLOW_QOS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_GRE_FRAM, DPAA2_FLOW_FS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_GRE);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of GRE not support.");

		return ret;
	}

	if (!mask->protocol)
		return 0;

	ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_GRE,
			NH_FLD_GRE_TYPE, &spec->protocol,
			&mask->protocol, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
	if (ret)
		return ret;

	ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_GRE,
			NH_FLD_GRE_TYPE, &spec->protocol,
			&mask->protocol, sizeof(rte_be16_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
	if (ret)
		return ret;

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_vxlan(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_vxlan *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_vxlan_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-VXLAN distribution not support");
		return -ENOTSUP;
	}

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_VXLAN_FRAM, DPAA2_FLOW_QOS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_VXLAN_FRAM, DPAA2_FLOW_FS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_VXLAN);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of VXLAN not support.");

		return ret;
	}

	if (mask->flags) {
		if (spec->flags != VXLAN_HF_VNI) {
			DPAA2_PMD_ERR("vxlan flag(0x%02x) must be 0x%02x.",
				spec->flags, VXLAN_HF_VNI);
			return -EINVAL;
		}
		if (mask->flags != 0xff) {
			DPAA2_PMD_ERR("Not support to extract vxlan flag.");
			return -EINVAL;
		}
	}

	if (mask->vni[0] || mask->vni[1] || mask->vni[2]) {
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_VNI_OFFSET,
			sizeof(mask->vni), spec->vni,
			mask->vni,
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_pr_extract_rule(flow,
			DPAA2_VXLAN_VNI_OFFSET,
			sizeof(mask->vni), spec->vni,
			mask->vni,
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_ecpri(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_ecpri *spec, *mask;
	struct rte_flow_item_ecpri local_mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern =
		&dpaa2_pattern->generic_item;
	uint8_t extract_nb = 0, i;
	uint64_t rule_data[DPAA2_ECPRI_MAX_EXTRACT_NB];
	uint64_t mask_data[DPAA2_ECPRI_MAX_EXTRACT_NB];
	uint8_t extract_size[DPAA2_ECPRI_MAX_EXTRACT_NB];
	uint8_t extract_off[DPAA2_ECPRI_MAX_EXTRACT_NB];

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	if (pattern->mask) {
		memcpy(&local_mask, pattern->mask,
			sizeof(struct rte_flow_item_ecpri));
		local_mask.hdr.common.u32 =
			rte_be_to_cpu_32(local_mask.hdr.common.u32);
		mask = &local_mask;
	} else {
		mask = &dpaa2_flow_item_ecpri_mask;
	}

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-ECPRI distribution not support");
		return -ENOTSUP;
	}

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAFE_ECPRI_FRAM, DPAA2_FLOW_QOS_TYPE,
			group, &local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
			FAFE_ECPRI_FRAM, DPAA2_FLOW_FS_TYPE,
			group, &local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_ECPRI);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of ECPRI not support.");

		return ret;
	}

	if (mask->hdr.common.type != 0xff) {
		DPAA2_PMD_WARN("ECPRI header type not specified.");

		return -EINVAL;
	}

	if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_IQ_DATA) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_0;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type0.pc_id) {
			rule_data[extract_nb] = spec->hdr.type0.pc_id;
			mask_data[extract_nb] = mask->hdr.type0.pc_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_iq_data, pc_id);
			extract_nb++;
		}
		if (mask->hdr.type0.seq_id) {
			rule_data[extract_nb] = spec->hdr.type0.seq_id;
			mask_data[extract_nb] = mask->hdr.type0.seq_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_iq_data, seq_id);
			extract_nb++;
		}
	} else if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_BIT_SEQ) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_1;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type1.pc_id) {
			rule_data[extract_nb] = spec->hdr.type1.pc_id;
			mask_data[extract_nb] = mask->hdr.type1.pc_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_bit_seq, pc_id);
			extract_nb++;
		}
		if (mask->hdr.type1.seq_id) {
			rule_data[extract_nb] = spec->hdr.type1.seq_id;
			mask_data[extract_nb] = mask->hdr.type1.seq_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_bit_seq, seq_id);
			extract_nb++;
		}
	} else if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_RTC_CTRL) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_2;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type2.rtc_id) {
			rule_data[extract_nb] = spec->hdr.type2.rtc_id;
			mask_data[extract_nb] = mask->hdr.type2.rtc_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_rtc_ctrl, rtc_id);
			extract_nb++;
		}
		if (mask->hdr.type2.seq_id) {
			rule_data[extract_nb] = spec->hdr.type2.seq_id;
			mask_data[extract_nb] = mask->hdr.type2.seq_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_rtc_ctrl, seq_id);
			extract_nb++;
		}
	} else if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_GEN_DATA) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_3;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type3.pc_id || mask->hdr.type3.seq_id)
			DPAA2_PMD_WARN("Extract type3 msg not support.");
	} else if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_RM_ACC) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_4;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type4.rma_id) {
			rule_data[extract_nb] = spec->hdr.type4.rma_id;
			mask_data[extract_nb] = mask->hdr.type4.rma_id;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET + 0;
				/** Compiler not support to take address
				 * of bit-field
				 * offsetof(struct rte_ecpri_msg_rm_access,
				 * rma_id);
				 */
			extract_nb++;
		}
		if (mask->hdr.type4.ele_id) {
			rule_data[extract_nb] = spec->hdr.type4.ele_id;
			mask_data[extract_nb] = mask->hdr.type4.ele_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET + 2;
				/** Compiler not support to take address
				 * of bit-field
				 * offsetof(struct rte_ecpri_msg_rm_access,
				 * ele_id);
				 */
			extract_nb++;
		}
	} else if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_DLY_MSR) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_5;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type5.msr_id) {
			rule_data[extract_nb] = spec->hdr.type5.msr_id;
			mask_data[extract_nb] = mask->hdr.type5.msr_id;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_delay_measure,
					msr_id);
			extract_nb++;
		}
		if (mask->hdr.type5.act_type) {
			rule_data[extract_nb] = spec->hdr.type5.act_type;
			mask_data[extract_nb] = mask->hdr.type5.act_type;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_delay_measure,
					act_type);
			extract_nb++;
		}
	} else if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_RMT_RST) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_6;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type6.rst_id) {
			rule_data[extract_nb] = spec->hdr.type6.rst_id;
			mask_data[extract_nb] = mask->hdr.type6.rst_id;
			extract_size[extract_nb] = sizeof(rte_be16_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_remote_reset,
					rst_id);
			extract_nb++;
		}
		if (mask->hdr.type6.rst_op) {
			rule_data[extract_nb] = spec->hdr.type6.rst_op;
			mask_data[extract_nb] = mask->hdr.type6.rst_op;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_remote_reset,
					rst_op);
			extract_nb++;
		}
	} else if (spec->hdr.common.type == RTE_ECPRI_MSG_TYPE_EVT_IND) {
		rule_data[extract_nb] = ECPRI_FAFE_TYPE_7;
		mask_data[extract_nb] = 0xff;
		extract_size[extract_nb] = sizeof(uint8_t);
		extract_off[extract_nb] = DPAA2_FAFE_PSR_OFFSET;
		extract_nb++;

		if (mask->hdr.type7.evt_id) {
			rule_data[extract_nb] = spec->hdr.type7.evt_id;
			mask_data[extract_nb] = mask->hdr.type7.evt_id;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_event_ind,
					evt_id);
			extract_nb++;
		}
		if (mask->hdr.type7.evt_type) {
			rule_data[extract_nb] = spec->hdr.type7.evt_type;
			mask_data[extract_nb] = mask->hdr.type7.evt_type;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_event_ind,
					evt_type);
			extract_nb++;
		}
		if (mask->hdr.type7.seq) {
			rule_data[extract_nb] = spec->hdr.type7.seq;
			mask_data[extract_nb] = mask->hdr.type7.seq;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_event_ind,
					seq);
			extract_nb++;
		}
		if (mask->hdr.type7.number) {
			rule_data[extract_nb] = spec->hdr.type7.number;
			mask_data[extract_nb] = mask->hdr.type7.number;
			extract_size[extract_nb] = sizeof(uint8_t);
			extract_off[extract_nb] =
				DPAA2_ECPRI_MSG_OFFSET +
				offsetof(struct rte_ecpri_msg_event_ind,
					number);
			extract_nb++;
		}
	} else {
		DPAA2_PMD_ERR("Invalid ecpri header type(%d)",
				spec->hdr.common.type);
		return -EINVAL;
	}

	for (i = 0; i < extract_nb; i++) {
		ret = dpaa2_flow_add_pr_extract_rule(flow,
			extract_off[i],
			extract_size[i], &rule_data[i], &mask_data[i],
			priv, group,
			device_configured,
			DPAA2_FLOW_QOS_TYPE);
		if (ret)
			return ret;

		ret = dpaa2_flow_add_pr_extract_rule(flow,
			extract_off[i],
			extract_size[i], &rule_data[i], &mask_data[i],
			priv, group,
			device_configured,
			DPAA2_FLOW_FS_TYPE);
		if (ret)
			return ret;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_gtp(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	int ret, local_cfg = 0;
	uint32_t group;
	const struct rte_flow_item_gtp *spec, *mask;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	const struct rte_flow_item *pattern =
		&dpaa2_pattern->generic_item;

	group = attr->group;

	/* Parse pattern list to get the matching parameters */
	spec = pattern->spec;
	mask = pattern->mask ?
		pattern->mask : &dpaa2_flow_item_gtp_mask;

	/* Get traffic class index and flow id to be configured */
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	if (dpaa2_pattern->in_tunnel) {
		DPAA2_PMD_ERR("Tunnel-GTP distribution not support");
		return -ENOTSUP;
	}

	if (!spec) {
		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_GTP_FRAM, DPAA2_FLOW_QOS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		ret = dpaa2_flow_identify_by_faf(priv, flow,
				FAF_GTP_FRAM, DPAA2_FLOW_FS_TYPE,
				group, &local_cfg);
		if (ret)
			return ret;

		(*device_configured) |= local_cfg;
		return 0;
	}

	ret = dpaa2_flow_extract_support((const uint8_t *)mask,
		RTE_FLOW_ITEM_TYPE_GTP);
	if (ret) {
		DPAA2_PMD_WARN("Extract field(s) of GTP not support.");

		return ret;
	}

	if (!mask->teid)
		return 0;

	ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_GTP,
			NH_FLD_GTP_TEID, &spec->teid,
			&mask->teid, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_QOS_TYPE);
	if (ret)
		return ret;

	ret = dpaa2_flow_add_hdr_extract_rule(flow, NET_PROT_GTP,
			NH_FLD_GTP_TEID, &spec->teid,
			&mask->teid, sizeof(rte_be32_t),
			priv, group, &local_cfg, DPAA2_FLOW_FS_TYPE);
	if (ret)
		return ret;

	(*device_configured) |= local_cfg;

	return 0;
}

static int
dpaa2_configure_flow_raw(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_dpaa2_flow_item *dpaa2_pattern,
	const struct rte_flow_action actions[] __rte_unused,
	struct rte_flow_error *error __rte_unused,
	int *device_configured)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	int local_cfg = 0, ret;
	uint32_t group;
	struct dpaa2_key_extract *qos_key_extract;
	struct dpaa2_key_extract *tc_key_extract;
	const struct rte_flow_item *pattern = &dpaa2_pattern->generic_item;
	const struct rte_flow_item_raw *spec = pattern->spec;
	const struct rte_flow_item_raw *mask = pattern->mask;

	/* Need both spec and mask */
	if (!spec || !mask) {
		DPAA2_PMD_ERR("spec or mask not present.");
		return -EINVAL;
	}

	if (spec->relative) {
		/* TBD: relative offset support.
		 * To support relative offset of previous L3 protocol item,
		 * extracts should be expanded to identify if the frame is:
		 * vlan or none-vlan.
		 *
		 * To support relative offset of previous L4 protocol item,
		 * extracts should be expanded to identify if the frame is:
		 * vlan/IPv4 or vlan/IPv6 or none-vlan/IPv4 or none-vlan/IPv6.
		 */
		DPAA2_PMD_ERR("relative not supported.");
		return -EINVAL;
	}

	if (spec->search) {
		DPAA2_PMD_ERR("search not supported.");
		return -EINVAL;
	}

	/* Spec len and mask len should be same */
	if (spec->length != mask->length) {
		DPAA2_PMD_ERR("Spec len and mask len mismatch.");
		return -EINVAL;
	}

	/* Get traffic class index and flow id to be configured */
	group = attr->group;
	flow->tc_id = group;
	flow->tc_index = attr->priority;

	qos_key_extract = &priv->extract.qos_key_extract;
	tc_key_extract = &priv->extract.tc_key_extract[group];

	ret = dpaa2_flow_extract_add_raw(priv,
			spec->offset, spec->length,
			DPAA2_FLOW_QOS_TYPE, 0, &local_cfg);
	if (ret) {
		DPAA2_PMD_ERR("QoS Extract RAW add failed.");
		return -EINVAL;
	}

	ret = dpaa2_flow_extract_add_raw(priv,
			spec->offset, spec->length,
			DPAA2_FLOW_FS_TYPE, group, &local_cfg);
	if (ret) {
		DPAA2_PMD_ERR("FS[%d] Extract RAW add failed.",
			group);
		return -EINVAL;
	}

	ret = dpaa2_flow_raw_rule_data_set(flow,
			&qos_key_extract->key_profile,
			spec->offset, spec->length,
			spec->pattern, mask->pattern,
			DPAA2_FLOW_QOS_TYPE);
	if (ret) {
		DPAA2_PMD_ERR("QoS RAW rule data set failed");
		return -EINVAL;
	}

	ret = dpaa2_flow_raw_rule_data_set(flow,
			&tc_key_extract->key_profile,
			spec->offset, spec->length,
			spec->pattern, mask->pattern,
			DPAA2_FLOW_FS_TYPE);
	if (ret) {
		DPAA2_PMD_ERR("FS RAW rule data set failed");
		return -EINVAL;
	}

	(*device_configured) |= local_cfg;

	return 0;
}

static inline int
dpaa2_flow_verify_attr(struct dpaa2_dev_priv *priv,
	const struct rte_flow_attr *attr)
{
	struct dpaa2_dev_flow *curr = LIST_FIRST(&priv->flows);

	while (curr) {
		if (curr->tc_id == attr->group &&
			curr->tc_index == attr->priority) {
			DPAA2_PMD_ERR("Flow(TC[%d].entry[%d] exists",
				attr->group, attr->priority);

			return -EINVAL;
		}
		curr = LIST_NEXT(curr, next);
	}

	return 0;
}

static inline struct rte_eth_dev *
dpaa2_flow_redirect_dev(struct dpaa2_dev_priv *priv,
	const struct rte_flow_action *action)
{
	const struct rte_flow_action_port_id *port_id;
	const struct rte_flow_action_ethdev *ethdev;
	int idx = -1;
	struct rte_eth_dev *dest_dev;

	if (action->type == RTE_FLOW_ACTION_TYPE_PORT_ID) {
		port_id = action->conf;
		if (!port_id->original)
			idx = port_id->id;
	} else if (action->type == RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT) {
		ethdev = action->conf;
		idx = ethdev->port_id;
	} else {
		return NULL;
	}

	if (idx >= 0) {
		if (!rte_eth_dev_is_valid_port(idx))
			return NULL;
		if (!rte_pmd_dpaa2_dev_is_dpaa2(idx))
			return NULL;
		dest_dev = &rte_eth_devices[idx];
	} else {
		dest_dev = priv->eth_dev;
	}

	return dest_dev;
}

static inline int
dpaa2_flow_verify_action(struct dpaa2_dev_priv *priv,
	const struct rte_flow_attr *attr,
	const struct rte_flow_action actions[])
{
	int end_of_list = 0, i, j = 0;
	const struct rte_flow_action_queue *dest_queue;
	const struct rte_flow_action_rss *rss_conf;
	struct dpaa2_queue *rxq;

	while (!end_of_list) {
		switch (actions[j].type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			dest_queue = actions[j].conf;
			rxq = priv->rx_vq[dest_queue->index];
			if (attr->group != rxq->tc_index) {
				DPAA2_PMD_ERR("FSQ(%d.%d) not in TC[%d]",
					rxq->tc_index, rxq->flow_id,
					attr->group);

				return -ENOTSUP;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			if (!dpaa2_flow_redirect_dev(priv, &actions[j])) {
				DPAA2_PMD_ERR("Invalid port id of action");
				return -ENOTSUP;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss_conf = (const struct rte_flow_action_rss *)
					(actions[j].conf);
			if (rss_conf->queue_num > priv->dist_queues) {
				DPAA2_PMD_ERR("RSS number too large");
				return -ENOTSUP;
			}
			for (i = 0; i < (int)rss_conf->queue_num; i++) {
				if (rss_conf->queue[i] >= priv->nb_rx_queues) {
					DPAA2_PMD_ERR("RSS queue not in range");
					return -ENOTSUP;
				}
				rxq = priv->rx_vq[rss_conf->queue[i]];
				if (rxq->tc_index != attr->group) {
					DPAA2_PMD_ERR("RSS queue not in group");
					return -ENOTSUP;
				}
			}

			break;
		case RTE_FLOW_ACTION_TYPE_PF:
			/* Skip this action, have to add for vxlan */
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			end_of_list = 1;
			break;
		default:
			DPAA2_PMD_ERR("Invalid action type");
			return -ENOTSUP;
		}
		j++;
	}

	return 0;
}

static int
dpaa2_configure_flow_fs_action(struct dpaa2_dev_priv *priv,
	struct dpaa2_dev_flow *flow,
	const struct rte_flow_action *rte_action)
{
	struct rte_eth_dev *dest_dev;
	struct dpaa2_dev_priv *dest_priv;
	const struct rte_flow_action_queue *dest_queue;
	struct dpaa2_queue *dest_q;

	memset(&flow->fs_action_cfg, 0,
		sizeof(struct dpni_fs_action_cfg));
	flow->action_type = rte_action->type;

	if (flow->action_type == RTE_FLOW_ACTION_TYPE_QUEUE) {
		dest_queue = rte_action->conf;
		dest_q = priv->rx_vq[dest_queue->index];
		flow->fs_action_cfg.flow_id = dest_q->flow_id;
	} else if (flow->action_type == RTE_FLOW_ACTION_TYPE_PORT_ID ||
		   flow->action_type == RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT) {
		dest_dev = dpaa2_flow_redirect_dev(priv, rte_action);
		if (!dest_dev) {
			DPAA2_PMD_ERR("Invalid device to redirect");
			return -EINVAL;
		}

		dest_priv = dest_dev->data->dev_private;
		dest_q = dest_priv->tx_vq[0];
		flow->fs_action_cfg.options =
			DPNI_FS_OPT_REDIRECT_TO_DPNI_TX;
		flow->fs_action_cfg.redirect_obj_token =
			dest_priv->token;
		flow->fs_action_cfg.flow_id = dest_q->flow_id;
	}

	return 0;
}

static inline uint16_t
dpaa2_flow_entry_size(uint16_t key_max_size)
{
	if (key_max_size > DPAA2_FLOW_ENTRY_MAX_SIZE) {
		DPAA2_PMD_ERR("Key size(%d) > max(%d)",
			key_max_size,
			DPAA2_FLOW_ENTRY_MAX_SIZE);

		return 0;
	}

	if (key_max_size > DPAA2_FLOW_ENTRY_MIN_SIZE)
		return DPAA2_FLOW_ENTRY_MAX_SIZE;

	/* Current MC only support fixed entry size(56)*/
	return DPAA2_FLOW_ENTRY_MAX_SIZE;
}

static inline int
dpaa2_flow_clear_fs_table(struct dpaa2_dev_priv *priv,
	uint8_t tc_id)
{
	struct dpaa2_dev_flow *curr = LIST_FIRST(&priv->flows);
	int need_clear = 0, ret;
	struct fsl_mc_io *dpni = priv->hw;

	while (curr) {
		if (curr->tc_id == tc_id) {
			need_clear = 1;
			break;
		}
		curr = LIST_NEXT(curr, next);
	}

	if (need_clear) {
		ret = dpni_clear_fs_entries(dpni, CMD_PRI_LOW,
				priv->token, tc_id);
		if (ret) {
			DPAA2_PMD_ERR("TC[%d] clear failed", tc_id);
			return ret;
		}
	}

	return 0;
}

static int
dpaa2_configure_fs_rss_table(struct dpaa2_dev_priv *priv,
	uint8_t tc_id, uint16_t dist_size, int rss_dist)
{
	struct dpaa2_key_extract *tc_extract;
	uint8_t *key_cfg_buf;
	uint64_t key_cfg_iova;
	int ret;
	struct dpni_rx_dist_cfg tc_cfg;
	struct fsl_mc_io *dpni = priv->hw;
	uint16_t entry_size;
	uint16_t key_max_size;

	ret = dpaa2_flow_clear_fs_table(priv, tc_id);
	if (ret < 0) {
		DPAA2_PMD_ERR("TC[%d] clear failed", tc_id);
		return ret;
	}

	tc_extract = &priv->extract.tc_key_extract[tc_id];
	key_cfg_buf = priv->extract.tc_extract_param[tc_id];
	key_cfg_iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(key_cfg_buf,
		DPAA2_EXTRACT_PARAM_MAX_SIZE);
	if (key_cfg_iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU map for key cfg(%p)",
			__func__, key_cfg_buf);

		return -ENOBUFS;
	}

	key_max_size = tc_extract->key_profile.key_max_size;
	entry_size = dpaa2_flow_entry_size(key_max_size);

	dpaa2_flow_fs_extracts_log(priv, tc_id);
	ret = dpkg_prepare_key_cfg(&tc_extract->dpkg,
			key_cfg_buf);
	if (ret < 0) {
		DPAA2_PMD_ERR("TC[%d] prepare key failed", tc_id);
		return ret;
	}

	memset(&tc_cfg, 0, sizeof(struct dpni_rx_dist_cfg));
	tc_cfg.dist_size = dist_size;
	tc_cfg.key_cfg_iova = key_cfg_iova;
	if (rss_dist)
		tc_cfg.enable = true;
	else
		tc_cfg.enable = false;
	tc_cfg.tc = tc_id;
	ret = dpni_set_rx_hash_dist(dpni, CMD_PRI_LOW,
			priv->token, &tc_cfg);
	if (ret < 0) {
		if (rss_dist) {
			DPAA2_PMD_ERR("RSS TC[%d] set failed",
				tc_id);
		} else {
			DPAA2_PMD_ERR("FS TC[%d] hash disable failed",
				tc_id);
		}

		return ret;
	}

	if (rss_dist)
		return 0;

	tc_cfg.enable = true;
	tc_cfg.fs_miss_flow_id = dpaa2_flow_miss_flow_id;
	ret = dpni_set_rx_fs_dist(dpni, CMD_PRI_LOW,
			priv->token, &tc_cfg);
	if (ret < 0) {
		DPAA2_PMD_ERR("TC[%d] FS configured failed", tc_id);
		return ret;
	}

	ret = dpaa2_flow_rule_add_all(priv, DPAA2_FLOW_FS_TYPE,
			entry_size, tc_id);
	if (ret)
		return ret;

	return 0;
}

static int
dpaa2_configure_qos_table(struct dpaa2_dev_priv *priv,
	int rss_dist)
{
	struct dpaa2_key_extract *qos_extract;
	uint8_t *key_cfg_buf;
	uint64_t key_cfg_iova;
	int ret;
	struct dpni_qos_tbl_cfg qos_cfg;
	struct fsl_mc_io *dpni = priv->hw;
	uint16_t entry_size;
	uint16_t key_max_size;

	if (!rss_dist && priv->num_rx_tc <= 1) {
		/* QoS table is effecitive for FS multiple TCs or RSS.*/
		return 0;
	}

	if (LIST_FIRST(&priv->flows)) {
		ret = dpni_clear_qos_table(dpni, CMD_PRI_LOW,
				priv->token);
		if (ret < 0) {
			DPAA2_PMD_ERR("QoS table clear failed");
			return ret;
		}
	}

	qos_extract = &priv->extract.qos_key_extract;
	key_cfg_buf = priv->extract.qos_extract_param;
	key_cfg_iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(key_cfg_buf,
		DPAA2_EXTRACT_PARAM_MAX_SIZE);
	if (key_cfg_iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU map for key cfg(%p)",
			__func__, key_cfg_buf);

		return -ENOBUFS;
	}

	key_max_size = qos_extract->key_profile.key_max_size;
	entry_size = dpaa2_flow_entry_size(key_max_size);

	dpaa2_flow_qos_extracts_log(priv);

	ret = dpkg_prepare_key_cfg(&qos_extract->dpkg,
			key_cfg_buf);
	if (ret < 0) {
		DPAA2_PMD_ERR("QoS prepare extract failed");
		return ret;
	}
	memset(&qos_cfg, 0, sizeof(struct dpni_qos_tbl_cfg));
	qos_cfg.keep_entries = true;
	qos_cfg.key_cfg_iova = key_cfg_iova;
	if (rss_dist) {
		qos_cfg.discard_on_miss = true;
	} else {
		qos_cfg.discard_on_miss = false;
		qos_cfg.default_tc = 0;
	}

	ret = dpni_set_qos_table(dpni, CMD_PRI_LOW,
			priv->token, &qos_cfg);
	if (ret < 0) {
		DPAA2_PMD_ERR("QoS table set failed");
		return ret;
	}

	ret = dpaa2_flow_rule_add_all(priv, DPAA2_FLOW_QOS_TYPE,
			entry_size, 0);
	if (ret)
		return ret;

	return 0;
}

static int
dpaa2_flow_item_convert(const struct rte_flow_item pattern[],
			struct rte_dpaa2_flow_item **dpaa2_pattern)
{
	struct rte_dpaa2_flow_item *new_pattern;
	int num = 0, tunnel_start = 0;

	while (1) {
		num++;
		if (pattern[num].type == RTE_FLOW_ITEM_TYPE_END)
			break;
	}

	new_pattern = rte_malloc(NULL, sizeof(struct rte_dpaa2_flow_item) * num,
				 RTE_CACHE_LINE_SIZE);
	if (!new_pattern) {
		DPAA2_PMD_ERR("Failed to alloc %d flow items", num);
		return -ENOMEM;
	}

	num = 0;
	while (pattern[num].type != RTE_FLOW_ITEM_TYPE_END) {
		memcpy(&new_pattern[num].generic_item, &pattern[num],
		       sizeof(struct rte_flow_item));
		new_pattern[num].in_tunnel = 0;

		if (pattern[num].type == RTE_FLOW_ITEM_TYPE_VXLAN)
			tunnel_start = 1;
		else if (tunnel_start)
			new_pattern[num].in_tunnel = 1;
		num++;
	}

	new_pattern[num].generic_item.type = RTE_FLOW_ITEM_TYPE_END;
	*dpaa2_pattern = new_pattern;

	return 0;
}

static int
dpaa2_generic_flow_set(struct dpaa2_dev_flow *flow,
	struct rte_eth_dev *dev,
	const struct rte_flow_attr *attr,
	const struct rte_flow_item pattern[],
	const struct rte_flow_action actions[],
	struct rte_flow_error *error)
{
	const struct rte_flow_action_rss *rss_conf;
	int is_keycfg_configured = 0, end_of_list = 0;
	int ret = 0, i = 0, j = 0;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_dev_flow *curr = LIST_FIRST(&priv->flows);
	uint16_t dist_size, key_size;
	struct dpaa2_key_extract *qos_key_extract;
	struct dpaa2_key_extract *tc_key_extract;
	struct rte_dpaa2_flow_item *dpaa2_pattern = NULL;

	ret = dpaa2_flow_verify_attr(priv, attr);
	if (ret)
		return ret;

	ret = dpaa2_flow_verify_action(priv, attr, actions);
	if (ret)
		return ret;

	ret = dpaa2_flow_item_convert(pattern, &dpaa2_pattern);
	if (ret)
		return ret;

	/* Parse pattern list to get the matching parameters */
	while (!end_of_list) {
		switch (pattern[i].type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = dpaa2_configure_flow_eth(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("ETH flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = dpaa2_configure_flow_vlan(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("vLan flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = dpaa2_configure_flow_ipv4(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("IPV4 flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = dpaa2_configure_flow_ipv6(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("IPV6 flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
			ret = dpaa2_configure_flow_icmp(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("ICMP flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = dpaa2_configure_flow_udp(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("UDP flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = dpaa2_configure_flow_tcp(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("TCP flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			ret = dpaa2_configure_flow_sctp(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("SCTP flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			ret = dpaa2_configure_flow_esp(flow,
					dev, attr, &dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("ESP flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_AH:
			ret = dpaa2_configure_flow_ah(flow,
					dev, attr, &dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("AH flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			ret = dpaa2_configure_flow_gre(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("GRE flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = dpaa2_configure_flow_vxlan(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("VXLAN flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_ECPRI:
			ret = dpaa2_configure_flow_ecpri(flow,
					dev, attr, &dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("ECPRI flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			ret = dpaa2_configure_flow_gtp(flow,
					dev, attr, &dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("GTP flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_RAW:
			ret = dpaa2_configure_flow_raw(flow, dev, attr,
					&dpaa2_pattern[i],
					actions, error,
					&is_keycfg_configured);
			if (ret) {
				DPAA2_PMD_ERR("RAW flow config failed!");
				goto end_flow_set;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_END:
			end_of_list = 1;
			break; /*End of List*/
		default:
			DPAA2_PMD_ERR("Invalid flow item[%d] type(%d)",
				i, pattern[i].type);
			ret = -ENOTSUP;
			break;
		}
		i++;
	}

	qos_key_extract = &priv->extract.qos_key_extract;
	key_size = qos_key_extract->key_profile.key_max_size;
	flow->qos_rule.key_size = dpaa2_flow_entry_size(key_size);

	tc_key_extract = &priv->extract.tc_key_extract[flow->tc_id];
	key_size = tc_key_extract->key_profile.key_max_size;
	flow->fs_rule.key_size = dpaa2_flow_entry_size(key_size);

	/* Let's parse action on matching traffic */
	end_of_list = 0;
	while (!end_of_list) {
		switch (actions[j].type) {
		case RTE_FLOW_ACTION_TYPE_QUEUE:
		case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			ret = dpaa2_configure_flow_fs_action(priv, flow,
							     &actions[j]);
			if (ret)
				goto end_flow_set;

			/* Configure FS table first*/
			dist_size = priv->nb_rx_queues / priv->num_rx_tc;
			if (is_keycfg_configured & DPAA2_FLOW_FS_TYPE) {
				ret = dpaa2_configure_fs_rss_table(priv,
								   flow->tc_id,
								   dist_size,
								   false);
				if (ret)
					goto end_flow_set;
			}

			/* Configure QoS table then.*/
			if (is_keycfg_configured & DPAA2_FLOW_QOS_TYPE) {
				ret = dpaa2_configure_qos_table(priv, false);
				if (ret)
					goto end_flow_set;
			}

			if (priv->num_rx_tc > 1) {
				ret = dpaa2_flow_add_qos_rule(priv, flow);
				if (ret)
					goto end_flow_set;
			}

			if (flow->tc_index >= priv->fs_entries) {
				DPAA2_PMD_ERR("FS table with %d entries full",
					priv->fs_entries);
				return -1;
			}

			ret = dpaa2_flow_add_fs_rule(priv, flow);
			if (ret)
				goto end_flow_set;

			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			rss_conf = actions[j].conf;
			flow->action_type = RTE_FLOW_ACTION_TYPE_RSS;

			ret = dpaa2_distset_to_dpkg_profile_cfg(rss_conf->types,
					&tc_key_extract->dpkg);
			if (ret < 0) {
				DPAA2_PMD_ERR("TC[%d] distset RSS failed",
					      flow->tc_id);
				goto end_flow_set;
			}

			dist_size = rss_conf->queue_num;
			if (is_keycfg_configured & DPAA2_FLOW_FS_TYPE) {
				ret = dpaa2_configure_fs_rss_table(priv,
								   flow->tc_id,
								   dist_size,
								   true);
				if (ret)
					goto end_flow_set;
			}

			if (is_keycfg_configured & DPAA2_FLOW_QOS_TYPE) {
				ret = dpaa2_configure_qos_table(priv, true);
				if (ret)
					goto end_flow_set;
			}

			ret = dpaa2_flow_add_qos_rule(priv, flow);
			if (ret)
				goto end_flow_set;

			ret = dpaa2_flow_add_fs_rule(priv, flow);
			if (ret)
				goto end_flow_set;

			break;
		case RTE_FLOW_ACTION_TYPE_PF:
			/* Skip this action, have to add for vxlan */
			break;
		case RTE_FLOW_ACTION_TYPE_END:
			end_of_list = 1;
			break;
		default:
			DPAA2_PMD_ERR("Invalid action type");
			ret = -ENOTSUP;
			break;
		}
		j++;
	}

end_flow_set:
	if (!ret) {
		/* New rules are inserted. */
		if (!curr) {
			LIST_INSERT_HEAD(&priv->flows, flow, next);
		} else {
			while (LIST_NEXT(curr, next))
				curr = LIST_NEXT(curr, next);
			LIST_INSERT_AFTER(curr, flow, next);
		}
	}

	rte_free(dpaa2_pattern);

	return ret;
}

static inline int
dpaa2_dev_verify_attr(struct dpni_attr *dpni_attr,
	const struct rte_flow_attr *attr)
{
	int ret = 0;

	if (unlikely(attr->group >= dpni_attr->num_rx_tcs)) {
		DPAA2_PMD_ERR("Group/TC(%d) is out of range(%d)",
			attr->group, dpni_attr->num_rx_tcs);
		ret = -ENOTSUP;
	}
	if (unlikely(attr->priority >= dpni_attr->fs_entries)) {
		DPAA2_PMD_ERR("Priority(%d) within group is out of range(%d)",
			attr->priority, dpni_attr->fs_entries);
		ret = -ENOTSUP;
	}
	if (unlikely(attr->egress)) {
		DPAA2_PMD_ERR("Egress flow configuration is not supported");
		ret = -ENOTSUP;
	}
	if (unlikely(!attr->ingress)) {
		DPAA2_PMD_ERR("Ingress flag must be configured");
		ret = -EINVAL;
	}
	return ret;
}

static inline int
dpaa2_dev_verify_patterns(const struct rte_flow_item pattern[])
{
	unsigned int i, j, is_found = 0;
	int ret = 0;
	const enum rte_flow_item_type *hp_supported;
	const enum rte_flow_item_type *sp_supported;
	uint64_t hp_supported_num, sp_supported_num;

	hp_supported = dpaa2_hp_supported_pattern_type;
	hp_supported_num = RTE_DIM(dpaa2_hp_supported_pattern_type);

	sp_supported = dpaa2_sp_supported_pattern_type;
	sp_supported_num = RTE_DIM(dpaa2_sp_supported_pattern_type);

	for (j = 0; pattern[j].type != RTE_FLOW_ITEM_TYPE_END; j++) {
		is_found = 0;
		for (i = 0; i < hp_supported_num; i++) {
			if (hp_supported[i] == pattern[j].type) {
				is_found = 1;
				break;
			}
		}
		if (is_found)
			continue;
		if (dpaa2_sp_loaded > 0) {
			for (i = 0; i < sp_supported_num; i++) {
				if (sp_supported[i] == pattern[j].type) {
					is_found = 1;
					break;
				}
			}
		}
		if (!is_found) {
			DPAA2_PMD_WARN("Flow type(%d) not supported",
				pattern[j].type);
			ret = -ENOTSUP;
			break;
		}
	}

	return ret;
}

static inline int
dpaa2_dev_verify_actions(const struct rte_flow_action actions[])
{
	unsigned int i, j, is_found = 0;
	int ret = 0;

	for (j = 0; actions[j].type != RTE_FLOW_ACTION_TYPE_END; j++) {
		for (i = 0; i < RTE_DIM(dpaa2_supported_action_type); i++) {
			if (dpaa2_supported_action_type[i] == actions[j].type) {
				is_found = 1;
				break;
			}
		}
		if (!is_found) {
			ret = -ENOTSUP;
			break;
		}
	}
	for (j = 0; actions[j].type != RTE_FLOW_ACTION_TYPE_END; j++) {
		if (actions[j].type != RTE_FLOW_ACTION_TYPE_DROP &&
		    !actions[j].conf)
			ret = -EINVAL;
	}
	return ret;
}

static int
dpaa2_flow_validate(struct rte_eth_dev *dev,
	const struct rte_flow_attr *flow_attr,
	const struct rte_flow_item pattern[],
	const struct rte_flow_action actions[],
	struct rte_flow_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpni_attr dpni_attr;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	uint16_t token = priv->token;
	int ret = 0;

	memset(&dpni_attr, 0, sizeof(struct dpni_attr));
	ret = dpni_get_attributes(dpni, CMD_PRI_LOW, token, &dpni_attr);
	if (ret < 0) {
		DPAA2_PMD_ERR("Get dpni@%d attribute failed(%d)",
			priv->hw_id, ret);
		rte_flow_error_set(error, EPERM,
			RTE_FLOW_ERROR_TYPE_ATTR,
			flow_attr, "invalid");
		return ret;
	}

	/* Verify input attributes */
	ret = dpaa2_dev_verify_attr(&dpni_attr, flow_attr);
	if (ret < 0) {
		DPAA2_PMD_ERR("Invalid attributes are given");
		rte_flow_error_set(error, EPERM,
			RTE_FLOW_ERROR_TYPE_ATTR,
			flow_attr, "invalid");
		goto not_valid_params;
	}
	/* Verify input pattern list */
	ret = dpaa2_dev_verify_patterns(pattern);
	if (ret < 0) {
		DPAA2_PMD_ERR("Invalid pattern list is given");
		rte_flow_error_set(error, EPERM,
			RTE_FLOW_ERROR_TYPE_ITEM,
			pattern, "invalid");
		goto not_valid_params;
	}
	/* Verify input action list */
	ret = dpaa2_dev_verify_actions(actions);
	if (ret < 0) {
		DPAA2_PMD_ERR("Invalid action list is given");
		rte_flow_error_set(error, EPERM,
			RTE_FLOW_ERROR_TYPE_ACTION,
			actions, "invalid");
		goto not_valid_params;
	}
not_valid_params:
	return ret;
}

static struct rte_flow *
dpaa2_flow_create(struct rte_eth_dev *dev, const struct rte_flow_attr *attr,
		  const struct rte_flow_item pattern[],
		  const struct rte_flow_action actions[],
		  struct rte_flow_error *error)
{
	struct dpaa2_dev_flow *flow = NULL;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	int ret;
	uint64_t iova;

	dpaa2_flow_control_log =
		getenv("DPAA2_FLOW_CONTROL_LOG");

	if (getenv("DPAA2_FLOW_CONTROL_MISS_FLOW")) {
		dpaa2_flow_miss_flow_id =
			(uint16_t)atoi(getenv("DPAA2_FLOW_CONTROL_MISS_FLOW"));
		if (dpaa2_flow_miss_flow_id >= priv->dist_queues) {
			DPAA2_PMD_ERR("Missed flow ID %d >= dist size(%d)",
				      dpaa2_flow_miss_flow_id,
				      priv->dist_queues);
			return NULL;
		}
	}

	flow = rte_zmalloc(NULL, sizeof(struct dpaa2_dev_flow),
			   RTE_CACHE_LINE_SIZE);
	if (!flow) {
		DPAA2_PMD_ERR("Failure to allocate memory for flow");
		goto mem_failure;
	}

	/* Allocate DMA'ble memory to write the qos rules */
	flow->qos_key_addr = rte_zmalloc(NULL,
		DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE, RTE_CACHE_LINE_SIZE);
	if (!flow->qos_key_addr) {
		DPAA2_PMD_ERR("Memory allocation failed");
		goto mem_failure;
	}
	iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(flow->qos_key_addr,
			DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE);
	if (iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU map for qos key(%p)",
			__func__, flow->qos_key_addr);
		goto mem_failure;
	}
	flow->qos_rule.key_iova = iova;

	flow->qos_mask_addr = rte_zmalloc(NULL,
		DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE, RTE_CACHE_LINE_SIZE);
	if (!flow->qos_mask_addr) {
		DPAA2_PMD_ERR("Memory allocation failed");
		goto mem_failure;
	}
	iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(flow->qos_mask_addr,
			DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE);
	if (iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU map for qos mask(%p)",
			__func__, flow->qos_mask_addr);
		goto mem_failure;
	}
	flow->qos_rule.mask_iova = iova;

	/* Allocate DMA'ble memory to write the FS rules */
	flow->fs_key_addr = rte_zmalloc(NULL,
		DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE, RTE_CACHE_LINE_SIZE);
	if (!flow->fs_key_addr) {
		DPAA2_PMD_ERR("Memory allocation failed");
		goto mem_failure;
	}
	iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(flow->fs_key_addr,
			DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE);
	if (iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU map for fs key(%p)",
			__func__, flow->fs_key_addr);
		goto mem_failure;
	}
	flow->fs_rule.key_iova = iova;

	flow->fs_mask_addr = rte_zmalloc(NULL,
		DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE, RTE_CACHE_LINE_SIZE);
	if (!flow->fs_mask_addr) {
		DPAA2_PMD_ERR("Memory allocation failed");
		goto mem_failure;
	}
	iova = DPAA2_VADDR_TO_IOVA_AND_CHECK(flow->fs_mask_addr,
		DPAA2_EXTRACT_ALLOC_KEY_MAX_SIZE);
	if (iova == RTE_BAD_IOVA) {
		DPAA2_PMD_ERR("%s: No IOMMU map for fs mask(%p)",
			__func__, flow->fs_mask_addr);
		goto mem_failure;
	}
	flow->fs_rule.mask_iova = iova;

	priv->curr = flow;

	ret = dpaa2_generic_flow_set(flow, dev, attr, pattern, actions, error);
	if (ret < 0) {
		if (error && error->type > RTE_FLOW_ERROR_TYPE_ACTION)
			rte_flow_error_set(error, EPERM,
					   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					   attr, "unknown");
		DPAA2_PMD_ERR("Create flow failed (%d)", ret);
		goto creation_error;
	}

	priv->curr = NULL;
	return (struct rte_flow *)flow;

mem_failure:
	rte_flow_error_set(error, EPERM, RTE_FLOW_ERROR_TYPE_UNSPECIFIED, NULL,
			   "memory alloc");

creation_error:
	if (flow) {
		rte_free(flow->qos_key_addr);
		rte_free(flow->qos_mask_addr);
		rte_free(flow->fs_key_addr);
		rte_free(flow->fs_mask_addr);
		rte_free(flow);
	}
	priv->curr = NULL;

	return NULL;
}

static int
dpaa2_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *_flow,
		   struct rte_flow_error *error)
{
	int ret = 0;
	struct dpaa2_dev_flow *flow;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = priv->hw;

	flow = (struct dpaa2_dev_flow *)_flow;

	switch (flow->action_type) {
	case RTE_FLOW_ACTION_TYPE_QUEUE:
	case RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT:
	case RTE_FLOW_ACTION_TYPE_PORT_ID:
		if (priv->num_rx_tc > 1) {
			/* Remove entry from QoS table first */
			ret = dpni_remove_qos_entry(dpni, CMD_PRI_LOW,
						    priv->token,
						    &flow->qos_rule);
			if (ret < 0) {
				DPAA2_PMD_ERR("Remove FS QoS entry failed");
				dpaa2_flow_qos_entry_log("Delete failed", flow,
							 -1);
				abort();
				goto error;
			}
		}

		/* Then remove entry from FS table */
		ret = dpni_remove_fs_entry(dpni, CMD_PRI_LOW, priv->token,
					   flow->tc_id, &flow->fs_rule);
		if (ret < 0) {
			DPAA2_PMD_ERR("Remove entry from FS[%d] failed",
				      flow->tc_id);
			goto error;
		}
		break;
	case RTE_FLOW_ACTION_TYPE_RSS:
		if (priv->num_rx_tc > 1) {
			ret = dpni_remove_qos_entry(dpni, CMD_PRI_LOW,
						    priv->token,
						    &flow->qos_rule);
			if (ret < 0) {
				DPAA2_PMD_ERR("Remove RSS QoS entry failed");
				goto error;
			}
		}
		break;
	default:
		DPAA2_PMD_ERR("Action(%d) not supported", flow->action_type);
		ret = -ENOTSUP;
		break;
	}

	LIST_REMOVE(flow, next);
	rte_free(flow->qos_key_addr);
	rte_free(flow->qos_mask_addr);
	rte_free(flow->fs_key_addr);
	rte_free(flow->fs_mask_addr);
	/* Now free the flow */
	rte_free(flow);

error:
	if (ret)
		rte_flow_error_set(error, EPERM,
				   RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				   NULL, "unknown");
	return ret;
}

/**
 * Destroy user-configured flow rules.
 *
 * This function skips internal flows rules.
 *
 * @see rte_flow_flush()
 * @see rte_flow_ops
 */
static int
dpaa2_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_dev_flow *flow = LIST_FIRST(&priv->flows);

	while (flow) {
		struct dpaa2_dev_flow *next = LIST_NEXT(flow, next);

		dpaa2_flow_destroy(dev, (struct rte_flow *)flow, error);
		flow = next;
	}
	return 0;
}

static int
dpaa2_flow_query(struct rte_eth_dev *dev __rte_unused,
	struct rte_flow *_flow __rte_unused,
	const struct rte_flow_action *actions __rte_unused,
	void *data __rte_unused,
	struct rte_flow_error *error __rte_unused)
{
	return 0;
}

/**
 * Clean up all flow rules.
 *
 * Unlike dpaa2_flow_flush(), this function takes care of all remaining flow
 * rules regardless of whether they are internal or user-configured.
 *
 * @param priv
 *   Pointer to private structure.
 */
void
dpaa2_flow_clean(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_flow *flow;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	while ((flow = LIST_FIRST(&priv->flows)))
		dpaa2_flow_destroy(dev, (struct rte_flow *)flow, NULL);
}

const struct rte_flow_ops dpaa2_flow_ops = {
	.create	= dpaa2_flow_create,
	.validate = dpaa2_flow_validate,
	.destroy = dpaa2_flow_destroy,
	.flush	= dpaa2_flow_flush,
	.query	= dpaa2_flow_query,
};
