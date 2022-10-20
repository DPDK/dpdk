/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include "mlx5dr_internal.h"

#define GTP_PDU_SC	0x85
#define BAD_PORT	0xBAD
#define ETH_TYPE_IPV4_VXLAN	0x0800
#define ETH_TYPE_IPV6_VXLAN	0x86DD
#define ETH_VXLAN_DEFAULT_PORT	4789

#define STE_NO_VLAN	0x0
#define STE_SVLAN	0x1
#define STE_CVLAN	0x2
#define STE_IPV4	0x1
#define STE_IPV6	0x2
#define STE_TCP		0x1
#define STE_UDP		0x2
#define STE_ICMP	0x3

/* Setter function based on bit offset and mask, for 32bit DW*/
#define _DR_SET_32(p, v, byte_off, bit_off, mask) \
	do { \
		u32 _v = v; \
		*((rte_be32_t *)(p) + ((byte_off) / 4)) = \
		rte_cpu_to_be_32((rte_be_to_cpu_32(*((u32 *)(p) + \
				  ((byte_off) / 4))) & \
				  (~((mask) << (bit_off)))) | \
				 (((_v) & (mask)) << \
				  (bit_off))); \
	} while (0)

/* Setter function based on bit offset and mask */
#define DR_SET(p, v, byte_off, bit_off, mask) \
	do { \
		if (unlikely((bit_off) < 0)) { \
			u32 _bit_off = -1 * (bit_off); \
			u32 second_dw_mask = (mask) & ((1 << _bit_off) - 1); \
			_DR_SET_32(p, (v) >> _bit_off, byte_off, 0, (mask) >> _bit_off); \
			_DR_SET_32(p, (v) & second_dw_mask, (byte_off) + DW_SIZE, \
				   (bit_off) % BITS_IN_DW, second_dw_mask); \
		} else { \
			_DR_SET_32(p, v, byte_off, (bit_off), (mask)); \
		} \
	} while (0)

/* Setter function based on byte offset to directly set FULL BE32 value  */
#define DR_SET_BE32(p, v, byte_off, bit_off, mask) \
	(*((rte_be32_t *)((uint8_t *)(p) + (byte_off))) = (v))

/* Setter function based on byte offset to directly set FULL BE32 value from ptr  */
#define DR_SET_BE32P(p, v_ptr, byte_off, bit_off, mask) \
	memcpy((uint8_t *)(p) + (byte_off), v_ptr, 4)

/* Setter function based on byte offset to directly set FULL BE16 value  */
#define DR_SET_BE16(p, v, byte_off, bit_off, mask) \
	(*((rte_be16_t *)((uint8_t *)(p) + (byte_off))) = (v))

/* Setter function based on byte offset to directly set FULL BE16 value from ptr  */
#define DR_SET_BE16P(p, v_ptr, byte_off, bit_off, mask) \
	memcpy((uint8_t *)(p) + (byte_off), v_ptr, 2)

#define DR_CALC_FNAME(field, inner) \
	((inner) ? MLX5DR_DEFINER_FNAME_##field##_I : \
		   MLX5DR_DEFINER_FNAME_##field##_O)

#define DR_CALC_SET_HDR(fc, hdr, field) \
	do { \
		(fc)->bit_mask = __mlx5_mask(definer_hl, hdr.field); \
		(fc)->bit_off = __mlx5_dw_bit_off(definer_hl, hdr.field); \
		(fc)->byte_off = MLX5_BYTE_OFF(definer_hl, hdr.field); \
	} while (0)

/* Helper to calculate data used by DR_SET */
#define DR_CALC_SET(fc, hdr, field, is_inner) \
	do { \
		if (is_inner) { \
			DR_CALC_SET_HDR(fc, hdr##_inner, field); \
		} else { \
			DR_CALC_SET_HDR(fc, hdr##_outer, field); \
		} \
	} while (0)

 #define DR_GET(typ, p, fld) \
	((rte_be_to_cpu_32(*((const rte_be32_t *)(p) + \
	__mlx5_dw_off(typ, fld))) >> __mlx5_dw_bit_off(typ, fld)) & \
	__mlx5_mask(typ, fld))

struct mlx5dr_definer_sel_ctrl {
	uint8_t allowed_full_dw; /* Full DW selectors cover all offsets */
	uint8_t allowed_lim_dw;  /* Limited DW selectors cover offset < 64 */
	uint8_t allowed_bytes;   /* Bytes selectors, up to offset 255 */
	uint8_t used_full_dw;
	uint8_t used_lim_dw;
	uint8_t used_bytes;
	uint8_t full_dw_selector[DW_SELECTORS];
	uint8_t lim_dw_selector[DW_SELECTORS_LIMITED];
	uint8_t byte_selector[BYTE_SELECTORS];
};

struct mlx5dr_definer_conv_data {
	struct mlx5dr_cmd_query_caps *caps;
	struct mlx5dr_definer_fc *fc;
	uint8_t relaxed;
	uint8_t tunnel;
	uint8_t *hl;
};

/* Xmacro used to create generic item setter from items */
#define LIST_OF_FIELDS_INFO \
	X(SET_BE16,	eth_type,		v->type,		rte_flow_item_eth) \
	X(SET_BE32P,	eth_smac_47_16,		&v->src.addr_bytes[0],	rte_flow_item_eth) \
	X(SET_BE16P,	eth_smac_15_0,		&v->src.addr_bytes[4],	rte_flow_item_eth) \
	X(SET_BE32P,	eth_dmac_47_16,		&v->dst.addr_bytes[0],	rte_flow_item_eth) \
	X(SET_BE16P,	eth_dmac_15_0,		&v->dst.addr_bytes[4],	rte_flow_item_eth) \
	X(SET_BE16,	tci,			v->tci,			rte_flow_item_vlan) \
	X(SET,		ipv4_ihl,		v->ihl,			rte_ipv4_hdr) \
	X(SET,		ipv4_tos,		v->type_of_service,	rte_ipv4_hdr) \
	X(SET,		ipv4_time_to_live,	v->time_to_live,	rte_ipv4_hdr) \
	X(SET_BE32,	ipv4_dst_addr,		v->dst_addr,		rte_ipv4_hdr) \
	X(SET_BE32,	ipv4_src_addr,		v->src_addr,		rte_ipv4_hdr) \
	X(SET,		ipv4_next_proto,	v->next_proto_id,	rte_ipv4_hdr) \
	X(SET,		ipv4_version,		STE_IPV4,		rte_ipv4_hdr) \
	X(SET_BE16,	ipv4_frag,		v->fragment_offset,	rte_ipv4_hdr) \
	X(SET_BE16,	ipv6_payload_len,	v->hdr.payload_len,	rte_flow_item_ipv6) \
	X(SET,		ipv6_proto,		v->hdr.proto,		rte_flow_item_ipv6) \
	X(SET,		ipv6_hop_limits,	v->hdr.hop_limits,	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_127_96,	&v->hdr.src_addr[0],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_95_64,	&v->hdr.src_addr[4],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_63_32,	&v->hdr.src_addr[8],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_31_0,	&v->hdr.src_addr[12],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_127_96,	&v->hdr.dst_addr[0],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_95_64,	&v->hdr.dst_addr[4],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_63_32,	&v->hdr.dst_addr[8],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_31_0,	&v->hdr.dst_addr[12],	rte_flow_item_ipv6) \
	X(SET,		ipv6_version,		STE_IPV6,		rte_flow_item_ipv6) \
	X(SET,		ipv6_frag,		v->has_frag_ext,	rte_flow_item_ipv6) \
	X(SET,		icmp_protocol,		STE_ICMP,		rte_flow_item_icmp) \
	X(SET,		udp_protocol,		STE_UDP,		rte_flow_item_udp) \
	X(SET_BE16,	udp_src_port,		v->hdr.src_port,	rte_flow_item_udp) \
	X(SET_BE16,	udp_dst_port,		v->hdr.dst_port,	rte_flow_item_udp) \
	X(SET,		tcp_flags,		v->hdr.tcp_flags,	rte_flow_item_tcp) \
	X(SET,		tcp_protocol,		STE_TCP,		rte_flow_item_tcp) \
	X(SET_BE16,	tcp_src_port,		v->hdr.src_port,	rte_flow_item_tcp) \
	X(SET_BE16,	tcp_dst_port,		v->hdr.dst_port,	rte_flow_item_tcp) \
	X(SET,		gtp_udp_port,		RTE_GTPU_UDP_PORT,	rte_flow_item_gtp) \
	X(SET_BE32,	gtp_teid,		v->teid,		rte_flow_item_gtp) \
	X(SET,		gtp_msg_type,		v->msg_type,		rte_flow_item_gtp) \
	X(SET,		gtp_ext_flag,		!!v->v_pt_rsv_flags,	rte_flow_item_gtp) \
	X(SET,		gtp_next_ext_hdr,	GTP_PDU_SC,		rte_flow_item_gtp_psc) \
	X(SET,		gtp_ext_hdr_pdu,	v->hdr.type,		rte_flow_item_gtp_psc) \
	X(SET,		gtp_ext_hdr_qfi,	v->hdr.qfi,		rte_flow_item_gtp_psc) \
	X(SET,		vxlan_flags,		v->flags,		rte_flow_item_vxlan) \
	X(SET,		vxlan_udp_port,		ETH_VXLAN_DEFAULT_PORT,	rte_flow_item_vxlan) \
	X(SET,		source_qp,		v->queue,		mlx5_rte_flow_item_sq) \
	X(SET,		tag,			v->data,		rte_flow_item_tag) \
	X(SET,		metadata,		v->data,		rte_flow_item_meta) \
	X(SET_BE16,	gre_c_ver,		v->c_rsvd0_ver,		rte_flow_item_gre) \
	X(SET_BE16,	gre_protocol_type,	v->protocol,		rte_flow_item_gre) \
	X(SET,		ipv4_protocol_gre,	IPPROTO_GRE,		rte_flow_item_gre) \
	X(SET_BE32,	gre_opt_key,		v->key.key,		rte_flow_item_gre_opt) \
	X(SET_BE32,	gre_opt_seq,		v->sequence.sequence,	rte_flow_item_gre_opt) \
	X(SET_BE16,	gre_opt_checksum,	v->checksum_rsvd.checksum,	rte_flow_item_gre_opt) \
	X(SET,		meter_color,		rte_col_2_mlx5_col(v->color),	rte_flow_item_meter_color)

/* Item set function format */
#define X(set_type, func_name, value, item_type) \
static void mlx5dr_definer_##func_name##_set( \
	struct mlx5dr_definer_fc *fc, \
	const void *item_spec, \
	uint8_t *tag) \
{ \
	__rte_unused const struct item_type *v = item_spec; \
	DR_##set_type(tag, value, fc->byte_off, fc->bit_off, fc->bit_mask); \
}
LIST_OF_FIELDS_INFO
#undef X

static void
mlx5dr_definer_ones_set(struct mlx5dr_definer_fc *fc,
			__rte_unused const void *item_spec,
			__rte_unused uint8_t *tag)
{
	DR_SET(tag, -1, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_eth_first_vlan_q_set(struct mlx5dr_definer_fc *fc,
				    const void *item_spec,
				    uint8_t *tag)
{
	const struct rte_flow_item_eth *v = item_spec;
	uint8_t vlan_type;

	vlan_type = v->has_vlan ? STE_CVLAN : STE_NO_VLAN;

	DR_SET(tag, vlan_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_first_vlan_q_set(struct mlx5dr_definer_fc *fc,
				const void *item_spec,
				uint8_t *tag)
{
	const struct rte_flow_item_vlan *v = item_spec;
	uint8_t vlan_type;

	vlan_type = v->has_more_vlan ? STE_SVLAN : STE_CVLAN;

	DR_SET(tag, vlan_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_conntrack_mask(struct mlx5dr_definer_fc *fc,
			      const void *item_spec,
			      uint8_t *tag)
{
	const struct rte_flow_item_conntrack *m = item_spec;
	uint32_t reg_mask = 0;

	if (m->flags & (RTE_FLOW_CONNTRACK_PKT_STATE_VALID |
			RTE_FLOW_CONNTRACK_PKT_STATE_INVALID |
			RTE_FLOW_CONNTRACK_PKT_STATE_DISABLED))
		reg_mask |= (MLX5_CT_SYNDROME_VALID | MLX5_CT_SYNDROME_INVALID |
			     MLX5_CT_SYNDROME_TRAP);

	if (m->flags & RTE_FLOW_CONNTRACK_PKT_STATE_CHANGED)
		reg_mask |= MLX5_CT_SYNDROME_STATE_CHANGE;

	if (m->flags & RTE_FLOW_CONNTRACK_PKT_STATE_BAD)
		reg_mask |= MLX5_CT_SYNDROME_BAD_PACKET;

	DR_SET(tag, reg_mask, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_conntrack_tag(struct mlx5dr_definer_fc *fc,
			     const void *item_spec,
			     uint8_t *tag)
{
	const struct rte_flow_item_conntrack *v = item_spec;
	uint32_t reg_value = 0;

	/* The conflict should be checked in the validation. */
	if (v->flags & RTE_FLOW_CONNTRACK_PKT_STATE_VALID)
		reg_value |= MLX5_CT_SYNDROME_VALID;

	if (v->flags & RTE_FLOW_CONNTRACK_PKT_STATE_CHANGED)
		reg_value |= MLX5_CT_SYNDROME_STATE_CHANGE;

	if (v->flags & RTE_FLOW_CONNTRACK_PKT_STATE_INVALID)
		reg_value |= MLX5_CT_SYNDROME_INVALID;

	if (v->flags & RTE_FLOW_CONNTRACK_PKT_STATE_DISABLED)
		reg_value |= MLX5_CT_SYNDROME_TRAP;

	if (v->flags & RTE_FLOW_CONNTRACK_PKT_STATE_BAD)
		reg_value |= MLX5_CT_SYNDROME_BAD_PACKET;

	DR_SET(tag, reg_value, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_integrity_set(struct mlx5dr_definer_fc *fc,
			     const void *item_spec,
			     uint8_t *tag)
{
	bool inner = (fc->fname == MLX5DR_DEFINER_FNAME_INTEGRITY_I);
	const struct rte_flow_item_integrity *v = item_spec;
	uint32_t ok1_bits = 0;

	if (v->l3_ok)
		ok1_bits |= inner ? BIT(MLX5DR_DEFINER_OKS1_SECOND_L3_OK) |
				    BIT(MLX5DR_DEFINER_OKS1_SECOND_IPV4_CSUM_OK) :
				    BIT(MLX5DR_DEFINER_OKS1_FIRST_L3_OK) |
				    BIT(MLX5DR_DEFINER_OKS1_FIRST_IPV4_CSUM_OK);

	if (v->ipv4_csum_ok)
		ok1_bits |= inner ? BIT(MLX5DR_DEFINER_OKS1_SECOND_IPV4_CSUM_OK) :
				    BIT(MLX5DR_DEFINER_OKS1_FIRST_IPV4_CSUM_OK);

	if (v->l4_ok)
		ok1_bits |= inner ? BIT(MLX5DR_DEFINER_OKS1_SECOND_L4_OK) |
				    BIT(MLX5DR_DEFINER_OKS1_SECOND_L4_CSUM_OK) :
				    BIT(MLX5DR_DEFINER_OKS1_FIRST_L4_OK) |
				    BIT(MLX5DR_DEFINER_OKS1_FIRST_L4_CSUM_OK);

	if (v->l4_csum_ok)
		ok1_bits |= inner ? BIT(MLX5DR_DEFINER_OKS1_SECOND_L4_CSUM_OK) :
				    BIT(MLX5DR_DEFINER_OKS1_FIRST_L4_CSUM_OK);

	DR_SET(tag, ok1_bits, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_gre_key_set(struct mlx5dr_definer_fc *fc,
			   const void *item_spec,
			   uint8_t *tag)
{
	const rte_be32_t *v = item_spec;

	DR_SET_BE32(tag, *v, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_vxlan_vni_set(struct mlx5dr_definer_fc *fc,
			     const void *item_spec,
			     uint8_t *tag)
{
	const struct rte_flow_item_vxlan *v = item_spec;

	memcpy(tag + fc->byte_off, v->vni, sizeof(v->vni));
}

static void
mlx5dr_definer_ipv6_tos_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	const struct rte_flow_item_ipv6 *v = item_spec;
	uint8_t tos = DR_GET(header_ipv6_vtc, &v->hdr.vtc_flow, tos);

	DR_SET(tag, tos, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_icmp_dw1_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	const struct rte_flow_item_icmp *v = item_spec;
	rte_be32_t icmp_dw1;

	icmp_dw1 = (v->hdr.icmp_type << __mlx5_dw_bit_off(header_icmp, type)) |
		   (v->hdr.icmp_code << __mlx5_dw_bit_off(header_icmp, code)) |
		   (rte_be_to_cpu_16(v->hdr.icmp_cksum) << __mlx5_dw_bit_off(header_icmp, cksum));

	DR_SET(tag, icmp_dw1, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_icmp_dw2_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	const struct rte_flow_item_icmp *v = item_spec;
	rte_be32_t icmp_dw2;

	icmp_dw2 = (rte_be_to_cpu_16(v->hdr.icmp_ident) << __mlx5_dw_bit_off(header_icmp, ident)) |
		   (rte_be_to_cpu_16(v->hdr.icmp_seq_nb) << __mlx5_dw_bit_off(header_icmp, seq_nb));

	DR_SET(tag, icmp_dw2, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_icmp6_dw1_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	const struct rte_flow_item_icmp6 *v = item_spec;
	rte_be32_t icmp_dw1;

	icmp_dw1 = (v->type << __mlx5_dw_bit_off(header_icmp, type)) |
		   (v->code << __mlx5_dw_bit_off(header_icmp, code)) |
		   (rte_be_to_cpu_16(v->checksum) << __mlx5_dw_bit_off(header_icmp, cksum));

	DR_SET(tag, icmp_dw1, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_ipv6_flow_label_set(struct mlx5dr_definer_fc *fc,
				   const void *item_spec,
				   uint8_t *tag)
{
	const struct rte_flow_item_ipv6 *v = item_spec;
	uint32_t flow_label = DR_GET(header_ipv6_vtc, &v->hdr.vtc_flow, flow_label);

	DR_SET(tag, flow_label, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_vport_set(struct mlx5dr_definer_fc *fc,
			 const void *item_spec,
			 uint8_t *tag)
{
	const struct rte_flow_item_ethdev *v = item_spec;
	const struct flow_hw_port_info *port_info;
	uint32_t regc_value;

	port_info = flow_hw_conv_port_id(v->port_id);
	if (unlikely(!port_info))
		regc_value = BAD_PORT;
	else
		regc_value = port_info->regc_value >> fc->bit_off;

	/* Bit offset is set to 0 to since regc value is 32bit */
	DR_SET(tag, regc_value, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static int
mlx5dr_definer_conv_item_eth(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_eth *m = item->mask;
	uint8_t empty_mac[RTE_ETHER_ADDR_LEN] = {0};
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (!m)
		return 0;

	if (m->reserved) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->type) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_type_set;
		DR_CALC_SET(fc, eth_l2, l3_ethertype, inner);
	}

	/* Check SMAC 47_16 */
	if (memcmp(m->src.addr_bytes, empty_mac, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_SMAC_48_16, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_smac_47_16_set;
		DR_CALC_SET(fc, eth_l2_src, smac_47_16, inner);
	}

	/* Check SMAC 15_0 */
	if (memcmp(m->src.addr_bytes + 4, empty_mac + 4, 2)) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_SMAC_15_0, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_smac_15_0_set;
		DR_CALC_SET(fc, eth_l2_src, smac_15_0, inner);
	}

	/* Check DMAC 47_16 */
	if (memcmp(m->dst.addr_bytes, empty_mac, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_DMAC_48_16, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_dmac_47_16_set;
		DR_CALC_SET(fc, eth_l2, dmac_47_16, inner);
	}

	/* Check DMAC 15_0 */
	if (memcmp(m->dst.addr_bytes + 4, empty_mac + 4, 2)) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_DMAC_15_0, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_dmac_15_0_set;
		DR_CALC_SET(fc, eth_l2, dmac_15_0, inner);
	}

	if (m->has_vlan) {
		/* Mark packet as tagged (CVLAN) */
		fc = &cd->fc[DR_CALC_FNAME(VLAN_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->tag_set = &mlx5dr_definer_eth_first_vlan_q_set;
		DR_CALC_SET(fc, eth_l2, first_vlan_qualifier, inner);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_vlan(struct mlx5dr_definer_conv_data *cd,
			      struct rte_flow_item *item,
			      int item_idx)
{
	const struct rte_flow_item_vlan *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (!m)
		return 0;

	if (m->reserved) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (!cd->relaxed || m->has_more_vlan) {
		/* Mark packet as tagged (CVLAN or SVLAN) even if TCI is not specified.*/
		fc = &cd->fc[DR_CALC_FNAME(VLAN_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->tag_set = &mlx5dr_definer_first_vlan_q_set;
		DR_CALC_SET(fc, eth_l2, first_vlan_qualifier, inner);
	}

	if (m->tci) {
		fc = &cd->fc[DR_CALC_FNAME(VLAN_TCI, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_tci_set;
		DR_CALC_SET(fc, eth_l2, tci, inner);
	}

	if (m->inner_type) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_type_set;
		DR_CALC_SET(fc, eth_l2, l3_ethertype, inner);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_ipv4(struct mlx5dr_definer_conv_data *cd,
			      struct rte_flow_item *item,
			      int item_idx)
{
	const struct rte_ipv4_hdr *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_VERSION, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_version_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l3_type, inner);

		/* Overwrite - Unset ethertype if present */
		memset(&cd->fc[DR_CALC_FNAME(ETH_TYPE, inner)], 0, sizeof(*fc));
	}

	if (!m)
		return 0;

	if (m->total_length || m->packet_id ||
	    m->hdr_checksum) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->fragment_offset) {
		fc = &cd->fc[DR_CALC_FNAME(IP_FRAG, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_frag_set;
		DR_CALC_SET(fc, eth_l3, fragment_offset, inner);
	}

	if (m->next_proto_id) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_next_proto_set;
		DR_CALC_SET(fc, eth_l3, protocol_next_header, inner);
	}

	if (m->dst_addr) {
		fc = &cd->fc[DR_CALC_FNAME(IPV4_DST, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_dst_addr_set;
		DR_CALC_SET(fc, ipv4_src_dest, destination_address, inner);
	}

	if (m->src_addr) {
		fc = &cd->fc[DR_CALC_FNAME(IPV4_SRC, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_src_addr_set;
		DR_CALC_SET(fc, ipv4_src_dest, source_address, inner);
	}

	if (m->ihl) {
		fc = &cd->fc[DR_CALC_FNAME(IPV4_IHL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_ihl_set;
		DR_CALC_SET(fc, eth_l3, ihl, inner);
	}

	if (m->time_to_live) {
		fc = &cd->fc[DR_CALC_FNAME(IP_TTL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_time_to_live_set;
		DR_CALC_SET(fc, eth_l3, time_to_live_hop_limit, inner);
	}

	if (m->type_of_service) {
		fc = &cd->fc[DR_CALC_FNAME(IP_TOS, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_tos_set;
		DR_CALC_SET(fc, eth_l3, tos, inner);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_ipv6(struct mlx5dr_definer_conv_data *cd,
			      struct rte_flow_item *item,
			      int item_idx)
{
	const struct rte_flow_item_ipv6 *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_VERSION, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_version_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l3_type, inner);

		/* Overwrite - Unset ethertype if present */
		memset(&cd->fc[DR_CALC_FNAME(ETH_TYPE, inner)], 0, sizeof(*fc));
	}

	if (!m)
		return 0;

	if (m->has_hop_ext || m->has_route_ext || m->has_auth_ext ||
	    m->has_esp_ext || m->has_dest_ext || m->has_mobil_ext ||
	    m->has_hip_ext || m->has_shim6_ext) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->has_frag_ext) {
		fc = &cd->fc[DR_CALC_FNAME(IP_FRAG, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_frag_set;
		DR_CALC_SET(fc, eth_l4, ip_fragmented, inner);
	}

	if (DR_GET(header_ipv6_vtc, &m->hdr.vtc_flow, tos)) {
		fc = &cd->fc[DR_CALC_FNAME(IP_TOS, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_tos_set;
		DR_CALC_SET(fc, eth_l3, tos, inner);
	}

	if (DR_GET(header_ipv6_vtc, &m->hdr.vtc_flow, flow_label)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_FLOW_LABEL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_flow_label_set;
		DR_CALC_SET(fc, eth_l3, flow_label, inner);
	}

	if (m->hdr.payload_len) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_PAYLOAD_LEN, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_payload_len_set;
		DR_CALC_SET(fc, eth_l3, ipv6_payload_length, inner);
	}

	if (m->hdr.proto) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_proto_set;
		DR_CALC_SET(fc, eth_l3, protocol_next_header, inner);
	}

	if (m->hdr.hop_limits) {
		fc = &cd->fc[DR_CALC_FNAME(IP_TTL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_hop_limits_set;
		DR_CALC_SET(fc, eth_l3, time_to_live_hop_limit, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_127_96, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_127_96_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_127_96, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr + 4, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_95_64, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_95_64_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_95_64, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr + 8, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_63_32, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_63_32_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_63_32, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr + 12, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_31_0, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_31_0_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_31_0, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_DST_127_96, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_dst_addr_127_96_set;
		DR_CALC_SET(fc, ipv6_dst, ipv6_address_127_96, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr + 4, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_DST_95_64, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_dst_addr_95_64_set;
		DR_CALC_SET(fc, ipv6_dst, ipv6_address_95_64, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr + 8, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_DST_63_32, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_dst_addr_63_32_set;
		DR_CALC_SET(fc, ipv6_dst, ipv6_address_63_32, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr + 12, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_DST_31_0, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_dst_addr_31_0_set;
		DR_CALC_SET(fc, ipv6_dst, ipv6_address_31_0, inner);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_udp(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_udp *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* Set match on L4 type UDP */
	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_udp_protocol_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l4_type_bwc, inner);
	}

	if (!m)
		return 0;

	if (m->hdr.dgram_cksum || m->hdr.dgram_len) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->hdr.src_port) {
		fc = &cd->fc[DR_CALC_FNAME(L4_SPORT, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_udp_src_port_set;
		DR_CALC_SET(fc, eth_l4, source_port, inner);
	}

	if (m->hdr.dst_port) {
		fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_udp_dst_port_set;
		DR_CALC_SET(fc, eth_l4, destination_port, inner);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_tcp(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_tcp *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* Overwrite match on L4 type TCP */
	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_tcp_protocol_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l4_type_bwc, inner);
	}

	if (!m)
		return 0;

	if (m->hdr.tcp_flags) {
		fc = &cd->fc[DR_CALC_FNAME(TCP_FLAGS, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_tcp_flags_set;
		DR_CALC_SET(fc, eth_l4, tcp_flags, inner);
	}

	if (m->hdr.src_port) {
		fc = &cd->fc[DR_CALC_FNAME(L4_SPORT, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_tcp_src_port_set;
		DR_CALC_SET(fc, eth_l4, source_port, inner);
	}

	if (m->hdr.dst_port) {
		fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_tcp_dst_port_set;
		DR_CALC_SET(fc, eth_l4, destination_port, inner);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_gtp(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_gtp *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	/* Overwrite GTPU dest port if not present */
	fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, false)];
	if (!fc->tag_set && !cd->relaxed) {
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_udp_port_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l4, destination_port, false);
	}

	if (!m)
		return 0;

	if (m->msg_len || m->v_pt_rsv_flags & ~MLX5DR_DEFINER_GTP_EXT_HDR_BIT) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->teid) {
		if (!(cd->caps->flex_protocols & MLX5_HCA_FLEX_GTPU_TEID_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_TEID];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_teid_set;
		fc->bit_mask = __mlx5_mask(header_gtp, teid);
		fc->byte_off = cd->caps->format_select_gtpu_dw_1 * DW_SIZE;
	}

	if (m->v_pt_rsv_flags) {
		if (!(cd->caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_EXT_FLAG];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_ext_flag_set;
		fc->bit_mask = __mlx5_mask(header_gtp, ext_hdr_flag);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp, ext_hdr_flag);
		fc->byte_off = cd->caps->format_select_gtpu_dw_0 * DW_SIZE;
	}


	if (m->msg_type) {
		if (!(cd->caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_MSG_TYPE];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_msg_type_set;
		fc->bit_mask = __mlx5_mask(header_gtp, msg_type);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp, msg_type);
		fc->byte_off = cd->caps->format_select_gtpu_dw_0 * DW_SIZE;
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_gtp_psc(struct mlx5dr_definer_conv_data *cd,
				 struct rte_flow_item *item,
				 int item_idx)
{
	const struct rte_flow_item_gtp_psc *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	/* Overwrite GTP extension flag to be 1 */
	if (!cd->relaxed) {
		if (!(cd->caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_EXT_FLAG];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ones_set;
		fc->bit_mask = __mlx5_mask(header_gtp, ext_hdr_flag);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp, ext_hdr_flag);
		fc->byte_off = cd->caps->format_select_gtpu_dw_0 * DW_SIZE;
	}

	/* Overwrite next extension header type */
	if (!cd->relaxed) {
		if (!(cd->caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_2_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_NEXT_EXT_HDR];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_next_ext_hdr_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->bit_mask = __mlx5_mask(header_opt_gtp, next_ext_hdr_type);
		fc->bit_off = __mlx5_dw_bit_off(header_opt_gtp, next_ext_hdr_type);
		fc->byte_off = cd->caps->format_select_gtpu_dw_2 * DW_SIZE;
	}

	if (!m)
		return 0;

	if (m->hdr.type) {
		if (!(cd->caps->flex_protocols & MLX5_HCA_FLEX_GTPU_FIRST_EXT_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_EXT_HDR_PDU];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_ext_hdr_pdu_set;
		fc->bit_mask = __mlx5_mask(header_gtp_psc, pdu_type);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp_psc, pdu_type);
		fc->byte_off = cd->caps->format_select_gtpu_ext_dw_0 * DW_SIZE;
	}

	if (m->hdr.qfi) {
		if (!(cd->caps->flex_protocols & MLX5_HCA_FLEX_GTPU_FIRST_EXT_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_EXT_HDR_QFI];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_ext_hdr_qfi_set;
		fc->bit_mask = __mlx5_mask(header_gtp_psc, qfi);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp_psc, qfi);
		fc->byte_off = cd->caps->format_select_gtpu_ext_dw_0 * DW_SIZE;
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_port(struct mlx5dr_definer_conv_data *cd,
			      struct rte_flow_item *item,
			      int item_idx)
{
	const struct rte_flow_item_ethdev *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	uint8_t bit_offset = 0;

	if (m->port_id) {
		if (!cd->caps->wire_regc_mask) {
			DR_LOG(ERR, "Port ID item not supported, missing wire REGC mask");
			rte_errno = ENOTSUP;
			return rte_errno;
		}

		while (!(cd->caps->wire_regc_mask & (1 << bit_offset)))
			bit_offset++;

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VPORT_REG_C_0];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vport_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET_HDR(fc, registers, register_c_0);
		fc->bit_off = bit_offset;
		fc->bit_mask = cd->caps->wire_regc_mask >> bit_offset;
	} else {
		DR_LOG(ERR, "Pord ID item mask must specify ID mask");
		rte_errno = EINVAL;
		return rte_errno;
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_vxlan(struct mlx5dr_definer_conv_data *cd,
			       struct rte_flow_item *item,
			       int item_idx)
{
	const struct rte_flow_item_vxlan *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* In order to match on VXLAN we must match on ether_type, ip_protocol
	 * and l4_dport.
	 */
	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		if (!fc->tag_set) {
			fc->item_idx = item_idx;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			fc->tag_set = &mlx5dr_definer_udp_protocol_set;
			DR_CALC_SET(fc, eth_l2, l4_type_bwc, inner);
		}

		fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, inner)];
		if (!fc->tag_set) {
			fc->item_idx = item_idx;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			fc->tag_set = &mlx5dr_definer_vxlan_udp_port_set;
			DR_CALC_SET(fc, eth_l4, destination_port, inner);
		}
	}

	if (!m)
		return 0;

	if (m->flags) {
		if (inner) {
			DR_LOG(ERR, "Inner VXLAN flags item not supported");
			rte_errno = ENOTSUP;
			return rte_errno;
		}

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_FLAGS];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_flags_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_vxlan, flags);
		fc->bit_off = __mlx5_dw_bit_off(header_vxlan, flags);
	}

	if (!is_mem_zero(m->vni, 3)) {
		if (inner) {
			DR_LOG(ERR, "Inner VXLAN vni item not supported");
			rte_errno = ENOTSUP;
			return rte_errno;
		}

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_VNI];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_vni_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_1);
		fc->bit_mask = __mlx5_mask(header_vxlan, vni);
		fc->bit_off = __mlx5_dw_bit_off(header_vxlan, vni);
	}

	return 0;
}

static struct mlx5dr_definer_fc *
mlx5dr_definer_get_register_fc(struct mlx5dr_definer_conv_data *cd, int reg)
{
	struct mlx5dr_definer_fc *fc;

	switch (reg) {
	case REG_C_0:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_0];
		DR_CALC_SET_HDR(fc, registers, register_c_0);
		break;
	case REG_C_1:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_1];
		DR_CALC_SET_HDR(fc, registers, register_c_1);
		break;
	case REG_C_2:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_2];
		DR_CALC_SET_HDR(fc, registers, register_c_2);
		break;
	case REG_C_3:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_3];
		DR_CALC_SET_HDR(fc, registers, register_c_3);
		break;
	case REG_C_4:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_4];
		DR_CALC_SET_HDR(fc, registers, register_c_4);
		break;
	case REG_C_5:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_5];
		DR_CALC_SET_HDR(fc, registers, register_c_5);
		break;
	case REG_C_6:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_6];
		DR_CALC_SET_HDR(fc, registers, register_c_6);
		break;
	case REG_C_7:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_7];
		DR_CALC_SET_HDR(fc, registers, register_c_7);
		break;
	case REG_A:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_A];
		DR_CALC_SET_HDR(fc, metadata, general_purpose);
		break;
	case REG_B:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_B];
		DR_CALC_SET_HDR(fc, metadata, metadata_to_cqe);
		break;
	default:
		rte_errno = ENOTSUP;
		return NULL;
	}

	return fc;
}

static int
mlx5dr_definer_conv_item_tag(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_tag *m = item->mask;
	const struct rte_flow_item_tag *v = item->spec;
	struct mlx5dr_definer_fc *fc;
	int reg;

	if (!m || !v)
		return 0;

	if (item->type == RTE_FLOW_ITEM_TYPE_TAG)
		reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_TAG, v->index);
	else
		reg = (int)v->index;

	if (reg <= 0) {
		DR_LOG(ERR, "Invalid register for item tag");
		rte_errno = EINVAL;
		return rte_errno;
	}

	fc = mlx5dr_definer_get_register_fc(cd, reg);
	if (!fc)
		return rte_errno;

	fc->item_idx = item_idx;
	fc->tag_set = &mlx5dr_definer_tag_set;
	return 0;
}

static int
mlx5dr_definer_conv_item_metadata(struct mlx5dr_definer_conv_data *cd,
				  struct rte_flow_item *item,
				  int item_idx)
{
	const struct rte_flow_item_meta *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	int reg;

	if (!m)
		return 0;

	reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_META, -1);
	if (reg <= 0) {
		DR_LOG(ERR, "Invalid register for item metadata");
		rte_errno = EINVAL;
		return rte_errno;
	}

	fc = mlx5dr_definer_get_register_fc(cd, reg);
	if (!fc)
		return rte_errno;

	fc->item_idx = item_idx;
	fc->tag_set = &mlx5dr_definer_metadata_set;
	return 0;
}

static int
mlx5dr_definer_conv_item_sq(struct mlx5dr_definer_conv_data *cd,
			    struct rte_flow_item *item,
			    int item_idx)
{
	const struct mlx5_rte_flow_item_sq *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	if (!m)
		return 0;

	if (m->queue) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_SOURCE_QP];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->tag_set = &mlx5dr_definer_source_qp_set;
		DR_CALC_SET_HDR(fc, source_qp_gvmi, source_qp);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_gre(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_gre *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (inner) {
		DR_LOG(ERR, "Inner GRE item not supported");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->tag_set = &mlx5dr_definer_ipv4_protocol_gre_set;
		DR_CALC_SET(fc, eth_l3, protocol_next_header, inner);
	}

	if (!m)
		return 0;

	if (m->c_rsvd0_ver) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GRE_C_VER];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gre_c_ver_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_gre, c_rsvd0_ver);
		fc->bit_off = __mlx5_dw_bit_off(header_gre, c_rsvd0_ver);
	}

	if (m->protocol) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GRE_PROTOCOL];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gre_protocol_type_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->byte_off += MLX5_BYTE_OFF(header_gre, gre_protocol);
		fc->bit_mask = __mlx5_mask(header_gre, gre_protocol);
		fc->bit_off = __mlx5_dw_bit_off(header_gre, gre_protocol);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_gre_opt(struct mlx5dr_definer_conv_data *cd,
				 struct rte_flow_item *item,
				 int item_idx)
{
	const struct rte_flow_item_gre_opt *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, false)];
		if (!fc->tag_set) {
			fc->item_idx = item_idx;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			fc->tag_set = &mlx5dr_definer_ipv4_protocol_gre_set;
			DR_CALC_SET(fc, eth_l3, protocol_next_header, false);
		}
	}

	if (!m)
		return 0;

	if (m->checksum_rsvd.checksum) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GRE_OPT_CHECKSUM];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gre_opt_checksum_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_1);
	}

	if (m->key.key) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GRE_OPT_KEY];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gre_opt_key_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_2);
	}

	if (m->sequence.sequence) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GRE_OPT_SEQ];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gre_opt_seq_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_3);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_gre_key(struct mlx5dr_definer_conv_data *cd,
				 struct rte_flow_item *item,
				 int item_idx)
{
	const rte_be32_t *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	if (!cd->relaxed) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GRE_KEY_PRESENT];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_gre, gre_k_present);
		fc->bit_off = __mlx5_dw_bit_off(header_gre, gre_k_present);

		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, false)];
		if (!fc->tag_set) {
			fc->item_idx = item_idx;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			fc->tag_set = &mlx5dr_definer_ipv4_protocol_gre_set;
			DR_CALC_SET(fc, eth_l3, protocol_next_header, false);
		}
	}

	if (!m)
		return 0;

	if (*m) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GRE_OPT_KEY];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gre_key_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_2);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_integrity(struct mlx5dr_definer_conv_data *cd,
				   struct rte_flow_item *item,
				   int item_idx)
{
	const struct rte_flow_item_integrity *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (!m)
		return 0;

	if (m->packet_ok || m->l2_ok || m->l2_crc_ok || m->l3_len_ok) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->l3_ok || m->ipv4_csum_ok || m->l4_ok || m->l4_csum_ok) {
		fc = &cd->fc[DR_CALC_FNAME(INTEGRITY, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_integrity_set;
		DR_CALC_SET_HDR(fc, oks1, oks1_bits);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_conntrack(struct mlx5dr_definer_conv_data *cd,
				   struct rte_flow_item *item,
				   int item_idx)
{
	const struct rte_flow_item_conntrack *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	int reg;

	if (!m)
		return 0;

	reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_CONNTRACK, -1);
	if (reg <= 0) {
		DR_LOG(ERR, "Invalid register for item conntrack");
		rte_errno = EINVAL;
		return rte_errno;
	}

	fc = mlx5dr_definer_get_register_fc(cd, reg);
	if (!fc)
		return rte_errno;

	fc->item_idx = item_idx;
	fc->tag_mask_set = &mlx5dr_definer_conntrack_mask;
	fc->tag_set = &mlx5dr_definer_conntrack_tag;

	return 0;
}

static int
mlx5dr_definer_conv_item_icmp(struct mlx5dr_definer_conv_data *cd,
			      struct rte_flow_item *item,
			      int item_idx)
{
	const struct rte_flow_item_icmp *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* Overwrite match on L4 type ICMP */
	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_icmp_protocol_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l4_type, inner);
	}

	if (!m)
		return 0;

	if (m->hdr.icmp_type || m->hdr.icmp_code || m->hdr.icmp_cksum) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ICMP_DW1];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_icmp_dw1_set;
		DR_CALC_SET_HDR(fc, tcp_icmp, icmp_dw1);
	}

	if (m->hdr.icmp_ident || m->hdr.icmp_seq_nb) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ICMP_DW2];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_icmp_dw2_set;
		DR_CALC_SET_HDR(fc, tcp_icmp, icmp_dw2);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_icmp6(struct mlx5dr_definer_conv_data *cd,
			       struct rte_flow_item *item,
			       int item_idx)
{
	const struct rte_flow_item_icmp6 *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* Overwrite match on L4 type ICMP6 */
	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_icmp_protocol_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l4_type, inner);
	}

	if (!m)
		return 0;

	if (m->type || m->code || m->checksum) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ICMP_DW1];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_icmp6_dw1_set;
		DR_CALC_SET_HDR(fc, tcp_icmp, icmp_dw1);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_meter_color(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_meter_color *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	int reg;

	if (!m)
		return 0;

	reg = flow_hw_get_reg_id(RTE_FLOW_ITEM_TYPE_METER_COLOR, 0);
	MLX5_ASSERT(reg > 0);

	fc = mlx5dr_definer_get_register_fc(cd, reg);
	if (!fc)
		return rte_errno;

	fc->item_idx = item_idx;
	fc->tag_set = &mlx5dr_definer_meter_color_set;
	return 0;
}

static int
mlx5dr_definer_conv_items_to_hl(struct mlx5dr_context *ctx,
				struct mlx5dr_match_template *mt,
				uint8_t *hl)
{
	struct mlx5dr_definer_fc fc[MLX5DR_DEFINER_FNAME_MAX] = {{0}};
	struct mlx5dr_definer_conv_data cd = {0};
	struct rte_flow_item *items = mt->items;
	uint64_t item_flags = 0;
	uint32_t total = 0;
	int i, j;
	int ret;

	cd.fc = fc;
	cd.hl = hl;
	cd.caps = ctx->caps;
	cd.relaxed = mt->flags & MLX5DR_MATCH_TEMPLATE_FLAG_RELAXED_MATCH;

	/* Collect all RTE fields to the field array and set header layout */
	for (i = 0; items->type != RTE_FLOW_ITEM_TYPE_END; i++, items++) {
		cd.tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);

		switch ((int)items->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			ret = mlx5dr_definer_conv_item_eth(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L2 :
						  MLX5_FLOW_LAYER_OUTER_L2;
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			ret = mlx5dr_definer_conv_item_vlan(&cd, items, i);
			item_flags |= cd.tunnel ?
				(MLX5_FLOW_LAYER_INNER_VLAN | MLX5_FLOW_LAYER_INNER_L2) :
				(MLX5_FLOW_LAYER_OUTER_VLAN | MLX5_FLOW_LAYER_OUTER_L2);
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			ret = mlx5dr_definer_conv_item_ipv4(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
						  MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			ret = mlx5dr_definer_conv_item_ipv6(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
						  MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			ret = mlx5dr_definer_conv_item_udp(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L4_UDP :
						  MLX5_FLOW_LAYER_OUTER_L4_UDP;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			ret = mlx5dr_definer_conv_item_tcp(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L4_TCP :
						  MLX5_FLOW_LAYER_OUTER_L4_TCP;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP:
			ret = mlx5dr_definer_conv_item_gtp(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_GTP;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			ret = mlx5dr_definer_conv_item_gtp_psc(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_GTP_PSC;
			break;
		case RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT:
			ret = mlx5dr_definer_conv_item_port(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_REPRESENTED_PORT;
			mt->vport_item_id = i;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			ret = mlx5dr_definer_conv_item_vxlan(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_VXLAN;
			break;
		case MLX5_RTE_FLOW_ITEM_TYPE_SQ:
			ret = mlx5dr_definer_conv_item_sq(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_SQ;
			break;
		case RTE_FLOW_ITEM_TYPE_TAG:
		case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
			ret = mlx5dr_definer_conv_item_tag(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_TAG;
			break;
		case RTE_FLOW_ITEM_TYPE_META:
			ret = mlx5dr_definer_conv_item_metadata(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_METADATA;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			ret = mlx5dr_definer_conv_item_gre(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_OPTION:
			ret = mlx5dr_definer_conv_item_gre_opt(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_GRE;
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
			ret = mlx5dr_definer_conv_item_gre_key(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_GRE_KEY;
			break;
		case RTE_FLOW_ITEM_TYPE_INTEGRITY:
			ret = mlx5dr_definer_conv_item_integrity(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_ITEM_INNER_INTEGRITY :
						  MLX5_FLOW_ITEM_OUTER_INTEGRITY;
			break;
		case RTE_FLOW_ITEM_TYPE_CONNTRACK:
			ret = mlx5dr_definer_conv_item_conntrack(&cd, items, i);
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP:
			ret = mlx5dr_definer_conv_item_icmp(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_ICMP;
			break;
		case RTE_FLOW_ITEM_TYPE_ICMP6:
			ret = mlx5dr_definer_conv_item_icmp6(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_ICMP6;
			break;
		case RTE_FLOW_ITEM_TYPE_METER_COLOR:
			ret = mlx5dr_definer_conv_item_meter_color(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_METER_COLOR;
			break;
		default:
			DR_LOG(ERR, "Unsupported item type %d", items->type);
			rte_errno = ENOTSUP;
			return rte_errno;
		}

		if (ret) {
			DR_LOG(ERR, "Failed processing item type: %d", items->type);
			return ret;
		}
	}

	mt->item_flags = item_flags;

	/* Fill in headers layout and calculate total number of fields  */
	for (i = 0; i < MLX5DR_DEFINER_FNAME_MAX; i++) {
		if (fc[i].tag_set) {
			total++;
			DR_SET(hl, -1, fc[i].byte_off, fc[i].bit_off, fc[i].bit_mask);
		}
	}

	mt->fc_sz = total;
	mt->fc = simple_calloc(total, sizeof(*mt->fc));
	if (!mt->fc) {
		DR_LOG(ERR, "Failed to allocate field copy array");
		rte_errno = ENOMEM;
		return rte_errno;
	}

	j = 0;
	for (i = 0; i < MLX5DR_DEFINER_FNAME_MAX; i++) {
		if (fc[i].tag_set) {
			memcpy(&mt->fc[j], &fc[i], sizeof(*mt->fc));
			mt->fc[j].fname = i;
			j++;
		}
	}

	return 0;
}

static int
mlx5dr_definer_find_byte_in_tag(struct mlx5dr_definer *definer,
				uint32_t hl_byte_off,
				uint32_t *tag_byte_off)
{
	uint8_t byte_offset;
	int i;

	/* Add offset since each DW covers multiple BYTEs */
	byte_offset = hl_byte_off % DW_SIZE;
	for (i = 0; i < DW_SELECTORS; i++) {
		if (definer->dw_selector[i] == hl_byte_off / DW_SIZE) {
			*tag_byte_off = byte_offset + DW_SIZE * (DW_SELECTORS - i - 1);
			return 0;
		}
	}

	/* Add offset to skip DWs in definer */
	byte_offset = DW_SIZE * DW_SELECTORS;
	/* Iterate in reverse since the code uses bytes from 7 -> 0 */
	for (i = BYTE_SELECTORS; i-- > 0 ;) {
		if (definer->byte_selector[i] == hl_byte_off) {
			*tag_byte_off = byte_offset + (BYTE_SELECTORS - i - 1);
			return 0;
		}
	}

	/* The hl byte offset must be part of the definer */
	DR_LOG(INFO, "Failed to map to definer, HL byte [%d] not found", byte_offset);
	rte_errno = EINVAL;
	return rte_errno;
}

static int
mlx5dr_definer_fc_bind(struct mlx5dr_definer *definer,
		       struct mlx5dr_definer_fc *fc,
		       uint32_t fc_sz)
{
	uint32_t tag_offset = 0;
	int ret, byte_diff;
	uint32_t i;

	for (i = 0; i < fc_sz; i++) {
		/* Map header layout byte offset to byte offset in tag */
		ret = mlx5dr_definer_find_byte_in_tag(definer, fc->byte_off, &tag_offset);
		if (ret)
			return ret;

		/* Move setter based on the location in the definer */
		byte_diff = fc->byte_off % DW_SIZE - tag_offset % DW_SIZE;
		fc->bit_off = fc->bit_off + byte_diff * BITS_IN_BYTE;

		/* Update offset in headers layout to offset in tag */
		fc->byte_off = tag_offset;
		fc++;
	}

	return 0;
}

static bool
mlx5dr_definer_best_hl_fit_recu(struct mlx5dr_definer_sel_ctrl *ctrl,
				uint32_t cur_dw,
				uint32_t *data)
{
	uint8_t bytes_set;
	int byte_idx;
	bool ret;
	int i;

	/* Reached end, nothing left to do */
	if (cur_dw == MLX5_ST_SZ_DW(definer_hl))
		return true;

	/* No data set, can skip to next DW */
	while (!*data) {
		cur_dw++;
		data++;

		/* Reached end, nothing left to do */
		if (cur_dw == MLX5_ST_SZ_DW(definer_hl))
			return true;
	}

	/* Used all DW selectors and Byte selectors, no possible solution */
	if (ctrl->allowed_full_dw == ctrl->used_full_dw &&
	    ctrl->allowed_lim_dw == ctrl->used_lim_dw &&
	    ctrl->allowed_bytes == ctrl->used_bytes)
		return false;

	/* Try to use limited DW selectors */
	if (ctrl->allowed_lim_dw > ctrl->used_lim_dw && cur_dw < 64) {
		ctrl->lim_dw_selector[ctrl->used_lim_dw++] = cur_dw;

		ret = mlx5dr_definer_best_hl_fit_recu(ctrl, cur_dw + 1, data + 1);
		if (ret)
			return ret;

		ctrl->lim_dw_selector[--ctrl->used_lim_dw] = 0;
	}

	/* Try to use DW selectors */
	if (ctrl->allowed_full_dw > ctrl->used_full_dw) {
		ctrl->full_dw_selector[ctrl->used_full_dw++] = cur_dw;

		ret = mlx5dr_definer_best_hl_fit_recu(ctrl, cur_dw + 1, data + 1);
		if (ret)
			return ret;

		ctrl->full_dw_selector[--ctrl->used_full_dw] = 0;
	}

	/* No byte selector for offset bigger than 255 */
	if (cur_dw * DW_SIZE > 255)
		return false;

	bytes_set = !!(0x000000ff & *data) +
		    !!(0x0000ff00 & *data) +
		    !!(0x00ff0000 & *data) +
		    !!(0xff000000 & *data);

	/* Check if there are enough byte selectors left */
	if (bytes_set + ctrl->used_bytes > ctrl->allowed_bytes)
		return false;

	/* Try to use Byte selectors */
	for (i = 0; i < DW_SIZE; i++)
		if ((0xff000000 >> (i * BITS_IN_BYTE)) & rte_be_to_cpu_32(*data)) {
			/* Use byte selectors high to low */
			byte_idx = ctrl->allowed_bytes - ctrl->used_bytes - 1;
			ctrl->byte_selector[byte_idx] = cur_dw * DW_SIZE + i;
			ctrl->used_bytes++;
		}

	ret = mlx5dr_definer_best_hl_fit_recu(ctrl, cur_dw + 1, data + 1);
	if (ret)
		return ret;

	for (i = 0; i < DW_SIZE; i++)
		if ((0xff << (i * BITS_IN_BYTE)) & rte_be_to_cpu_32(*data)) {
			ctrl->used_bytes--;
			byte_idx = ctrl->allowed_bytes - ctrl->used_bytes - 1;
			ctrl->byte_selector[byte_idx] = 0;
		}

	return false;
}

static void
mlx5dr_definer_apply_sel_ctrl(struct mlx5dr_definer_sel_ctrl *ctrl,
			      struct mlx5dr_definer *definer)
{
	memcpy(definer->byte_selector, ctrl->byte_selector, ctrl->allowed_bytes);
	memcpy(definer->dw_selector, ctrl->full_dw_selector, ctrl->allowed_full_dw);
	memcpy(definer->dw_selector + ctrl->allowed_full_dw,
	       ctrl->lim_dw_selector, ctrl->allowed_lim_dw);
}

static int
mlx5dr_definer_find_best_hl_fit(struct mlx5dr_context *ctx,
				struct mlx5dr_match_template *mt,
				uint8_t *hl)
{
	struct mlx5dr_definer_sel_ctrl ctrl = {0};
	bool found;

	/* Try to create a match definer */
	ctrl.allowed_full_dw = DW_SELECTORS_MATCH;
	ctrl.allowed_lim_dw = 0;
	ctrl.allowed_bytes = BYTE_SELECTORS;

	found = mlx5dr_definer_best_hl_fit_recu(&ctrl, 0, (uint32_t *)hl);
	if (found) {
		mlx5dr_definer_apply_sel_ctrl(&ctrl, mt->definer);
		mt->definer->type = MLX5DR_DEFINER_TYPE_MATCH;
		return 0;
	}

	/* Try to create a full/limited jumbo definer */
	ctrl.allowed_full_dw = ctx->caps->full_dw_jumbo_support ? DW_SELECTORS :
								  DW_SELECTORS_MATCH;
	ctrl.allowed_lim_dw = ctx->caps->full_dw_jumbo_support ? 0 :
								 DW_SELECTORS_LIMITED;
	ctrl.allowed_bytes = BYTE_SELECTORS;

	found = mlx5dr_definer_best_hl_fit_recu(&ctrl, 0, (uint32_t *)hl);
	if (found) {
		mlx5dr_definer_apply_sel_ctrl(&ctrl, mt->definer);
		mt->definer->type = MLX5DR_DEFINER_TYPE_JUMBO;
		return 0;
	}

	DR_LOG(ERR, "Unable to find supporting match/jumbo definer combination");
	rte_errno = ENOTSUP;
	return rte_errno;
}

static void
mlx5dr_definer_create_tag_mask(struct rte_flow_item *items,
			       struct mlx5dr_definer_fc *fc,
			       uint32_t fc_sz,
			       uint8_t *tag)
{
	uint32_t i;

	for (i = 0; i < fc_sz; i++) {
		if (fc->tag_mask_set)
			fc->tag_mask_set(fc, items[fc->item_idx].mask, tag);
		else
			fc->tag_set(fc, items[fc->item_idx].mask, tag);
		fc++;
	}
}

void mlx5dr_definer_create_tag(const struct rte_flow_item *items,
			       struct mlx5dr_definer_fc *fc,
			       uint32_t fc_sz,
			       uint8_t *tag)
{
	uint32_t i;

	for (i = 0; i < fc_sz; i++) {
		fc->tag_set(fc, items[fc->item_idx].spec, tag);
		fc++;
	}
}

int mlx5dr_definer_get_id(struct mlx5dr_definer *definer)
{
	return definer->obj->id;
}

int mlx5dr_definer_compare(struct mlx5dr_definer *definer_a,
			   struct mlx5dr_definer *definer_b)
{
	int i;

	if (definer_a->type != definer_b->type)
		return 1;

	for (i = 0; i < BYTE_SELECTORS; i++)
		if (definer_a->byte_selector[i] != definer_b->byte_selector[i])
			return 1;

	for (i = 0; i < DW_SELECTORS; i++)
		if (definer_a->dw_selector[i] != definer_b->dw_selector[i])
			return 1;

	for (i = 0; i < MLX5DR_JUMBO_TAG_SZ; i++)
		if (definer_a->mask.jumbo[i] != definer_b->mask.jumbo[i])
			return 1;

	return 0;
}

int mlx5dr_definer_get(struct mlx5dr_context *ctx,
		       struct mlx5dr_match_template *mt)
{
	struct mlx5dr_cmd_definer_create_attr def_attr = {0};
	struct ibv_context *ibv_ctx = ctx->ibv_ctx;
	uint8_t *hl;
	int ret;

	if (mt->refcount++)
		return 0;

	mt->definer = simple_calloc(1, sizeof(*mt->definer));
	if (!mt->definer) {
		DR_LOG(ERR, "Failed to allocate memory for definer");
		rte_errno = ENOMEM;
		goto dec_refcount;
	}

	/* Header layout (hl) holds full bit mask per field */
	hl = simple_calloc(1, MLX5_ST_SZ_BYTES(definer_hl));
	if (!hl) {
		DR_LOG(ERR, "Failed to allocate memory for header layout");
		rte_errno = ENOMEM;
		goto free_definer;
	}

	/* Convert items to hl and allocate the field copy array (fc) */
	ret = mlx5dr_definer_conv_items_to_hl(ctx, mt, hl);
	if (ret) {
		DR_LOG(ERR, "Failed to convert items to hl");
		goto free_hl;
	}

	/* Find the definer for given header layout */
	ret = mlx5dr_definer_find_best_hl_fit(ctx, mt, hl);
	if (ret) {
		DR_LOG(ERR, "Failed to create definer from header layout");
		goto free_field_copy;
	}

	/* Align field copy array based on the new definer */
	ret = mlx5dr_definer_fc_bind(mt->definer,
				     mt->fc,
				     mt->fc_sz);
	if (ret) {
		DR_LOG(ERR, "Failed to bind field copy to definer");
		goto free_field_copy;
	}

	/* Create the tag mask used for definer creation */
	mlx5dr_definer_create_tag_mask(mt->items,
				       mt->fc,
				       mt->fc_sz,
				       mt->definer->mask.jumbo);

	/* Create definer based on the bitmask tag */
	def_attr.match_mask = mt->definer->mask.jumbo;
	def_attr.dw_selector = mt->definer->dw_selector;
	def_attr.byte_selector = mt->definer->byte_selector;
	mt->definer->obj = mlx5dr_cmd_definer_create(ibv_ctx, &def_attr);
	if (!mt->definer->obj)
		goto free_field_copy;

	simple_free(hl);

	return 0;

free_field_copy:
	simple_free(mt->fc);
free_hl:
	simple_free(hl);
free_definer:
	simple_free(mt->definer);
dec_refcount:
	mt->refcount--;

	return rte_errno;
}

void mlx5dr_definer_put(struct mlx5dr_match_template *mt)
{
	if (--mt->refcount)
		return;

	simple_free(mt->fc);
	mlx5dr_cmd_destroy_obj(mt->definer->obj);
	simple_free(mt->definer);
}
