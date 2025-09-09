/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_bitops.h>

#include "mlx5dr_internal.h"

#define GTP_PDU_SC	0x85
#define BAD_PORT	0xBAD
#define BAD_SQN		0xBAD
#define ETH_TYPE_IPV4_VXLAN	0x0800
#define ETH_TYPE_IPV6_VXLAN	0x86DD
#define UDP_VXLAN_PORT	4789
#define UDP_VXLAN_GPE_PORT	4790
#define UDP_GTPU_PORT	2152
#define UDP_ESP_PORT	4500
#define UDP_PORT_MPLS	6635
#define UDP_GENEVE_PORT 6081
#define UDP_ROCEV2_PORT	4791
#define DR_FLOW_LAYER_TUNNEL_NO_MPLS (MLX5_FLOW_LAYER_TUNNEL & ~MLX5_FLOW_LAYER_MPLS)
#define NVGRE_PORT 0x6558
#define NVGRE_C_RSVD0_VER 0x2000
#define NVGRE_C_RSVD0_VER_MASK 0xB000

#define STE_NO_VLAN	0x0
#define STE_SVLAN	0x1
#define STE_CVLAN	0x2
#define STE_NO_L3	0x0
#define STE_IPV4	0x1
#define STE_IPV6	0x2
#define STE_NO_L4	0x0
#define STE_TCP		0x1
#define STE_UDP		0x2
#define STE_ICMP	0x3
#define STE_NO_TUN	0x0
#define STE_ESP		0x3

#define MLX5DR_DEFINER_QUOTA_BLOCK 0
#define MLX5DR_DEFINER_QUOTA_PASS  2
#define MLX5DR_DEFINER_MAX_ROW_LOG 32
#define MLX5DR_DEFINER_HL_OPT_MAX 2

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

/* Getter function based on bit offset and mask, for 32bit DW*/
#define DR_GET_32(p, byte_off, bit_off, mask) \
	((rte_be_to_cpu_32(*((const rte_be32_t *)(p) + ((byte_off) / 4))) >> (bit_off)) & (mask))

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

/* Each row (i) indicates a different matcher size, and each column (j)
 * represents {DW5, DW4, DW3, DW2, DW1, DW0}.
 * For values 0,..,2^i, and j (DW) 0,..,5: mlx5dr_optimal_dist_dw[i][j] is 1 if the
 * number of different hash results on these values equals 2^i, meaning this
 * DW hash distribution is complete.
 */
int mlx5dr_optimal_dist_dw[MLX5DR_DEFINER_MAX_ROW_LOG][DW_SELECTORS_MATCH] = {
	{1, 1, 1, 1, 1, 1}, {0, 1, 1, 0, 1, 0}, {0, 1, 1, 0, 1, 0},
	{1, 0, 1, 0, 1, 0}, {0, 0, 0, 1, 1, 0}, {0, 1, 1, 0, 1, 0},
	{0, 0, 0, 0, 1, 0}, {0, 1, 1, 0, 1, 0}, {0, 0, 0, 0, 0, 0},
	{1, 0, 1, 0, 0, 0}, {0, 0, 0, 0, 0, 0}, {0, 1, 0, 1, 0, 0},
	{1, 0, 0, 0, 0, 0}, {0, 0, 1, 0, 0, 1}, {1, 1, 1, 0, 0, 0},
	{1, 1, 1, 0, 1, 0}, {0, 0, 1, 1, 0, 0}, {0, 1, 1, 0, 0, 1},
	{0, 0, 1, 0, 0, 1}, {0, 0, 1, 0, 0, 0}, {1, 0, 1, 1, 0, 0},
	{1, 0, 1, 0, 0, 1}, {0, 0, 1, 1, 0, 1}, {1, 1, 1, 0, 0, 0},
	{0, 1, 0, 1, 0, 1}, {0, 0, 0, 0, 0, 1}, {0, 0, 0, 1, 1, 1},
	{0, 0, 1, 0, 0, 1}, {1, 1, 0, 1, 1, 0}, {0, 0, 0, 0, 1, 0},
	{0, 0, 0, 1, 1, 0}};

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
	struct mlx5dr_context *ctx;
	struct mlx5dr_definer_fc *fc;
	uint8_t relaxed;
	uint8_t tunnel;
	uint8_t mpls_idx;
	uint8_t geneve_opt_ok_idx;
	uint8_t geneve_opt_data_idx;
	enum rte_flow_item_type last_item;
	enum mlx5dr_table_type table_type;
};

/* Xmacro used to create generic item setter from items */
#define LIST_OF_FIELDS_INFO \
	X(SET_BE16,	eth_type,		v->hdr.ether_type,		rte_flow_item_eth) \
	X(SET_BE32P,	eth_smac_47_16,		&v->hdr.src_addr.addr_bytes[0],	rte_flow_item_eth) \
	X(SET_BE16P,	eth_smac_15_0,		&v->hdr.src_addr.addr_bytes[4],	rte_flow_item_eth) \
	X(SET_BE32P,	eth_dmac_47_16,		&v->hdr.dst_addr.addr_bytes[0],	rte_flow_item_eth) \
	X(SET_BE16P,	eth_dmac_15_0,		&v->hdr.dst_addr.addr_bytes[4],	rte_flow_item_eth) \
	X(SET_BE16,	tci,			v->hdr.vlan_tci,		rte_flow_item_vlan) \
	X(SET,		ipv4_ihl,		v->ihl,			rte_ipv4_hdr) \
	X(SET,		ipv4_tos,		v->type_of_service,	rte_ipv4_hdr) \
	X(SET,		ipv4_time_to_live,	v->time_to_live,	rte_ipv4_hdr) \
	X(SET_BE32,	ipv4_dst_addr,		v->dst_addr,		rte_ipv4_hdr) \
	X(SET_BE32,	ipv4_src_addr,		v->src_addr,		rte_ipv4_hdr) \
	X(SET,		ipv4_next_proto,	v->next_proto_id,	rte_ipv4_hdr) \
	X(SET,		ipv4_version,		STE_IPV4,		rte_ipv4_hdr) \
	X(SET_BE16,	ipv4_frag,		v->fragment_offset,	rte_ipv4_hdr) \
	X(SET_BE16,	ipv4_len,		v->total_length,	rte_ipv4_hdr) \
	X(SET_BE16,	ipv4_identification,	v->packet_id,		rte_ipv4_hdr) \
	X(SET,          ip_fragmented,          !!v->fragment_offset,   rte_ipv4_hdr) \
	X(SET_BE16,	ipv6_payload_len,	v->hdr.payload_len,	rte_flow_item_ipv6) \
	X(SET,		ipv6_proto,		v->hdr.proto,		rte_flow_item_ipv6) \
	X(SET,		ipv6_frag_proto,	v->hdr.next_header, rte_flow_item_ipv6_frag_ext) \
	X(SET,		ipv6_routing_hdr,	IPPROTO_ROUTING,	rte_flow_item_ipv6) \
	X(SET,		ipv6_hop_limits,	v->hdr.hop_limits,	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_127_96,	&v->hdr.src_addr.a[0],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_95_64,	&v->hdr.src_addr.a[4],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_63_32,	&v->hdr.src_addr.a[8],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_src_addr_31_0,	&v->hdr.src_addr.a[12],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_127_96,	&v->hdr.dst_addr.a[0],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_95_64,	&v->hdr.dst_addr.a[4],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_63_32,	&v->hdr.dst_addr.a[8],	rte_flow_item_ipv6) \
	X(SET_BE32P,	ipv6_dst_addr_31_0,	&v->hdr.dst_addr.a[12],	rte_flow_item_ipv6) \
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
	X(SET,		gtp_udp_port,		UDP_GTPU_PORT,		rte_flow_item_gtp) \
	X(SET_BE32,	gtp_teid,		v->hdr.teid,		rte_flow_item_gtp) \
	X(SET,		gtp_msg_type,		v->hdr.msg_type,	rte_flow_item_gtp) \
	X(SET,		gtp_flags,		v->hdr.gtp_hdr_info,	rte_flow_item_gtp) \
	X(SET,		gtp_next_ext_hdr,	GTP_PDU_SC,		rte_flow_item_gtp_psc) \
	X(SET,		gtp_ext_hdr_pdu,	v->hdr.type,		rte_flow_item_gtp_psc) \
	X(SET,		gtp_ext_hdr_qfi,	v->hdr.qfi,		rte_flow_item_gtp_psc) \
	X(SET_BE32,	vxlan_vx_flags,		v->hdr.vx_flags,	rte_flow_item_vxlan) \
	X(SET_BE32,	vxlan_vx_vni,		v->hdr.vx_vni,		rte_flow_item_vxlan) \
	X(SET,		vxlan_udp_port,		UDP_VXLAN_PORT,		rte_flow_item_vxlan) \
	X(SET,		vxlan_gpe_udp_port,	UDP_VXLAN_GPE_PORT,	rte_flow_item_vxlan_gpe) \
	X(SET,		vxlan_gpe_flags,	v->flags,		rte_flow_item_vxlan_gpe) \
	X(SET,		vxlan_gpe_protocol,	v->protocol,		rte_flow_item_vxlan_gpe) \
	X(SET,		vxlan_gpe_rsvd1,	v->rsvd1,		rte_flow_item_vxlan_gpe) \
	X(SET,		mpls_udp_port,		UDP_PORT_MPLS,		rte_flow_item_mpls) \
	X(SET,		source_qp,		v->queue,		mlx5_rte_flow_item_sq) \
	X(SET,		tag,			v->data,		rte_flow_item_tag) \
	X(SET,		metadata,		v->data,		rte_flow_item_meta) \
	X(SET_BE16,	geneve_protocol,	v->protocol,		rte_flow_item_geneve) \
	X(SET,		geneve_udp_port,	UDP_GENEVE_PORT,	rte_flow_item_geneve) \
	X(SET_BE16,	geneve_ctrl,		v->ver_opt_len_o_c_rsvd0,	rte_flow_item_geneve) \
	X(SET_BE16,	gre_c_ver,		v->c_rsvd0_ver,		rte_flow_item_gre) \
	X(SET_BE16,	gre_protocol_type,	v->protocol,		rte_flow_item_gre) \
	X(SET,		ipv4_protocol_gre,	IPPROTO_GRE,		rte_flow_item_gre) \
	X(SET_BE32,	gre_opt_key,		v->key.key,		rte_flow_item_gre_opt) \
	X(SET_BE32,	gre_opt_seq,		v->sequence.sequence,	rte_flow_item_gre_opt) \
	X(SET_BE16,	gre_opt_checksum,	v->checksum_rsvd.checksum,	rte_flow_item_gre_opt) \
	X(SET,		nvgre_def_c_rsvd0_ver,	NVGRE_C_RSVD0_VER,	rte_flow_item_nvgre) \
	X(SET,		nvgre_def_c_rsvd0_ver_mask,	NVGRE_C_RSVD0_VER_MASK,	rte_flow_item_nvgre) \
	X(SET,		nvgre_def_protocol,	NVGRE_PORT,		rte_flow_item_nvgre) \
	X(SET_BE16,	nvgre_c_rsvd0_ver,	v->c_k_s_rsvd0_ver,	rte_flow_item_nvgre) \
	X(SET_BE16,	nvgre_protocol,		v->protocol,		rte_flow_item_nvgre) \
	X(SET_BE32P,	nvgre_dw1,		&v->tni[0],		rte_flow_item_nvgre) \
	X(SET,		meter_color,		rte_col_2_mlx5_col(v->color),	rte_flow_item_meter_color) \
	X(SET,		ipsec_protocol,		IPPROTO_ESP,		rte_flow_item_esp) \
	X(SET,		ipsec_udp_port,		UDP_ESP_PORT,		rte_flow_item_esp) \
	X(SET_BE32,     ipsec_spi,              v->hdr.spi,             rte_flow_item_esp) \
	X(SET_BE32,     ipsec_sequence_number,  v->hdr.seq,             rte_flow_item_esp) \
	X(SET,		ib_l4_udp_port,		UDP_ROCEV2_PORT,	rte_flow_item_ib_bth) \
	X(SET,		ib_l4_opcode,		v->hdr.opcode,		rte_flow_item_ib_bth) \
	X(SET,		random_number,		v->value,		rte_flow_item_random) \
	X(SET,		ib_l4_bth_a,		v->hdr.a,		rte_flow_item_ib_bth) \
	X(SET,		cvlan,			STE_CVLAN,		rte_flow_item_vlan) \
	X(SET_BE16,	inner_type,		v->inner_type,		rte_flow_item_vlan) \

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
mlx5dr_definer_ptype_l2_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	bool inner = (fc->fname == MLX5DR_DEFINER_FNAME_PTYPE_L2_I);
	const struct rte_flow_item_ptype *v = item_spec;
	uint32_t packet_type = v->packet_type &
		(inner ? RTE_PTYPE_INNER_L2_MASK : RTE_PTYPE_L2_MASK);
	uint8_t l2_type = STE_NO_VLAN;

	if (packet_type == (inner ? RTE_PTYPE_INNER_L2_ETHER : RTE_PTYPE_L2_ETHER))
		l2_type = STE_NO_VLAN;
	else if (packet_type == (inner ? RTE_PTYPE_INNER_L2_ETHER_VLAN : RTE_PTYPE_L2_ETHER_VLAN))
		l2_type = STE_CVLAN;
	else if (packet_type == (inner ? RTE_PTYPE_INNER_L2_ETHER_QINQ : RTE_PTYPE_L2_ETHER_QINQ))
		l2_type = STE_SVLAN;

	DR_SET(tag, l2_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_ptype_l3_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	bool inner = (fc->fname == MLX5DR_DEFINER_FNAME_PTYPE_L3_I);
	const struct rte_flow_item_ptype *v = item_spec;
	uint32_t packet_type = v->packet_type &
		(inner ? RTE_PTYPE_INNER_L3_MASK : RTE_PTYPE_L3_MASK);
	uint8_t l3_type = STE_NO_L3;

	if (packet_type == (inner ? RTE_PTYPE_INNER_L3_IPV4 : RTE_PTYPE_L3_IPV4))
		l3_type = STE_IPV4;
	else if (packet_type == (inner ? RTE_PTYPE_INNER_L3_IPV6 : RTE_PTYPE_L3_IPV6))
		l3_type = STE_IPV6;

	DR_SET(tag, l3_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_ptype_l4_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	bool inner = (fc->fname == MLX5DR_DEFINER_FNAME_PTYPE_L4_I);
	const struct rte_flow_item_ptype *v = item_spec;
	uint32_t packet_type = v->packet_type &
		(inner ? RTE_PTYPE_INNER_L4_MASK : RTE_PTYPE_L4_MASK);
	uint8_t l4_type = STE_NO_L4;

	if (packet_type == (inner ? RTE_PTYPE_INNER_L4_TCP : RTE_PTYPE_L4_TCP))
		l4_type = STE_TCP;
	else if (packet_type == (inner ? RTE_PTYPE_INNER_L4_UDP : RTE_PTYPE_L4_UDP))
		l4_type = STE_UDP;
	else if (packet_type == (inner ? RTE_PTYPE_INNER_L4_ESP : RTE_PTYPE_L4_ESP))
		l4_type = STE_ESP;

	DR_SET(tag, l4_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_ptype_l4_ext_set(struct mlx5dr_definer_fc *fc,
				const void *item_spec,
				uint8_t *tag)
{
	bool inner = (fc->fname == MLX5DR_DEFINER_FNAME_PTYPE_L4_EXT_I);
	const struct rte_flow_item_ptype *v = item_spec;
	uint32_t packet_type = v->packet_type &
		(inner ? RTE_PTYPE_INNER_L4_MASK : RTE_PTYPE_L4_MASK);
	uint8_t l4_type = STE_NO_L4;

	if (packet_type == (inner ? RTE_PTYPE_INNER_L4_TCP : RTE_PTYPE_L4_TCP))
		l4_type = STE_TCP;
	else if (packet_type == (inner ? RTE_PTYPE_INNER_L4_UDP : RTE_PTYPE_L4_UDP))
		l4_type = STE_UDP;
	else if (packet_type == (inner ? RTE_PTYPE_INNER_L4_ICMP : RTE_PTYPE_L4_ICMP))
		l4_type = STE_ICMP;
	else if (packet_type == RTE_PTYPE_TUNNEL_ESP)
		l4_type = STE_ESP;

	DR_SET(tag, l4_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_ptype_tunnel_set(struct mlx5dr_definer_fc *fc,
				const void *item_spec,
				uint8_t *tag)
{
	const struct rte_flow_item_ptype *v = item_spec;
	uint32_t packet_type = v->packet_type & RTE_PTYPE_TUNNEL_MASK;
	uint8_t tun_type = STE_NO_TUN;

	if (packet_type == RTE_PTYPE_TUNNEL_ESP)
		tun_type = STE_ESP;

	DR_SET(tag, tun_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_ptype_frag_set(struct mlx5dr_definer_fc *fc,
			      const void *item_spec,
			      uint8_t *tag)
{
	bool inner = (fc->fname == MLX5DR_DEFINER_FNAME_PTYPE_FRAG_I);
	const struct rte_flow_item_ptype *v = item_spec;
	uint32_t packet_type = v->packet_type &
		(inner ? RTE_PTYPE_INNER_L4_FRAG : RTE_PTYPE_L4_FRAG);

	DR_SET(tag, !!packet_type, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_compare_base_value_set(const void *item_spec,
				      uint8_t *tag)
{
	uint32_t *ctrl = &(((uint32_t *)tag)[MLX5DR_DEFINER_COMPARE_STE_ARGUMENT_1]);
	uint32_t *base = &(((uint32_t *)tag)[MLX5DR_DEFINER_COMPARE_STE_BASE_0]);
	const struct rte_flow_item_compare *v = item_spec;
	const struct rte_flow_field_data *a = &v->a;
	const struct rte_flow_field_data *b = &v->b;
	const uint32_t *value;

	value = (const uint32_t *)&b->value[0];

	switch (a->field) {
	case RTE_FLOW_FIELD_RANDOM:
		*base = htobe32(*value << 16);
		break;
	case RTE_FLOW_FIELD_TAG:
	case RTE_FLOW_FIELD_META:
		*base = htobe32(*value);
		break;
	case RTE_FLOW_FIELD_ESP_SEQ_NUM:
		*base = *value;
		break;
	default:
		break;
	}

	MLX5_SET(ste_match_4dw_range_ctrl_dw, ctrl, base0, 1);
}

static void
mlx5dr_definer_compare_op_translate(enum rte_flow_item_compare_op op,
				    uint8_t *tag)
{
	uint32_t *ctrl = &(((uint32_t *)tag)[MLX5DR_DEFINER_COMPARE_STE_ARGUMENT_1]);
	uint8_t operator = 0;
	uint8_t inverse = 0;

	switch (op) {
	case RTE_FLOW_ITEM_COMPARE_EQ:
		operator = 2;
		break;
	case RTE_FLOW_ITEM_COMPARE_NE:
		operator = 2;
		inverse = 1;
		break;
	case RTE_FLOW_ITEM_COMPARE_LT:
		inverse = 1;
		break;
	case RTE_FLOW_ITEM_COMPARE_LE:
		operator = 1;
		break;
	case RTE_FLOW_ITEM_COMPARE_GT:
		operator = 1;
		inverse = 1;
		break;
	case RTE_FLOW_ITEM_COMPARE_GE:
		break;
	default:
		DR_LOG(ERR, "Invalid operation type %d", op);
		assert(false);
	}

	MLX5_SET(ste_match_4dw_range_ctrl_dw, ctrl, inverse0, inverse);
	MLX5_SET(ste_match_4dw_range_ctrl_dw, ctrl, operator0, operator);
}

static void
mlx5dr_definer_compare_arg_set(const void *item_spec,
			       uint8_t *tag)
{
	const struct rte_flow_item_compare *v = item_spec;
	enum rte_flow_item_compare_op op = v->operation;

	mlx5dr_definer_compare_op_translate(op, tag);
}

static void
mlx5dr_definer_compare_set(struct mlx5dr_definer_fc *fc,
			   const void *item_spec,
			   uint8_t *tag)
{
	if (fc->compare_idx == MLX5DR_DEFINER_COMPARE_ARGUMENT_0) {
		mlx5dr_definer_compare_arg_set(item_spec, tag);
		if (fc->compare_set_base)
			mlx5dr_definer_compare_base_value_set(item_spec, tag);
	}
}

static void
mlx5dr_definer_integrity_set(struct mlx5dr_definer_fc *fc,
			     const void *item_spec,
			     uint8_t *tag)
{
	bool inner = (fc->fname == MLX5DR_DEFINER_FNAME_INTEGRITY_I);
	const struct rte_flow_item_integrity *v = item_spec;
	uint32_t ok1_bits = DR_GET_32(tag, fc->byte_off, fc->bit_off, fc->bit_mask);

	if (v->l3_ok)
		ok1_bits |= inner ? BIT(MLX5DR_DEFINER_OKS1_SECOND_L3_OK) :
				    BIT(MLX5DR_DEFINER_OKS1_FIRST_L3_OK);

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
mlx5dr_definer_ipv6_routing_ext_set(struct mlx5dr_definer_fc *fc,
				    const void *item,
				    uint8_t *tag)
{
	const struct rte_flow_item_ipv6_routing_ext *v = item;
	uint32_t val;

	val = v->hdr.next_hdr << __mlx5_dw_bit_off(header_ipv6_routing_ext, next_hdr);
	val |= v->hdr.type << __mlx5_dw_bit_off(header_ipv6_routing_ext, type);
	val |= v->hdr.segments_left <<
		__mlx5_dw_bit_off(header_ipv6_routing_ext, segments_left);
	DR_SET(tag, val, fc->byte_off, 0, fc->bit_mask);
}

static void
mlx5dr_definer_ecpri_common_set(struct mlx5dr_definer_fc *fc,
				const void *item,
				uint8_t *tag)
{
	const struct rte_flow_item_ecpri *ec = item;
	uint32_t val;

	val = ec->hdr.common.u32;

	DR_SET_BE32(tag, val, fc->byte_off, 0, fc->bit_mask);
}

static void
mlx5dr_definer_ecpri_body_set(struct mlx5dr_definer_fc *fc,
			      const void *item,
			      uint8_t *tag)
{
	const struct rte_flow_item_ecpri *ec = item;
	uint32_t val, idx;


	idx = fc->fname - MLX5DR_DEFINER_FNAME_FLEX_PARSER_0;
	/* The 1st DW is used for common field, indeed, there are only 2 DWs. */
	val = ec->hdr.dummy[idx - 1];

	DR_SET_BE32(tag, val, fc->byte_off, 0, fc->bit_mask);
}

static void
mlx5dr_definer_flex_parser_set(struct mlx5dr_definer_fc *fc,
			       const void *item,
			       uint8_t *tag, bool is_inner)
{
	const struct rte_flow_item_flex *flex = item;
	uint32_t byte_off, val, idx;
	int ret;

	val = 0;
	byte_off = MLX5_BYTE_OFF(definer_hl, flex_parser.flex_parser_0);
	idx = fc->fname - MLX5DR_DEFINER_FNAME_FLEX_PARSER_0;
	byte_off -= idx * sizeof(uint32_t);
	ret = mlx5_flex_get_parser_value_per_byte_off(flex, flex->handle, byte_off,
						      is_inner, &val);
	if (ret == -1 || !val)
		return;

	DR_SET(tag, val, fc->byte_off, 0, fc->bit_mask);
}

static void
mlx5dr_definer_flex_parser_inner_set(struct mlx5dr_definer_fc *fc,
				     const void *item,
				     uint8_t *tag)
{
	mlx5dr_definer_flex_parser_set(fc, item, tag, true);
}

static void
mlx5dr_definer_flex_parser_outer_set(struct mlx5dr_definer_fc *fc,
				     const void *item,
				     uint8_t *tag)
{
	mlx5dr_definer_flex_parser_set(fc, item, tag, false);
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
mlx5dr_definer_icmp6_echo_dw1_mask_set(struct mlx5dr_definer_fc *fc,
				       __rte_unused const void *item_spec,
				       uint8_t *tag)
{
	const struct rte_flow_item_icmp6 spec = {0xFF, 0xFF, 0x0};
	mlx5dr_definer_icmp6_dw1_set(fc, &spec, tag);
}

static void
mlx5dr_definer_icmp6_echo_request_dw1_set(struct mlx5dr_definer_fc *fc,
					  __rte_unused const void *item_spec,
					  uint8_t *tag)
{
	const struct rte_flow_item_icmp6 spec = {RTE_ICMP6_ECHO_REQUEST, 0, 0};
	mlx5dr_definer_icmp6_dw1_set(fc, &spec, tag);
}

static void
mlx5dr_definer_icmp6_echo_reply_dw1_set(struct mlx5dr_definer_fc *fc,
					__rte_unused const void *item_spec,
					uint8_t *tag)
{
	const struct rte_flow_item_icmp6 spec = {RTE_ICMP6_ECHO_REPLY, 0, 0};
	mlx5dr_definer_icmp6_dw1_set(fc, &spec, tag);
}

static void
mlx5dr_definer_icmp6_echo_dw2_set(struct mlx5dr_definer_fc *fc,
				  const void *item_spec,
				  uint8_t *tag)
{
	const struct rte_flow_item_icmp6_echo *v = item_spec;
	rte_be32_t dw2;

	dw2 = (rte_be_to_cpu_16(v->hdr.identifier) << __mlx5_dw_bit_off(header_icmp, ident)) |
	      (rte_be_to_cpu_16(v->hdr.sequence) << __mlx5_dw_bit_off(header_icmp, seq_nb));

	DR_SET(tag, dw2, fc->byte_off, fc->bit_off, fc->bit_mask);
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
	const struct flow_hw_port_info *port_info = NULL;
	uint32_t regc_value;

	if (v)
		port_info = flow_hw_conv_port_id(fc->dr_ctx, v->port_id);
	if (unlikely(!port_info))
		regc_value = BAD_PORT;
	else
		regc_value = port_info->regc_value >> fc->bit_off;

	/* Bit offset is set to 0 to since regc value is 32bit */
	DR_SET(tag, regc_value, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static struct mlx5dr_definer_fc *
mlx5dr_definer_get_mpls_fc(struct mlx5dr_definer_conv_data *cd, bool inner)
{
	uint8_t mpls_idx = cd->mpls_idx;
	struct mlx5dr_definer_fc *fc;

	switch (mpls_idx) {
	case 0:
		fc = &cd->fc[DR_CALC_FNAME(MPLS0, inner)];
		DR_CALC_SET_HDR(fc, mpls_inner, mpls0_label);
		break;
	case 1:
		fc = &cd->fc[DR_CALC_FNAME(MPLS1, inner)];
		DR_CALC_SET_HDR(fc, mpls_inner, mpls1_label);
		break;
	case 2:
		fc = &cd->fc[DR_CALC_FNAME(MPLS2, inner)];
		DR_CALC_SET_HDR(fc, mpls_inner, mpls2_label);
		break;
	case 3:
		fc = &cd->fc[DR_CALC_FNAME(MPLS3, inner)];
		DR_CALC_SET_HDR(fc, mpls_inner, mpls3_label);
		break;
	case 4:
		fc = &cd->fc[DR_CALC_FNAME(MPLS4, inner)];
		DR_CALC_SET_HDR(fc, mpls_inner, mpls4_label);
		break;
	default:
		rte_errno = ENOTSUP;
		DR_LOG(ERR, "MPLS index %d is not supported", mpls_idx);
		return NULL;
	}

	return fc;
}

static struct mlx5dr_definer_fc *
mlx5dr_definer_get_mpls_oks_fc(struct mlx5dr_definer_conv_data *cd, bool inner)
{
	uint8_t mpls_idx = cd->mpls_idx;
	struct mlx5dr_definer_fc *fc;

	switch (mpls_idx) {
	case 0:
		fc = &cd->fc[DR_CALC_FNAME(OKS2_MPLS0, inner)];
		DR_CALC_SET_HDR(fc, oks2, second_mpls0_qualifier);
		break;
	case 1:
		fc = &cd->fc[DR_CALC_FNAME(OKS2_MPLS1, inner)];
		DR_CALC_SET_HDR(fc, oks2, second_mpls1_qualifier);
		break;
	case 2:
		fc = &cd->fc[DR_CALC_FNAME(OKS2_MPLS2, inner)];
		DR_CALC_SET_HDR(fc, oks2, second_mpls2_qualifier);
		break;
	case 3:
		fc = &cd->fc[DR_CALC_FNAME(OKS2_MPLS3, inner)];
		DR_CALC_SET_HDR(fc, oks2, second_mpls3_qualifier);
		break;
	case 4:
		fc = &cd->fc[DR_CALC_FNAME(OKS2_MPLS4, inner)];
		DR_CALC_SET_HDR(fc, oks2, second_mpls4_qualifier);
		break;
	default:
		rte_errno = ENOTSUP;
		DR_LOG(ERR, "MPLS index %d is not supported", mpls_idx);
		return NULL;
	}

	return fc;
}

static void
mlx5dr_definer_mpls_label_set(struct mlx5dr_definer_fc *fc,
			      const void *item_spec,
			      uint8_t *tag)
{
	const struct rte_flow_item_mpls *v = item_spec;

	memcpy(tag + fc->byte_off, v->label_tc_s, sizeof(v->label_tc_s));
	memcpy(tag + fc->byte_off + sizeof(v->label_tc_s), &v->ttl, sizeof(v->ttl));
}

static void
mlx5dr_definer_geneve_vni_set(struct mlx5dr_definer_fc *fc,
			      const void *item_spec,
			      uint8_t *tag)
{
	const struct rte_flow_item_geneve *v = item_spec;

	memcpy(tag + fc->byte_off, v->vni, sizeof(v->vni));
}

static void
mlx5dr_definer_geneve_opt_ctrl_set(struct mlx5dr_definer_fc *fc,
				   const void *item_spec,
				   uint8_t *tag)
{
	const struct rte_flow_item_geneve_opt *v = item_spec;
	uint32_t dw0 = 0;

	dw0 |= v->option_type << __mlx5_dw_bit_off(header_geneve_opt, type);
	dw0 |= rte_cpu_to_be_16(v->option_class) << __mlx5_dw_bit_off(header_geneve_opt, class);
	DR_SET(tag, dw0, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_geneve_opt_data_set(struct mlx5dr_definer_fc *fc,
				   const void *item_spec,
				   uint8_t *tag)
{
	const struct rte_flow_item_geneve_opt *v = item_spec;

	DR_SET_BE32(tag, v->data[fc->extra_data], fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_ib_l4_qp_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	const struct rte_flow_item_ib_bth *v = item_spec;

	memcpy(tag + fc->byte_off, &v->hdr.dst_qp, sizeof(v->hdr.dst_qp));
}

static void
mlx5dr_definer_vxlan_gpe_vni_set(struct mlx5dr_definer_fc *fc,
				 const void *item_spec,
				 uint8_t *tag)
{
	const struct rte_flow_item_vxlan_gpe *v = item_spec;

	memcpy(tag + fc->byte_off, v->vni, sizeof(v->vni));
}

static void
mlx5dr_definer_vxlan_gpe_rsvd0_set(struct mlx5dr_definer_fc *fc,
				   const void *item_spec,
				   uint8_t *tag)
{
	const struct rte_flow_item_vxlan_gpe *v = item_spec;
	uint16_t rsvd0;

	rsvd0 = (v->rsvd0[0] << 8 | v->rsvd0[1]);
	DR_SET(tag, rsvd0, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static void
mlx5dr_definer_tx_queue_set(struct mlx5dr_definer_fc *fc,
			    const void *item_spec,
			    uint8_t *tag)
{
	const struct rte_flow_item_tx_queue *v = item_spec;
	uint32_t sqn = 0;
	int ret;

	ret = flow_hw_conv_sqn(fc->extra_data, v->tx_queue, &sqn);
	if (unlikely(ret))
		sqn = BAD_SQN;

	DR_SET(tag, sqn, fc->byte_off, fc->bit_off, fc->bit_mask);
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

	if (m->hdr.ether_type) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_type_set;
		DR_CALC_SET(fc, eth_l2, l3_ethertype, inner);
	}

	/* Check SMAC 47_16 */
	if (memcmp(m->hdr.src_addr.addr_bytes, empty_mac, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_SMAC_48_16, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_smac_47_16_set;
		DR_CALC_SET(fc, eth_l2_src, smac_47_16, inner);
	}

	/* Check SMAC 15_0 */
	if (memcmp(m->hdr.src_addr.addr_bytes + 4, empty_mac + 4, 2)) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_SMAC_15_0, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_smac_15_0_set;
		DR_CALC_SET(fc, eth_l2_src, smac_15_0, inner);
	}

	/* Check DMAC 47_16 */
	if (memcmp(m->hdr.dst_addr.addr_bytes, empty_mac, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_DMAC_48_16, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_eth_dmac_47_16_set;
		DR_CALC_SET(fc, eth_l2, dmac_47_16, inner);
	}

	/* Check DMAC 15_0 */
	if (memcmp(m->hdr.dst_addr.addr_bytes + 4, empty_mac + 4, 2)) {
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

	if (!cd->relaxed) {
		/* Mark packet as tagged (CVLAN) */
		fc = &cd->fc[DR_CALC_FNAME(VLAN_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->tag_set = &mlx5dr_definer_cvlan_set;
		DR_CALC_SET(fc, eth_l2, first_vlan_qualifier, inner);
	}

	if (!m)
		return 0;

	if (m->reserved) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->has_more_vlan) {
		fc = &cd->fc[DR_CALC_FNAME(VLAN_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->tag_set = &mlx5dr_definer_first_vlan_q_set;
		DR_CALC_SET(fc, eth_l2, first_vlan_qualifier, inner);
	}

	if (m->hdr.vlan_tci) {
		fc = &cd->fc[DR_CALC_FNAME(VLAN_TCI, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_tci_set;
		DR_CALC_SET(fc, eth_l2, tci, inner);
	}

	if (m->hdr.eth_proto) {
		fc = &cd->fc[DR_CALC_FNAME(ETH_TYPE, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_inner_type_set;
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
	const struct rte_ipv4_hdr *l = item->last;
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

	if (m->hdr_checksum ||
	    (l && (l->next_proto_id || l->type_of_service))) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->version) {
		fc = &cd->fc[DR_CALC_FNAME(IP_VERSION, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_version_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l3_type, inner);
	}

	if (m->fragment_offset) {
		fc = &cd->fc[DR_CALC_FNAME(IP_FRAG, inner)];
		fc->item_idx = item_idx;
		if (rte_be_to_cpu_16(m->fragment_offset) == 0x3fff) {
			fc->tag_set = &mlx5dr_definer_ip_fragmented_set;
			DR_CALC_SET(fc, eth_l2, ip_fragmented, inner);
		} else {
			fc->is_range = l && l->fragment_offset;
			fc->tag_set = &mlx5dr_definer_ipv4_frag_set;
			DR_CALC_SET(fc, eth_l3, ipv4_frag, inner);
		}
	}

	if (m->next_proto_id) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv4_next_proto_set;
		DR_CALC_SET(fc, eth_l3, protocol_next_header, inner);
	}

	if (m->packet_id) {
		fc = &cd->fc[DR_CALC_FNAME(IP_ID, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->packet_id;
		fc->tag_set = &mlx5dr_definer_ipv4_identification_set;
		DR_CALC_SET(fc, eth_l3, identification, inner);
	}

	if (m->total_length) {
		fc = &cd->fc[DR_CALC_FNAME(IP_LEN, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->total_length;
		fc->tag_set = &mlx5dr_definer_ipv4_len_set;
		DR_CALC_SET(fc, eth_l3, ipv4_total_length, inner);
	}

	if (m->dst_addr) {
		fc = &cd->fc[DR_CALC_FNAME(IPV4_DST, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->dst_addr;
		fc->tag_set = &mlx5dr_definer_ipv4_dst_addr_set;
		DR_CALC_SET(fc, ipv4_src_dest, destination_address, inner);
	}

	if (m->src_addr) {
		fc = &cd->fc[DR_CALC_FNAME(IPV4_SRC, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->src_addr;
		fc->tag_set = &mlx5dr_definer_ipv4_src_addr_set;
		DR_CALC_SET(fc, ipv4_src_dest, source_address, inner);
	}

	if (m->ihl) {
		fc = &cd->fc[DR_CALC_FNAME(IPV4_IHL, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->ihl;
		fc->tag_set = &mlx5dr_definer_ipv4_ihl_set;
		DR_CALC_SET(fc, eth_l3, ihl, inner);
	}

	if (m->time_to_live) {
		fc = &cd->fc[DR_CALC_FNAME(IP_TTL, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->time_to_live;
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
	const struct rte_flow_item_ipv6 *l = item->last;
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
	    m->has_hip_ext || m->has_shim6_ext ||
	    (l && (l->has_frag_ext || l->hdr.vtc_flow || l->hdr.proto ||
		   !is_mem_zero(l->hdr.src_addr.a, 16) ||
		   !is_mem_zero(l->hdr.dst_addr.a, 16)))) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->has_frag_ext) {
		fc = &cd->fc[DR_CALC_FNAME(IP_FRAG, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_frag_set;
		DR_CALC_SET(fc, eth_l4, ip_fragmented, inner);
	}

	if (DR_GET(header_ipv6_vtc, &m->hdr.vtc_flow, version)) {
		fc = &cd->fc[DR_CALC_FNAME(IP_VERSION, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_version_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l3_type, inner);
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
		fc = &cd->fc[DR_CALC_FNAME(IP_LEN, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->hdr.payload_len;
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
		fc->is_range = l && l->hdr.hop_limits;
		fc->tag_set = &mlx5dr_definer_ipv6_hop_limits_set;
		DR_CALC_SET(fc, eth_l3, time_to_live_hop_limit, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr.a, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_127_96, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_127_96_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_127_96, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr.a + 4, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_95_64, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_95_64_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_95_64, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr.a + 8, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_63_32, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_63_32_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_63_32, inner);
	}

	if (!is_mem_zero(m->hdr.src_addr.a + 12, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_SRC_31_0, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_src_addr_31_0_set;
		DR_CALC_SET(fc, ipv6_src, ipv6_address_31_0, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr.a, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_DST_127_96, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_dst_addr_127_96_set;
		DR_CALC_SET(fc, ipv6_dst, ipv6_address_127_96, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr.a + 4, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_DST_95_64, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_dst_addr_95_64_set;
		DR_CALC_SET(fc, ipv6_dst, ipv6_address_95_64, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr.a + 8, 4)) {
		fc = &cd->fc[DR_CALC_FNAME(IPV6_DST_63_32, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_dst_addr_63_32_set;
		DR_CALC_SET(fc, ipv6_dst, ipv6_address_63_32, inner);
	}

	if (!is_mem_zero(m->hdr.dst_addr.a + 12, 4)) {
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
	const struct rte_flow_item_udp *l = item->last;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* Set match on L4 type UDP */
	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		if (!fc->not_overwrite) {
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_udp_protocol_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, l4_type_bwc, inner);
		}
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
		fc->is_range = l && l->hdr.src_port;
		fc->tag_set = &mlx5dr_definer_udp_src_port_set;
		DR_CALC_SET(fc, eth_l4, source_port, inner);
	}

	if (m->hdr.dst_port) {
		fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->hdr.dst_port;
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
	const struct rte_flow_item_tcp *l = item->last;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* Overwrite match on L4 type TCP */
	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		if (!fc->not_overwrite) {
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_tcp_protocol_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, l4_type_bwc, inner);
		}
	}

	if (!m)
		return 0;

	if (m->hdr.sent_seq || m->hdr.recv_ack || m->hdr.data_off ||
	    m->hdr.rx_win || m->hdr.cksum || m->hdr.tcp_urp) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->hdr.tcp_flags) {
		fc = &cd->fc[DR_CALC_FNAME(TCP_FLAGS, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->hdr.tcp_flags;
		fc->tag_set = &mlx5dr_definer_tcp_flags_set;
		DR_CALC_SET(fc, eth_l4, tcp_flags, inner);
	}

	if (m->hdr.src_port) {
		fc = &cd->fc[DR_CALC_FNAME(L4_SPORT, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->hdr.src_port;
		fc->tag_set = &mlx5dr_definer_tcp_src_port_set;
		DR_CALC_SET(fc, eth_l4, source_port, inner);
	}

	if (m->hdr.dst_port) {
		fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, inner)];
		fc->item_idx = item_idx;
		fc->is_range = l && l->hdr.dst_port;
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
	struct mlx5dr_cmd_query_caps *caps = cd->ctx->caps;
	const struct rte_flow_item_gtp *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	if (cd->tunnel) {
		DR_LOG(ERR, "Inner GTPU item not supported");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

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

	if (m->msg_len) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->hdr.teid) {
		if (!(caps->flex_protocols & MLX5_HCA_FLEX_GTPU_TEID_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_TEID];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_teid_set;
		fc->bit_mask = __mlx5_mask(header_gtp, teid);
		fc->byte_off = caps->format_select_gtpu_dw_1 * DW_SIZE;
	}

	if (m->hdr.gtp_hdr_info) {
		if (!(caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_FLAGS];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_flags_set;
		fc->bit_mask = __mlx5_mask(header_gtp, v_pt_rsv_flags);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp, v_pt_rsv_flags);
		fc->byte_off = caps->format_select_gtpu_dw_0 * DW_SIZE;
	}


	if (m->hdr.msg_type) {
		if (!(caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_MSG_TYPE];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_msg_type_set;
		fc->bit_mask = __mlx5_mask(header_gtp, msg_type);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp, msg_type);
		fc->byte_off = caps->format_select_gtpu_dw_0 * DW_SIZE;
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_gtp_psc(struct mlx5dr_definer_conv_data *cd,
				 struct rte_flow_item *item,
				 int item_idx)
{
	struct mlx5dr_cmd_query_caps *caps = cd->ctx->caps;
	const struct rte_flow_item_gtp_psc *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	/* Overwrite GTP extension flag to be 1 */
	if (!cd->relaxed) {
		if (!(caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_EXT_FLAG];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ones_set;
		fc->bit_mask = __mlx5_mask(header_gtp, ext_hdr_flag);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp, ext_hdr_flag);
		fc->byte_off = caps->format_select_gtpu_dw_0 * DW_SIZE;
	}

	/* Overwrite next extension header type */
	if (!cd->relaxed) {
		if (!(caps->flex_protocols & MLX5_HCA_FLEX_GTPU_DW_2_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_NEXT_EXT_HDR];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_next_ext_hdr_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->bit_mask = __mlx5_mask(header_opt_gtp, next_ext_hdr_type);
		fc->bit_off = __mlx5_dw_bit_off(header_opt_gtp, next_ext_hdr_type);
		fc->byte_off = caps->format_select_gtpu_dw_2 * DW_SIZE;
	}

	if (!m)
		return 0;

	if (m->hdr.type) {
		if (!(caps->flex_protocols & MLX5_HCA_FLEX_GTPU_FIRST_EXT_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_EXT_HDR_PDU];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_ext_hdr_pdu_set;
		fc->bit_mask = __mlx5_mask(header_gtp_psc, pdu_type);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp_psc, pdu_type);
		fc->byte_off = caps->format_select_gtpu_ext_dw_0 * DW_SIZE;
	}

	if (m->hdr.qfi) {
		if (!(caps->flex_protocols & MLX5_HCA_FLEX_GTPU_FIRST_EXT_DW_0_ENABLED)) {
			rte_errno = ENOTSUP;
			return rte_errno;
		}
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GTP_EXT_HDR_QFI];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_gtp_ext_hdr_qfi_set;
		fc->bit_mask = __mlx5_mask(header_gtp_psc, qfi);
		fc->bit_off = __mlx5_dw_bit_off(header_gtp_psc, qfi);
		fc->byte_off = caps->format_select_gtpu_ext_dw_0 * DW_SIZE;
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_port(struct mlx5dr_definer_conv_data *cd,
			      struct rte_flow_item *item,
			      int item_idx)
{
	struct mlx5dr_cmd_query_caps *caps = cd->ctx->caps;
	uint16_t port_id = item->mask ?
			   ((const struct rte_flow_item_ethdev *)(item->mask))->port_id : 0;
	struct mlx5dr_definer_fc *fc;

	if (port_id) {
		if (!caps->wire_regc_mask) {
			DR_LOG(ERR, "Port ID item not supported, missing wire REGC mask");
			rte_errno = ENOTSUP;
			return rte_errno;
		}

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VPORT_REG_C_0];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vport_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET_HDR(fc, registers, register_c_0);
		fc->bit_off = rte_ctz32(caps->wire_regc_mask);
		fc->bit_mask = caps->wire_regc_mask >> fc->bit_off;
		fc->dr_ctx = cd->ctx;
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

	if (inner) {
		DR_LOG(ERR, "Inner VXLAN item not supported");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	/* In order to match on VXLAN we must match on ip_protocol and l4_dport */
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

	if (m->hdr.vx_flags) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_DW0];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_vx_flags_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
	}

	if (m->hdr.vx_vni) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_DW1];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_vx_vni_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_1);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_mpls(struct mlx5dr_definer_conv_data *cd,
			      struct rte_flow_item *item,
			      int item_idx)
{
	const struct rte_flow_item_mpls *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool is_udp;

	/* If no protocol is set - assume MPLSoUDP */
	if (!cd->relaxed) {
		/* In order to match on MPLS we must match on ip_protocol and l4_dport. */
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, false)];
		if (!fc->tag_set) {
			fc->item_idx = item_idx;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			fc->tag_set = &mlx5dr_definer_udp_protocol_set;
			DR_CALC_SET(fc, eth_l2, l4_type_bwc, false);
		}
		is_udp = (fc->tag_set == &mlx5dr_definer_udp_protocol_set);

		if (is_udp) {
			/* Set UDP dest port to MPLS. */
			fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, false)];
			if (!fc->tag_set) {
				fc->item_idx = item_idx;
				fc->tag_mask_set = &mlx5dr_definer_ones_set;
				fc->tag_set = &mlx5dr_definer_mpls_udp_port_set;
				DR_CALC_SET(fc, eth_l4, destination_port, false);
			}
		}
	}

	if (m && (!is_mem_zero(m->label_tc_s, 3) || m->ttl)) {
		/* According to HW MPLSoUDP is handled as inner */
		fc = mlx5dr_definer_get_mpls_fc(cd, true);
		if (!fc)
			return rte_errno;

		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_mpls_label_set;
	} else { /* Mask relevant oks2 bit, indicates MPLS label exists.
		  * According to HW MPLSoUDP is handled as inner
		  */
		fc = mlx5dr_definer_get_mpls_oks_fc(cd, true);
		if (!fc)
			return rte_errno;

		fc->item_idx = item_idx;
		fc->tag_set = mlx5dr_definer_ones_set;
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
	case REG_C_8:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_8];
		DR_CALC_SET_HDR(fc, registers, register_c_8);
		break;
	case REG_C_9:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_9];
		DR_CALC_SET_HDR(fc, registers, register_c_9);
		break;
	case REG_C_10:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_10];
		DR_CALC_SET_HDR(fc, registers, register_c_10);
		break;
	case REG_C_11:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_REG_11];
		DR_CALC_SET_HDR(fc, registers, register_c_11);
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
	const struct rte_flow_item_tag *l = item->last;
	struct mlx5dr_definer_fc *fc;
	int reg;

	if (!m || !v)
		return 0;

	if (item->type == RTE_FLOW_ITEM_TYPE_TAG)
		reg = flow_hw_get_reg_id_from_ctx(cd->ctx,
						  RTE_FLOW_ITEM_TYPE_TAG,
						  cd->table_type,
						  v->index);
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
	fc->is_range = l && l->index;
	fc->tag_set = &mlx5dr_definer_tag_set;

	return 0;
}

static void
mlx5dr_definer_quota_set(struct mlx5dr_definer_fc *fc,
			 const void *item_data, uint8_t *tag)
{
	/**
	 * MLX5 PMD implements QUOTA with Meter object.
	 * PMD Quota action translation implicitly increments
	 * Meter register value after HW assigns it.
	 * Meter register values are:
	 *            HW     QUOTA(HW+1)  QUOTA state
	 * RED        0        1 (01b)       BLOCK
	 * YELLOW     1        2 (10b)       PASS
	 * GREEN      2        3 (11b)       PASS
	 *
	 * Quota item checks Meter register bit 1 value to determine state:
	 *            SPEC       MASK
	 * PASS     2 (10b)    2 (10b)
	 * BLOCK    0 (00b)    2 (10b)
	 *
	 * item_data is NULL when template quota item is non-masked:
	 * .. / quota / ..
	 */

	const struct rte_flow_item_quota *quota = item_data;
	uint32_t val;

	if (quota && quota->state == RTE_FLOW_QUOTA_STATE_BLOCK)
		val = MLX5DR_DEFINER_QUOTA_BLOCK;
	else
		val = MLX5DR_DEFINER_QUOTA_PASS;

	DR_SET(tag, val, fc->byte_off, fc->bit_off, fc->bit_mask);
}

static int
mlx5dr_definer_conv_item_quota(struct mlx5dr_definer_conv_data *cd,
			       __rte_unused struct rte_flow_item *item,
			       int item_idx)
{
	int mtr_reg = flow_hw_get_reg_id_from_ctx(cd->ctx,
						  RTE_FLOW_ITEM_TYPE_METER_COLOR,
						  cd->table_type, 0);
	struct mlx5dr_definer_fc *fc;

	if (mtr_reg < 0) {
		rte_errno = EINVAL;
		return rte_errno;
	}

	fc = mlx5dr_definer_get_register_fc(cd, mtr_reg);
	if (!fc)
		return rte_errno;

	fc->tag_set = &mlx5dr_definer_quota_set;
	fc->item_idx = item_idx;
	return 0;
}

static int
mlx5dr_definer_conv_item_metadata(struct mlx5dr_definer_conv_data *cd,
				  struct rte_flow_item *item,
				  int item_idx)
{
	const struct rte_flow_item_meta *m = item->mask;
	const struct rte_flow_item_meta *l = item->last;
	struct mlx5dr_definer_fc *fc;
	int reg;

	if (!m)
		return 0;

	reg = flow_hw_get_reg_id_from_ctx(cd->ctx, RTE_FLOW_ITEM_TYPE_META,
					  cd->table_type, -1);
	if (reg <= 0) {
		DR_LOG(ERR, "Invalid register for item metadata");
		rte_errno = EINVAL;
		return rte_errno;
	}

	fc = mlx5dr_definer_get_register_fc(cd, reg);
	if (!fc)
		return rte_errno;

	fc->item_idx = item_idx;
	fc->is_range = l && l->data;
	fc->tag_set = &mlx5dr_definer_metadata_set;

	return 0;
}

static int
mlx5dr_definer_conv_item_tx_queue(struct mlx5dr_definer_conv_data *cd,
				  struct rte_flow_item *item,
				  int item_idx)
{
	const struct rte_flow_item_tx_queue *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	if (!m)
		return 0;

	if (m->tx_queue) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_SOURCE_QP];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->tag_set = &mlx5dr_definer_tx_queue_set;
		/* User extra_data to save DPDK port_id. */
		fc->extra_data = flow_hw_get_port_id(cd->ctx);
		if (fc->extra_data == UINT16_MAX) {
			DR_LOG(ERR, "Invalid port for item tx_queue");
			rte_errno = EINVAL;
			return rte_errno;
		}
		DR_CALC_SET_HDR(fc, source_qp_gvmi, source_qp);
	}

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
mlx5dr_definer_conv_item_nvgre(struct mlx5dr_definer_conv_data *cd,
				struct rte_flow_item *item,
				int item_idx)
{
	const struct rte_flow_item_nvgre *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (inner) {
		DR_LOG(ERR, "Inner gre item not supported");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		if (!fc->tag_set) {
			fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
			fc->item_idx = item_idx;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			fc->tag_set = &mlx5dr_definer_ipv4_protocol_gre_set;
			DR_CALC_SET(fc, eth_l3, protocol_next_header, inner);
		}

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_NVGRE_C_K_S];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_nvgre_def_c_rsvd0_ver_set;
		fc->tag_mask_set = &mlx5dr_definer_nvgre_def_c_rsvd0_ver_mask_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_gre, c_rsvd0_ver);
		fc->bit_off = __mlx5_dw_bit_off(header_gre, c_rsvd0_ver);

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_NVGRE_PROTOCOL];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_nvgre_def_protocol_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->byte_off += MLX5_BYTE_OFF(header_gre, gre_protocol);
		fc->bit_mask = __mlx5_mask(header_gre, gre_protocol);
		fc->bit_off = __mlx5_dw_bit_off(header_gre, gre_protocol);
	}

	if (!m)
		return 0;

	if (m->c_k_s_rsvd0_ver) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_NVGRE_C_K_S];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_nvgre_c_rsvd0_ver_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_gre, c_rsvd0_ver);
		fc->bit_off = __mlx5_dw_bit_off(header_gre, c_rsvd0_ver);
	}

	if (m->protocol) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_NVGRE_PROTOCOL];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_nvgre_protocol_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->byte_off += MLX5_BYTE_OFF(header_gre, gre_protocol);
		fc->bit_mask = __mlx5_mask(header_gre, gre_protocol);
		fc->bit_off = __mlx5_dw_bit_off(header_gre, gre_protocol);
	}

	if (!is_mem_zero(m->tni, 4)) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_NVGRE_DW1];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_nvgre_dw1_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_2);
	}
	return 0;
}

static int
mlx5dr_definer_conv_item_ptype(struct mlx5dr_definer_conv_data *cd,
			       struct rte_flow_item *item,
			       int item_idx)
{
	const struct rte_flow_item_ptype *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	if (!m)
		return 0;

	if (!(m->packet_type &
	      (RTE_PTYPE_L2_MASK | RTE_PTYPE_L3_MASK | RTE_PTYPE_L4_MASK | RTE_PTYPE_TUNNEL_MASK |
	       RTE_PTYPE_INNER_L2_MASK | RTE_PTYPE_INNER_L3_MASK | RTE_PTYPE_INNER_L4_MASK))) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->packet_type & RTE_PTYPE_L2_MASK) {
		fc = &cd->fc[DR_CALC_FNAME(PTYPE_L2, false)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ptype_l2_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, first_vlan_qualifier, false);
	}

	if (m->packet_type & RTE_PTYPE_INNER_L2_MASK) {
		fc = &cd->fc[DR_CALC_FNAME(PTYPE_L2, true)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ptype_l2_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, first_vlan_qualifier, true);
	}

	if (m->packet_type & RTE_PTYPE_L3_MASK) {
		fc = &cd->fc[DR_CALC_FNAME(PTYPE_L3, false)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ptype_l3_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l3_type, false);
	}

	if (m->packet_type & RTE_PTYPE_INNER_L3_MASK) {
		fc = &cd->fc[DR_CALC_FNAME(PTYPE_L3, true)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ptype_l3_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l3_type, true);
	}

	if (m->packet_type & RTE_PTYPE_L4_MASK) {
		/*
		 * Fragmented IP (Internet Protocol) packet type.
		 * Cannot be combined with Layer 4 Types (TCP/UDP).
		 * The exact value must be specified in the mask.
		 */
		if ((m->packet_type & RTE_PTYPE_L4_MASK) == RTE_PTYPE_L4_FRAG) {
			fc = &cd->fc[DR_CALC_FNAME(PTYPE_FRAG, false)];
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ptype_frag_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, ip_fragmented, false);
		} else {
			fc = &cd->fc[DR_CALC_FNAME(PTYPE_L4, false)];
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ptype_l4_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, l4_type_bwc, false);

			fc = &cd->fc[DR_CALC_FNAME(PTYPE_L4_EXT, false)];
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ptype_l4_ext_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, l4_type, false);
		}
	}

	if (m->packet_type & RTE_PTYPE_INNER_L4_MASK) {
		if ((m->packet_type & RTE_PTYPE_INNER_L4_MASK) == RTE_PTYPE_INNER_L4_FRAG) {
			fc = &cd->fc[DR_CALC_FNAME(PTYPE_FRAG, true)];
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ptype_frag_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, ip_fragmented, true);
		} else {
			fc = &cd->fc[DR_CALC_FNAME(PTYPE_L4, true)];
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ptype_l4_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, l4_type_bwc, true);

			fc = &cd->fc[DR_CALC_FNAME(PTYPE_L4_EXT, true)];
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ptype_l4_ext_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l2, l4_type, true);
		}
	}

	if (m->packet_type & RTE_PTYPE_TUNNEL_MASK) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_PTYPE_TUNNEL];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ptype_tunnel_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l4_type_bwc, false);
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

	if (!m)
		return 0;

	if (m->packet_ok || m->l2_ok || m->l2_crc_ok || m->l3_len_ok) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->l3_ok || m->ipv4_csum_ok || m->l4_ok || m->l4_csum_ok) {
		fc = &cd->fc[DR_CALC_FNAME(INTEGRITY, m->level)];
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

	reg = flow_hw_get_reg_id_from_ctx(cd->ctx,
					  RTE_FLOW_ITEM_TYPE_CONNTRACK,
					  cd->table_type, -1);
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
mlx5dr_definer_conv_item_icmp6_echo(struct mlx5dr_definer_conv_data *cd,
				    struct rte_flow_item *item,
				    int item_idx)
{
	const struct rte_flow_item_icmp6_echo *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (!cd->relaxed) {
		/* Overwrite match on L4 type ICMP6 */
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_icmp_protocol_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l4_type, inner);

		/* Set fixed type and code for icmp6 echo request/reply */
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ICMP_DW1];
		fc->item_idx = item_idx;
		fc->tag_mask_set = &mlx5dr_definer_icmp6_echo_dw1_mask_set;
		if (item->type == RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST)
			fc->tag_set = &mlx5dr_definer_icmp6_echo_request_dw1_set;
		else /* RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY */
			fc->tag_set = &mlx5dr_definer_icmp6_echo_reply_dw1_set;
		DR_CALC_SET_HDR(fc, tcp_icmp, icmp_dw1);
	}

	if (!m)
		return 0;

	/* Set identifier & sequence into icmp_dw2 */
	if (m->hdr.identifier || m->hdr.sequence) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ICMP_DW2];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_icmp6_echo_dw2_set;
		DR_CALC_SET_HDR(fc, tcp_icmp, icmp_dw2);
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

	reg = flow_hw_get_reg_id_from_ctx(cd->ctx,
					  RTE_FLOW_ITEM_TYPE_METER_COLOR,
					  cd->table_type, 0);
	MLX5_ASSERT(reg > 0);

	fc = mlx5dr_definer_get_register_fc(cd, reg);
	if (!fc)
		return rte_errno;

	fc->item_idx = item_idx;
	fc->tag_set = &mlx5dr_definer_meter_color_set;
	return 0;
}

static struct mlx5dr_definer_fc *
mlx5dr_definer_get_flex_parser_fc(struct mlx5dr_definer_conv_data *cd,
				  uint32_t byte_off,
				  int item_idx)
{
	uint32_t byte_off_fp7 = MLX5_BYTE_OFF(definer_hl, flex_parser.flex_parser_7);
	uint32_t byte_off_fp0 = MLX5_BYTE_OFF(definer_hl, flex_parser.flex_parser_0);
	enum mlx5dr_definer_fname fname = MLX5DR_DEFINER_FNAME_FLEX_PARSER_0;
	struct mlx5dr_definer_fc *fc;
	uint32_t idx;

	if (byte_off < byte_off_fp7 || byte_off > byte_off_fp0) {
		rte_errno = EINVAL;
		return NULL;
	}

	/* To match on ESP we must match on ip_protocol and optionally on l4_dport */
	if (!cd->relaxed) {
		bool over_udp;

		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, false)];
		over_udp = fc->tag_set == &mlx5dr_definer_udp_protocol_set;

		if (over_udp) {
			fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, false)];
			if (!fc->tag_set) {
				fc->item_idx = item_idx;
				fc->tag_mask_set = &mlx5dr_definer_ones_set;
				fc->tag_set = &mlx5dr_definer_ipsec_udp_port_set;
				DR_CALC_SET(fc, eth_l4, destination_port, false);
			}
		} else {
			fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, false)];
			if (!fc->tag_set) {
				fc->item_idx = item_idx;
				fc->tag_set = &mlx5dr_definer_ipsec_protocol_set;
				fc->tag_mask_set = &mlx5dr_definer_ones_set;
				DR_CALC_SET(fc, eth_l3, protocol_next_header, false);
			}
		}
	}

	idx = (byte_off_fp0 - byte_off) / (sizeof(uint32_t));
	fname += (enum mlx5dr_definer_fname)idx;
	fc = &cd->fc[fname];
	fc->byte_off = byte_off;
	fc->bit_mask = UINT32_MAX;
	return fc;
}

static int
mlx5dr_definer_conv_item_ipv6_routing_ext(struct mlx5dr_definer_conv_data *cd,
					  struct rte_flow_item *item,
					  int item_idx)
{
	const struct rte_flow_item_ipv6_routing_ext *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;
	uint32_t byte_off;

	if (!cd->relaxed) {
		fc = &cd->fc[DR_CALC_FNAME(IP_VERSION, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_version_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		DR_CALC_SET(fc, eth_l2, l3_type, inner);

		/* Overwrite - Unset ethertype if present */
		memset(&cd->fc[DR_CALC_FNAME(ETH_TYPE, inner)], 0, sizeof(*fc));

		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		if (!fc->tag_set) {
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ipv6_routing_hdr_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			fc->not_overwrite = 1;
			DR_CALC_SET(fc, eth_l3, protocol_next_header, inner);
		}
	} else {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (!m)
		return 0;

	if (m->hdr.hdr_len || m->hdr.flags) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->hdr.next_hdr || m->hdr.type || m->hdr.segments_left) {
		byte_off = flow_hw_get_srh_flex_parser_byte_off_from_ctx(cd->ctx);
		fc = mlx5dr_definer_get_flex_parser_fc(cd, byte_off, item_idx);
		if (!fc)
			return rte_errno;

		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_routing_ext_set;
	}
	return 0;
}

static int
mlx5dr_definer_conv_item_ipv6_frag_ext(struct mlx5dr_definer_conv_data *cd,
					  struct rte_flow_item *item,
					  int item_idx)
{
	const struct rte_flow_item_ipv6_frag_ext *m = item->mask;
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

		fc = &cd->fc[DR_CALC_FNAME(IP_FRAG, inner)];
		if (!fc->tag_set) {
			fc->item_idx = item_idx;
			fc->tag_set = &mlx5dr_definer_ones_set;
			fc->tag_mask_set = &mlx5dr_definer_ones_set;
			DR_CALC_SET(fc, eth_l4, ip_fragmented, inner);
		}
	}

	if (!m)
		return 0;

	if (m->hdr.frag_data || m->hdr.id || m->hdr.reserved) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->hdr.next_header) {
		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, inner)];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipv6_frag_proto_set;
		DR_CALC_SET(fc, eth_l3, protocol_next_header, inner);
	}
	return 0;
}

static int
mlx5dr_definer_conv_item_random(struct mlx5dr_definer_conv_data *cd,
				struct rte_flow_item *item,
				int item_idx)
{
	const struct rte_flow_item_random *m = item->mask;
	const struct rte_flow_item_random *l = item->last;
	struct mlx5dr_definer_fc *fc;

	if (!m)
		return 0;

	if (m->value != (m->value & UINT16_MAX)) {
		DR_LOG(ERR, "Random value is 16 bits only");
		rte_errno = EINVAL;
		return rte_errno;
	}

	fc = &cd->fc[MLX5DR_DEFINER_FNAME_RANDOM_NUM];
	fc->item_idx = item_idx;
	fc->tag_set = &mlx5dr_definer_random_number_set;
	fc->is_range = l && l->value;
	DR_CALC_SET_HDR(fc, random_number, random_number);

	return 0;
}

static uint32_t
mlx5dr_definer_get_ecpri_parser_byte_off_from_ctx(void *dr_ctx, uint32_t *byte_off)
{
	uint32_t base_off = MLX5_BYTE_OFF(definer_hl, flex_parser.flex_parser_0);
	struct mlx5_ecpri_parser_profile *ecp;
	uint32_t i;

	ecp = flow_hw_get_ecpri_parser_profile(dr_ctx);
	if (!ecp)
		return UINT32_MAX;
	for (i = 0; i < ecp->num; i++)
		byte_off[i] = base_off - ecp->ids[i] * sizeof(uint32_t);
	return i;
}

static int
mlx5dr_definer_conv_item_ecpri(struct mlx5dr_definer_conv_data *cd,
			       struct rte_flow_item *item,
			       int item_idx)
{
	const struct rte_flow_item_ecpri *m;
	uint32_t i, mask, byte_off[8] = {0};
	struct mlx5dr_definer_fc *fc;
	uint32_t num_dws;

	num_dws = mlx5dr_definer_get_ecpri_parser_byte_off_from_ctx(cd->ctx, byte_off);
	if (num_dws == UINT32_MAX) {
		DR_LOG(ERR, "failed to get eCPRI samples %d", -rte_errno);
		return rte_errno;
	}

	m = item->mask;
	if (!m)
		return 0;

	for (i = 0; i < num_dws; i++) {
		mask = i == 0 ? m->hdr.common.u32 : m->hdr.dummy[i - 1];
		if (!mask)
			continue;
		mask = htobe32(mask);
		fc = mlx5dr_definer_get_flex_parser_fc(cd, byte_off[i], item_idx);
		if (!fc)
			return rte_errno;

		fc->item_idx = item_idx;
		fc->tag_set = i == 0 ? &mlx5dr_definer_ecpri_common_set :
				       &mlx5dr_definer_ecpri_body_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->bit_mask = mask;
	}
	return 0;
}

static int
mlx5dr_definer_conv_item_geneve(struct mlx5dr_definer_conv_data *cd,
				struct rte_flow_item *item,
				int item_idx)
{
	const struct rte_flow_item_geneve *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (inner) {
		DR_LOG(ERR, "Inner GENEVE item not supported");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	/* In order to match on Geneve we must match on ip_protocol and l4_dport */
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
			fc->tag_set = &mlx5dr_definer_geneve_udp_port_set;
			DR_CALC_SET(fc, eth_l4, destination_port, inner);
		}
	}

	if (!m)
		return 0;

	if (m->rsvd1) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->ver_opt_len_o_c_rsvd0) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GENEVE_CTRL];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_geneve_ctrl_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_geneve, ver_opt_len_o_c_rsvd);
		fc->bit_off = __mlx5_dw_bit_off(header_geneve, ver_opt_len_o_c_rsvd);
	}

	if (m->protocol) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GENEVE_PROTO];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_geneve_protocol_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->byte_off += MLX5_BYTE_OFF(header_geneve, protocol_type);
		fc->bit_mask = __mlx5_mask(header_geneve, protocol_type);
		fc->bit_off = __mlx5_dw_bit_off(header_geneve, protocol_type);
	}

	if (!is_mem_zero(m->vni, 3)) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GENEVE_VNI];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_geneve_vni_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_1);
		fc->bit_mask = __mlx5_mask(header_geneve, vni);
		fc->bit_off = __mlx5_dw_bit_off(header_geneve, vni);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_geneve_opt(struct mlx5dr_definer_conv_data *cd,
				    struct rte_flow_item *item,
				    int item_idx)
{
	const struct rte_flow_item_geneve_opt *m = item->mask;
	const struct rte_flow_item_geneve_opt *v = item->spec;
	struct mlx5_hl_data *hl_ok_bit, *hl_dws;
	struct mlx5dr_definer_fc *fc;
	uint8_t num_of_dws, i;
	bool ok_bit_on_class;
	int ret;

	if (!m || !(m->option_class || m->option_type || m->data))
		return 0;

	if (!v || m->option_type != 0xff) {
		DR_LOG(ERR, "Cannot match geneve opt without valid opt type");
		goto out_not_supp;
	}

	ret = mlx5_get_geneve_hl_data(cd->ctx,
				      v->option_type,
				      v->option_class,
				      &hl_ok_bit,
				      &num_of_dws,
				      &hl_dws,
				      &ok_bit_on_class);
	if (ret) {
		DR_LOG(ERR, "Geneve opt type and class %d not supported", v->option_type);
		goto out_not_supp;
	}

	if (ok_bit_on_class && m->option_class != RTE_BE16(UINT16_MAX)) {
		DR_LOG(ERR, "Geneve option class has invalid mask");
		goto out_not_supp;
	}

	if (!ok_bit_on_class && m->option_class) {
		/* DW0 is used, we will match type, class */
		if (!num_of_dws || hl_dws[0].dw_mask != UINT32_MAX) {
			DR_LOG(ERR, "Geneve opt type %d DW0 not supported", v->option_type);
			goto out_not_supp;
		}

		if (MLX5DR_DEFINER_FNAME_GENEVE_OPT_DW_0 + cd->geneve_opt_data_idx >
		    MLX5DR_DEFINER_FNAME_GENEVE_OPT_DW_7) {
			DR_LOG(ERR, "Max match geneve opt DWs reached");
			goto out_not_supp;
		}

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GENEVE_OPT_DW_0 + cd->geneve_opt_data_idx++];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_geneve_opt_ctrl_set;
		fc->byte_off = hl_dws[0].dw_offset * DW_SIZE;
		fc->bit_mask = UINT32_MAX;
	} else {
		/* DW0 is not used, we must verify geneve opt type exists in packet */
		if (!hl_ok_bit->dw_mask) {
			DR_LOG(ERR, "Geneve opt OK bits not supported");
			goto out_not_supp;
		}

		if (MLX5DR_DEFINER_FNAME_GENEVE_OPT_OK_0 + cd->geneve_opt_ok_idx >
		    MLX5DR_DEFINER_FNAME_GENEVE_OPT_OK_7) {
			DR_LOG(ERR, "Max match geneve opt reached");
			goto out_not_supp;
		}

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GENEVE_OPT_OK_0 + cd->geneve_opt_ok_idx++];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ones_set;
		fc->byte_off = hl_ok_bit->dw_offset * DW_SIZE +
				rte_clz32(hl_ok_bit->dw_mask) / 8;
		fc->bit_off = rte_ctz32(hl_ok_bit->dw_mask);
		fc->bit_mask = 0x1;
	}

	for (i = 1; i < num_of_dws; i++) {
		/* Process each valid geneve option data DW1..N */
		if (!m->data[i - 1])
			continue;

		if (hl_dws[i].dw_mask != UINT32_MAX) {
			DR_LOG(ERR, "Matching Geneve opt data[%d] not supported", i - 1);
			goto out_not_supp;
		}

		if (MLX5DR_DEFINER_FNAME_GENEVE_OPT_DW_0 + cd->geneve_opt_data_idx >
		    MLX5DR_DEFINER_FNAME_GENEVE_OPT_DW_7) {
			DR_LOG(ERR, "Max match geneve options DWs reached");
			goto out_not_supp;
		}

		fc = &cd->fc[MLX5DR_DEFINER_FNAME_GENEVE_OPT_DW_0 + cd->geneve_opt_data_idx++];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_geneve_opt_data_set;
		fc->byte_off = hl_dws[i].dw_offset * DW_SIZE;
		fc->bit_mask = m->data[i - 1];
		/* Use extra_data for data[] set offset */
		fc->extra_data = i - 1;
	}

	return 0;

out_not_supp:
	rte_errno = ENOTSUP;
	return rte_errno;
}

static int
mlx5dr_definer_mt_set_fc(struct mlx5dr_match_template *mt,
			 struct mlx5dr_definer_fc *fc,
			 uint8_t *hl)
{
	uint32_t fc_sz = 0, fcr_sz = 0;
	int i;

	for (i = 0; i < MLX5DR_DEFINER_FNAME_MAX; i++)
		if (fc[i].tag_set)
			fc[i].is_range ? fcr_sz++ : fc_sz++;

	mt->fc = simple_calloc(fc_sz + fcr_sz, sizeof(*mt->fc));
	if (!mt->fc) {
		rte_errno = ENOMEM;
		return rte_errno;
	}

	mt->fcr = mt->fc + fc_sz;

	for (i = 0; i < MLX5DR_DEFINER_FNAME_MAX; i++) {
		if (!fc[i].tag_set)
			continue;

		fc[i].fname = i;

		if (fc[i].is_range) {
			memcpy(&mt->fcr[mt->fcr_sz++], &fc[i], sizeof(*mt->fcr));
		} else {
			memcpy(&mt->fc[mt->fc_sz++], &fc[i], sizeof(*mt->fc));
			DR_SET(hl, -1, fc[i].byte_off, fc[i].bit_off, fc[i].bit_mask);
		}
	}

	return 0;
}

static int
mlx5dr_definer_check_item_range_supp(struct rte_flow_item *item)
{
	if (!item->last)
		return 0;

	switch ((int)item->type) {
	case RTE_FLOW_ITEM_TYPE_IPV4:
	case RTE_FLOW_ITEM_TYPE_IPV6:
	case RTE_FLOW_ITEM_TYPE_UDP:
	case RTE_FLOW_ITEM_TYPE_TCP:
	case RTE_FLOW_ITEM_TYPE_TAG:
	case RTE_FLOW_ITEM_TYPE_META:
	case MLX5_RTE_FLOW_ITEM_TYPE_TAG:
	case RTE_FLOW_ITEM_TYPE_RANDOM:
		return 0;
	default:
		DR_LOG(ERR, "Range not supported over item type %d", item->type);
		rte_errno = ENOTSUP;
		return rte_errno;
	}
}

static int
mlx5dr_definer_conv_item_esp(struct mlx5dr_definer_conv_data *cd,
			     struct rte_flow_item *item,
			     int item_idx)
{
	const struct rte_flow_item_esp *m = item->mask;
	struct mlx5dr_definer_fc *fc;

	/* To match on ESP we must match on ip_protocol and optionally on l4_dport */
	if (!cd->relaxed) {
		bool over_udp;

		fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, false)];
		over_udp = fc->tag_set == &mlx5dr_definer_udp_protocol_set;

		if (over_udp) {
			fc = &cd->fc[DR_CALC_FNAME(L4_DPORT, false)];
			if (!fc->tag_set) {
				fc->item_idx = item_idx;
				fc->tag_mask_set = &mlx5dr_definer_ones_set;
				fc->tag_set = &mlx5dr_definer_ipsec_udp_port_set;
				DR_CALC_SET(fc, eth_l4, destination_port, false);
			}
		} else {
			fc = &cd->fc[DR_CALC_FNAME(IP_PROTOCOL, false)];
			if (!fc->tag_set) {
				fc->item_idx = item_idx;
				fc->tag_set = &mlx5dr_definer_ipsec_protocol_set;
				fc->tag_mask_set = &mlx5dr_definer_ones_set;
				DR_CALC_SET(fc, eth_l3, protocol_next_header, false);
			}
		}
	}

	if (!m)
		return 0;
	if (m->hdr.spi) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ESP_SPI];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipsec_spi_set;
		DR_CALC_SET_HDR(fc, ipsec, spi);
	}
	if (m->hdr.seq) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ESP_SEQUENCE_NUMBER];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ipsec_sequence_number_set;
		DR_CALC_SET_HDR(fc, ipsec, sequence_number);
	}
	return 0;
}

static void mlx5dr_definer_set_conv_tunnel(enum rte_flow_item_type cur_type,
					   uint64_t item_flags,
					   struct mlx5dr_definer_conv_data *cd)
{
	/* Already tunnel nothing to change */
	if (cd->tunnel)
		return;

	/* We can have more than one MPLS label at each level (inner/outer), so
	 * consider tunnel only when it is already under tunnel or if we moved to the
	 * second MPLS level.
	 */
	if (cur_type != RTE_FLOW_ITEM_TYPE_MPLS)
		cd->tunnel = !!(item_flags & MLX5_FLOW_LAYER_TUNNEL);
	else
		cd->tunnel = !!(item_flags & DR_FLOW_LAYER_TUNNEL_NO_MPLS);
}

static int
mlx5dr_definer_conv_item_flex_parser(struct mlx5dr_definer_conv_data *cd,
				     struct rte_flow_item *item,
				     int item_idx)
{
	uint32_t base_off = MLX5_BYTE_OFF(definer_hl, flex_parser.flex_parser_0);
	const struct rte_flow_item_flex *v, *m;
	enum mlx5dr_definer_fname fname;
	struct mlx5dr_definer_fc *fc;
	uint32_t i, mask, byte_off;
	bool is_inner = cd->tunnel;
	int ret;

	m = item->mask;
	v = item->spec;
	mask = 0;
	for (i = 0; i < MLX5_GRAPH_NODE_SAMPLE_NUM; i++) {
		byte_off = base_off - i * sizeof(uint32_t);
		ret = mlx5_flex_get_parser_value_per_byte_off(m, v->handle, byte_off,
							      is_inner, &mask);
		if (ret == -1) {
			rte_errno = EINVAL;
			return rte_errno;
		}

		if (!mask)
			continue;

		fname = MLX5DR_DEFINER_FNAME_FLEX_PARSER_0;
		fname += (enum mlx5dr_definer_fname)i;
		fc = &cd->fc[fname];
		fc->byte_off = byte_off;
		fc->item_idx = item_idx;
		fc->tag_set = cd->tunnel ? &mlx5dr_definer_flex_parser_inner_set :
					   &mlx5dr_definer_flex_parser_outer_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->bit_mask = mask;
	}
	return 0;
}

static int
mlx5dr_definer_conv_item_ib_l4(struct mlx5dr_definer_conv_data *cd,
			       struct rte_flow_item *item,
			       int item_idx)
{
	const struct rte_flow_item_ib_bth *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	/* In order to match on RoCEv2(layer4 ib), we must match
	 * on ip_protocol and l4_dport.
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
			fc->tag_set = &mlx5dr_definer_ib_l4_udp_port_set;
			DR_CALC_SET(fc, eth_l4, destination_port, inner);
		}
	}

	if (!m)
		return 0;

	if (m->hdr.se || m->hdr.m || m->hdr.padcnt || m->hdr.tver ||
		m->hdr.pkey || m->hdr.f || m->hdr.b || m->hdr.rsvd0 ||
		m->hdr.rsvd1 || !is_mem_zero(m->hdr.psn, 3)) {
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	if (m->hdr.opcode) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_IB_L4_OPCODE];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ib_l4_opcode_set;
		DR_CALC_SET_HDR(fc, ib_l4, opcode);
	}

	if (!is_mem_zero(m->hdr.dst_qp, 3)) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_IB_L4_QPN];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ib_l4_qp_set;
		DR_CALC_SET_HDR(fc, ib_l4, qp);
	}

	if (m->hdr.a) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_IB_L4_A];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_ib_l4_bth_a_set;
		DR_CALC_SET_HDR(fc, ib_l4, ackreq);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_vxlan_gpe(struct mlx5dr_definer_conv_data *cd,
				   struct rte_flow_item *item,
				   int item_idx)
{
	const struct rte_flow_item_vxlan_gpe *m = item->mask;
	struct mlx5dr_definer_fc *fc;
	bool inner = cd->tunnel;

	if (inner) {
		DR_LOG(ERR, "Inner VXLAN GPE item not supported");
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	/* In order to match on VXLAN GPE we must match on ip_protocol and l4_dport */
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
			fc->tag_set = &mlx5dr_definer_vxlan_gpe_udp_port_set;
			DR_CALC_SET(fc, eth_l4, destination_port, inner);
		}
	}

	if (!m)
		return 0;

	if (m->flags) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_GPE_FLAGS];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_gpe_flags_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_vxlan_gpe, flags);
		fc->bit_off = __mlx5_dw_bit_off(header_vxlan_gpe, flags);
	}

	if (!is_mem_zero(m->rsvd0, 2)) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_GPE_RSVD0];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_gpe_rsvd0_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->bit_mask = __mlx5_mask(header_vxlan_gpe, rsvd0);
		fc->bit_off = __mlx5_dw_bit_off(header_vxlan_gpe, rsvd0);
	}

	if (m->protocol) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_GPE_PROTO];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_gpe_protocol_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_0);
		fc->byte_off += MLX5_BYTE_OFF(header_vxlan_gpe, protocol);
		fc->bit_mask = __mlx5_mask(header_vxlan_gpe, protocol);
		fc->bit_off = __mlx5_dw_bit_off(header_vxlan_gpe, protocol);
	}

	if (!is_mem_zero(m->vni, 3)) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_GPE_VNI];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_gpe_vni_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_1);
		fc->bit_mask = __mlx5_mask(header_vxlan_gpe, vni);
		fc->bit_off = __mlx5_dw_bit_off(header_vxlan_gpe, vni);
	}

	if (m->rsvd1) {
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_VXLAN_GPE_RSVD1];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_vxlan_gpe_rsvd1_set;
		DR_CALC_SET_HDR(fc, tunnel_header, tunnel_header_1);
		fc->bit_mask = __mlx5_mask(header_vxlan_gpe, rsvd1);
		fc->bit_off = __mlx5_dw_bit_off(header_vxlan_gpe, rsvd1);
	}

	return 0;
}

static int
mlx5dr_definer_conv_item_compare_field(const struct rte_flow_field_data *f,
				       const struct rte_flow_field_data *other_f,
				       struct mlx5dr_definer_conv_data *cd,
				       int item_idx,
				       enum mlx5dr_definer_compare_dw_selectors dw_offset)
{
	struct mlx5dr_definer_fc *fc = NULL;
	int reg;

	if (f->offset) {
		DR_LOG(ERR, "field offset %u is not supported, only offset zero supported",
		       f->offset);
		goto err_notsup;
	}

	switch (f->field) {
	case RTE_FLOW_FIELD_META:
		reg = flow_hw_get_reg_id_from_ctx(cd->ctx,
						  RTE_FLOW_ITEM_TYPE_META,
						  cd->table_type, -1);
		if (reg <= 0) {
			DR_LOG(ERR, "Invalid register for compare metadata field");
			rte_errno = EINVAL;
			return rte_errno;
		}

		fc = mlx5dr_definer_get_register_fc(cd, reg);
		if (!fc)
			return rte_errno;

		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_compare_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->compare_idx = dw_offset;
		break;
	case RTE_FLOW_FIELD_TAG:
		reg = flow_hw_get_reg_id_from_ctx(cd->ctx,
						  RTE_FLOW_ITEM_TYPE_TAG,
						  cd->table_type,
						  f->tag_index);
		if (reg <= 0) {
			DR_LOG(ERR, "Invalid register for compare tag field");
			rte_errno = EINVAL;
			return rte_errno;
		}

		fc = mlx5dr_definer_get_register_fc(cd, reg);
		if (!fc)
			return rte_errno;

		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_compare_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->compare_idx = dw_offset;
		break;
	case RTE_FLOW_FIELD_VALUE:
		if (dw_offset == MLX5DR_DEFINER_COMPARE_ARGUMENT_0) {
			DR_LOG(ERR, "Argument field does not support immediate value");
			goto err_notsup;
		}
		break;
	case RTE_FLOW_FIELD_RANDOM:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_RANDOM_NUM];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_compare_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->compare_idx = dw_offset;
		DR_CALC_SET_HDR(fc, random_number, random_number);
		break;
	case RTE_FLOW_FIELD_ESP_SEQ_NUM:
		fc = &cd->fc[MLX5DR_DEFINER_FNAME_ESP_SEQUENCE_NUMBER];
		fc->item_idx = item_idx;
		fc->tag_set = &mlx5dr_definer_compare_set;
		fc->tag_mask_set = &mlx5dr_definer_ones_set;
		fc->compare_idx = dw_offset;
		DR_CALC_SET_HDR(fc, ipsec, sequence_number);
		break;
	default:
		DR_LOG(ERR, "%u field is not supported", f->field);
		goto err_notsup;
	}

	if (fc && other_f && other_f->field == RTE_FLOW_FIELD_VALUE)
		fc->compare_set_base = true;

	return 0;

err_notsup:
	rte_errno = ENOTSUP;
	return rte_errno;
}

static int
mlx5dr_definer_conv_item_compare(struct mlx5dr_definer_conv_data *cd,
				 struct rte_flow_item *item,
				 int item_idx)
{
	const struct rte_flow_item_compare *m = item->mask;
	const struct rte_flow_field_data *a = &m->a;
	const struct rte_flow_field_data *b = &m->b;
	int ret;

	if (m->width != 0xffffffff) {
		DR_LOG(ERR, "compare item width of 0x%x is not supported, only full DW supported",
				m->width);
		rte_errno = ENOTSUP;
		return rte_errno;
	}

	ret = mlx5dr_definer_conv_item_compare_field(a, b, cd, item_idx,
						     MLX5DR_DEFINER_COMPARE_ARGUMENT_0);
	if (ret)
		return ret;

	ret = mlx5dr_definer_conv_item_compare_field(b, NULL, cd, item_idx,
						     MLX5DR_DEFINER_COMPARE_BASE_0);
	if (ret)
		return ret;

	return 0;
}

static int
mlx5dr_definer_conv_items_to_hl(struct mlx5dr_context *ctx,
				struct mlx5dr_match_template *mt,
				uint8_t *hl,
				struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_definer_fc fc[MLX5DR_DEFINER_FNAME_MAX] = {{0}};
	struct mlx5dr_definer_conv_data cd = {0};
	struct rte_flow_item *items = mt->items;
	uint64_t item_flags = 0;
	int i, ret;

	cd.fc = fc;
	cd.ctx = ctx;
	cd.relaxed = mt->flags & MLX5DR_MATCH_TEMPLATE_FLAG_RELAXED_MATCH;
	cd.table_type = matcher->tbl->type;

	/* Collect all RTE fields to the field array and set header layout */
	for (i = 0; items->type != RTE_FLOW_ITEM_TYPE_END; i++, items++) {
		mlx5dr_definer_set_conv_tunnel(items->type, item_flags, &cd);

		ret = mlx5dr_definer_check_item_range_supp(items);
		if (ret)
			return ret;

		if (mlx5dr_matcher_is_compare(matcher)) {
			DR_LOG(ERR, "Compare matcher not supported for more than one item");
			goto not_supp;
		}

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
			if (cd.last_item == RTE_FLOW_ITEM_TYPE_IPV4 ||
			    cd.last_item == RTE_FLOW_ITEM_TYPE_IPV6) {
				cd.tunnel = true;
				item_flags |= MLX5_FLOW_LAYER_IPIP;
			}
			ret = mlx5dr_definer_conv_item_ipv4(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV4 :
						  MLX5_FLOW_LAYER_OUTER_L3_IPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			if (cd.last_item == RTE_FLOW_ITEM_TYPE_IPV4 ||
			    cd.last_item == RTE_FLOW_ITEM_TYPE_IPV6) {
				cd.tunnel = true;
				item_flags |= MLX5_FLOW_LAYER_IPIP;
			}
			ret = mlx5dr_definer_conv_item_ipv6(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6 :
						  MLX5_FLOW_LAYER_OUTER_L3_IPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT:
			ret = mlx5dr_definer_conv_item_ipv6_frag_ext(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_LAYER_INNER_L3_IPV6_FRAG_EXT :
						  MLX5_FLOW_LAYER_OUTER_L3_IPV6_FRAG_EXT;
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
		case RTE_FLOW_ITEM_TYPE_TX_QUEUE:
			ret = mlx5dr_definer_conv_item_tx_queue(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_SQ;
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
			item_flags |= MLX5_FLOW_ITEM_INTEGRITY;
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
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REQUEST:
		case RTE_FLOW_ITEM_TYPE_ICMP6_ECHO_REPLY:
			ret = mlx5dr_definer_conv_item_icmp6_echo(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_ICMP6;
			break;
		case RTE_FLOW_ITEM_TYPE_METER_COLOR:
			ret = mlx5dr_definer_conv_item_meter_color(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_METER_COLOR;
			break;
		case RTE_FLOW_ITEM_TYPE_QUOTA:
			ret = mlx5dr_definer_conv_item_quota(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_QUOTA;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_ROUTING_EXT:
			ret = mlx5dr_definer_conv_item_ipv6_routing_ext(&cd, items, i);
			item_flags |= cd.tunnel ? MLX5_FLOW_ITEM_INNER_IPV6_ROUTING_EXT :
						  MLX5_FLOW_ITEM_OUTER_IPV6_ROUTING_EXT;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			ret = mlx5dr_definer_conv_item_esp(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_ESP;
			break;
		case RTE_FLOW_ITEM_TYPE_FLEX:
			ret = mlx5dr_definer_conv_item_flex_parser(&cd, items, i);
			if (ret == 0) {
				enum rte_flow_item_flex_tunnel_mode tunnel_mode =
								FLEX_TUNNEL_MODE_SINGLE;

				ret = mlx5_flex_get_tunnel_mode(items, &tunnel_mode);
				if (tunnel_mode == FLEX_TUNNEL_MODE_TUNNEL)
					item_flags |= MLX5_FLOW_ITEM_FLEX_TUNNEL;
				else
					item_flags |= cd.tunnel ? MLX5_FLOW_ITEM_INNER_FLEX :
								  MLX5_FLOW_ITEM_OUTER_FLEX;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_ECPRI:
			ret = mlx5dr_definer_conv_item_ecpri(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_ECPRI;
			break;
		case RTE_FLOW_ITEM_TYPE_MPLS:
			ret = mlx5dr_definer_conv_item_mpls(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_MPLS;
			cd.mpls_idx++;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			ret = mlx5dr_definer_conv_item_geneve(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_GENEVE;
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE_OPT:
			ret = mlx5dr_definer_conv_item_geneve_opt(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_GENEVE_OPT;
			break;
		case RTE_FLOW_ITEM_TYPE_IB_BTH:
			ret = mlx5dr_definer_conv_item_ib_l4(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_IB_BTH;
			break;
		case RTE_FLOW_ITEM_TYPE_PTYPE:
			ret = mlx5dr_definer_conv_item_ptype(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_PTYPE;
			break;
		case RTE_FLOW_ITEM_TYPE_RANDOM:
			ret = mlx5dr_definer_conv_item_random(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_RANDOM;
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN_GPE:
			ret = mlx5dr_definer_conv_item_vxlan_gpe(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_VXLAN_GPE;
			break;
		case RTE_FLOW_ITEM_TYPE_COMPARE:
			if (i) {
				DR_LOG(ERR, "Compare matcher not supported for more than one item");
				goto not_supp;
			}
			ret = mlx5dr_definer_conv_item_compare(&cd, items, i);
			item_flags |= MLX5_FLOW_ITEM_COMPARE;
			matcher->flags |= MLX5DR_MATCHER_FLAGS_COMPARE;
			break;
		case RTE_FLOW_ITEM_TYPE_NSH:
			item_flags |= MLX5_FLOW_ITEM_NSH;
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_NVGRE:
			ret = mlx5dr_definer_conv_item_nvgre(&cd, items, i);
			item_flags |= MLX5_FLOW_LAYER_NVGRE;
			break;
		default:
			DR_LOG(ERR, "Unsupported item type %d", items->type);
			goto not_supp;
		}

		cd.last_item = items->type;

		if (ret) {
			DR_LOG(ERR, "Failed processing item type: %d", items->type);
			return ret;
		}
	}

	mt->item_flags = item_flags;

	/* Fill in headers layout and allocate fc & fcr array on mt */
	ret = mlx5dr_definer_mt_set_fc(mt, fc, hl);
	if (ret) {
		DR_LOG(ERR, "Failed to set field copy to match template");
		return ret;
	}

	return 0;

not_supp:
	rte_errno = ENOTSUP;
	return rte_errno;
}

static int
mlx5dr_definer_find_byte_in_tag(struct mlx5dr_definer *definer,
				uint32_t hl_byte_off,
				uint32_t *tag_byte_off)
{
	uint8_t byte_offset;
	int i, dw_to_scan;

	/* Avoid accessing unused DW selectors */
	dw_to_scan = mlx5dr_definer_is_jumbo(definer) ?
		DW_SELECTORS : DW_SELECTORS_MATCH;

	/* Add offset since each DW covers multiple BYTEs */
	byte_offset = hl_byte_off % DW_SIZE;
	for (i = 0; i < dw_to_scan; i++) {
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
mlx5dr_definer_copy_sel_ctrl(struct mlx5dr_definer_sel_ctrl *ctrl,
			     struct mlx5dr_definer *definer)
{
	memcpy(definer->byte_selector, ctrl->byte_selector, ctrl->allowed_bytes);
	memcpy(definer->dw_selector, ctrl->full_dw_selector, ctrl->allowed_full_dw);
	memcpy(definer->dw_selector + ctrl->allowed_full_dw,
	       ctrl->lim_dw_selector, ctrl->allowed_lim_dw);
}

static int
mlx5dr_definer_find_best_range_fit(struct mlx5dr_definer *definer,
				   struct mlx5dr_matcher *matcher)
{
	uint8_t tag_byte_offset[MLX5DR_DEFINER_FNAME_MAX] = {0};
	uint8_t field_select[MLX5DR_DEFINER_FNAME_MAX] = {0};
	struct mlx5dr_definer_sel_ctrl ctrl = {0};
	uint32_t byte_offset, algn_byte_off;
	struct mlx5dr_definer_fc *fcr;
	bool require_dw;
	int idx, i, j;

	/* Try to create a range definer */
	ctrl.allowed_full_dw = DW_SELECTORS_RANGE;
	ctrl.allowed_bytes = BYTE_SELECTORS_RANGE;

	/* Multiple fields cannot share the same DW for range match.
	 * The HW doesn't recognize each field but compares the full dw.
	 * For example definer DW consists of FieldA_FieldB
	 * FieldA: Mask 0xFFFF range 0x1 to 0x2
	 * FieldB: Mask 0xFFFF range 0x3 to 0x4
	 * STE DW range will be 0x00010003 - 0x00020004
	 * This will cause invalid match for FieldB if FieldA=1 and FieldB=8
	 * Since 0x10003 < 0x10008 < 0x20004
	 */
	for (i = 0; i < matcher->num_of_mt; i++) {
		for (j = 0; j < matcher->mt[i].fcr_sz; j++) {
			fcr = &matcher->mt[i].fcr[j];

			/* Found - Reuse previous mt binding */
			if (field_select[fcr->fname]) {
				fcr->byte_off = tag_byte_offset[fcr->fname];
				continue;
			}

			/* Not found */
			require_dw = fcr->byte_off >= (64 * DW_SIZE);
			if (require_dw || ctrl.used_bytes == ctrl.allowed_bytes) {
				/* Try to cover using DW selector */
				if (ctrl.used_full_dw == ctrl.allowed_full_dw)
					goto not_supported;

				ctrl.full_dw_selector[ctrl.used_full_dw++] =
					fcr->byte_off / DW_SIZE;

				/* Bind DW */
				idx = ctrl.used_full_dw - 1;
				byte_offset = fcr->byte_off % DW_SIZE;
				byte_offset += DW_SIZE * (DW_SELECTORS - idx - 1);
			} else {
				/* Try to cover using Bytes selectors */
				if (ctrl.used_bytes == ctrl.allowed_bytes)
					goto not_supported;

				algn_byte_off = DW_SIZE * (fcr->byte_off / DW_SIZE);
				ctrl.byte_selector[ctrl.used_bytes++] = algn_byte_off + 3;
				ctrl.byte_selector[ctrl.used_bytes++] = algn_byte_off + 2;
				ctrl.byte_selector[ctrl.used_bytes++] = algn_byte_off + 1;
				ctrl.byte_selector[ctrl.used_bytes++] = algn_byte_off;

				/* Bind BYTE */
				byte_offset = DW_SIZE * DW_SELECTORS;
				byte_offset += BYTE_SELECTORS - ctrl.used_bytes;
				byte_offset += fcr->byte_off % DW_SIZE;
			}

			fcr->byte_off = byte_offset;
			tag_byte_offset[fcr->fname] = byte_offset;
			field_select[fcr->fname] = 1;
		}
	}

	mlx5dr_definer_copy_sel_ctrl(&ctrl, definer);
	definer->type = MLX5DR_DEFINER_TYPE_RANGE;

	return 0;

not_supported:
	DR_LOG(ERR, "Unable to find supporting range definer combination");
	rte_errno = ENOTSUP;
	return rte_errno;
}

static void mlx5dr_definer_optimize_order(struct mlx5dr_definer *definer, int num_log)
{
	uint8_t hl_prio[MLX5DR_DEFINER_HL_OPT_MAX];
	int dw = 0, i = 0, j;
	int *dw_flag;
	uint8_t tmp;

	dw_flag = mlx5dr_optimal_dist_dw[num_log];
	hl_prio[0] = __mlx5_dw_off(definer_hl, ipv4_src_dest_outer.source_address);
	hl_prio[1] = __mlx5_dw_off(definer_hl, ipv4_src_dest_outer.destination_address);

	while (i < MLX5DR_DEFINER_HL_OPT_MAX) {
		j = 0;
		/* Finding a candidate to improve its hash distribution */
		while (j < DW_SELECTORS_MATCH && (hl_prio[i] != definer->dw_selector[j]))
			j++;

		/* Finding a DW location with good hash distribution */
		while (dw < DW_SELECTORS_MATCH && dw_flag[dw] == 0)
			dw++;

		if (dw < DW_SELECTORS_MATCH && j < DW_SELECTORS_MATCH) {
			tmp = definer->dw_selector[dw];
			definer->dw_selector[dw] = definer->dw_selector[j];
			definer->dw_selector[j] = tmp;
			dw++;
		}
		i++;
	}
}

static int
mlx5dr_definer_find_best_match_fit(struct mlx5dr_context *ctx,
				   struct mlx5dr_definer *definer,
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
		mlx5dr_definer_copy_sel_ctrl(&ctrl, definer);
		definer->type = MLX5DR_DEFINER_TYPE_MATCH;
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
		mlx5dr_definer_copy_sel_ctrl(&ctrl, definer);
		definer->type = MLX5DR_DEFINER_TYPE_JUMBO;
		return 0;
	}

	DR_LOG(DEBUG, "Unable to find supporting match/jumbo definer combination");
	rte_errno = E2BIG;
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

static uint32_t mlx5dr_definer_get_range_byte_off(uint32_t match_byte_off)
{
	uint8_t curr_dw_idx = match_byte_off / DW_SIZE;
	uint8_t new_dw_idx;

	/* Range DW can have the following values 7,8,9,10
	 * -DW7 is mapped to DW9
	 * -DW8 is mapped to DW7
	 * -DW9 is mapped to DW5
	 * -DW10 is mapped to DW3
	 * To reduce calculation the following formula is used:
	 */
	new_dw_idx = curr_dw_idx * (-2) + 23;

	return new_dw_idx * DW_SIZE + match_byte_off % DW_SIZE;
}

void mlx5dr_definer_create_tag_range(const struct rte_flow_item *items,
				     struct mlx5dr_definer_fc *fc,
				     uint32_t fc_sz,
				     uint8_t *tag)
{
	struct mlx5dr_definer_fc tmp_fc;
	uint32_t i;

	for (i = 0; i < fc_sz; i++) {
		tmp_fc = *fc;
		/* Set MAX value */
		tmp_fc.byte_off = mlx5dr_definer_get_range_byte_off(fc->byte_off);
		tmp_fc.tag_set(&tmp_fc, items[fc->item_idx].last, tag);
		/* Set MIN value */
		tmp_fc.byte_off += DW_SIZE;
		tmp_fc.tag_set(&tmp_fc, items[fc->item_idx].spec, tag);
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

	/* Future: Optimize by comparing selectors with valid mask only */
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

static int
mlx5dr_definer_optimize_order_supported(struct mlx5dr_definer *match_definer,
					struct mlx5dr_matcher *matcher)
{
	return !mlx5dr_definer_is_jumbo(match_definer) &&
	       !mlx5dr_matcher_req_fw_wqe(matcher) &&
	       !mlx5dr_matcher_is_resizable(matcher) &&
	       !mlx5dr_matcher_is_insert_by_idx(matcher);
}

static int
mlx5dr_definer_calc_layout(struct mlx5dr_matcher *matcher,
			   struct mlx5dr_definer *match_definer,
			   struct mlx5dr_definer *range_definer)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	struct mlx5dr_match_template *mt = matcher->mt;
	struct mlx5dr_definer_fc *fc;
	uint8_t *match_hl;
	int i, ret;

	/* Union header-layout (hl) is used for creating a single definer
	 * field layout used with different bitmasks for hash and match.
	 */
	match_hl = simple_calloc(1, MLX5_ST_SZ_BYTES(definer_hl));
	if (!match_hl) {
		DR_LOG(ERR, "Failed to allocate memory for header layout");
		rte_errno = ENOMEM;
		return rte_errno;
	}

	/* Convert all mt items to header layout (hl)
	 * and allocate the match and range field copy array (fc & fcr).
	 */
	for (i = 0; i < matcher->num_of_mt; i++) {
		ret = mlx5dr_definer_conv_items_to_hl(ctx, &mt[i], match_hl, matcher);
		if (ret) {
			DR_LOG(ERR, "Failed to convert items to header layout");
			goto free_fc;
		}
	}

	if (mlx5dr_matcher_is_compare(matcher)) {
		ret = mlx5dr_matcher_validate_compare_attr(matcher);
		if (ret)
			goto free_fc;

		/* Due some HW limitation need to fill unused
		 * DW's 0-5 and byte selectors with 0xff.
		 */
		for (i = 0; i < DW_SELECTORS_MATCH; i++)
			match_definer->dw_selector[i] = 0xff;

		for (i = 0; i < BYTE_SELECTORS; i++)
			match_definer->byte_selector[i] = 0xff;

		for (i = 0; i < mt[0].fc_sz; i++) {
			fc = &mt[0].fc[i];
			match_definer->dw_selector[fc->compare_idx] = fc->byte_off / DW_SIZE;
		}

		goto out;
	}

	/* Find the match definer layout for header layout match union */
	ret = mlx5dr_definer_find_best_match_fit(ctx, match_definer, match_hl);
	if (ret) {
		DR_LOG(DEBUG, "Failed to create match definer from header layout");
		goto free_fc;
	}

	if (mlx5dr_definer_optimize_order_supported(match_definer, matcher))
		mlx5dr_definer_optimize_order(match_definer, matcher->attr.rule.num_log);

	/* Find the range definer layout for match templates fcrs */
	ret = mlx5dr_definer_find_best_range_fit(range_definer, matcher);
	if (ret) {
		DR_LOG(ERR, "Failed to create range definer from header layout");
		goto free_fc;
	}

out:
	simple_free(match_hl);
	return 0;

free_fc:
	for (i = 0; i < matcher->num_of_mt; i++)
		if (mt[i].fc)
			simple_free(mt[i].fc);

	simple_free(match_hl);
	return rte_errno;
}

int mlx5dr_definer_init_cache(struct mlx5dr_definer_cache **cache)
{
	struct mlx5dr_definer_cache *new_cache;

	new_cache = simple_calloc(1, sizeof(*new_cache));
	if (!new_cache) {
		rte_errno = ENOMEM;
		return rte_errno;
	}
	LIST_INIT(&new_cache->head);
	*cache = new_cache;

	return 0;
}

void mlx5dr_definer_uninit_cache(struct mlx5dr_definer_cache *cache)
{
	simple_free(cache);
}

static struct mlx5dr_devx_obj *
mlx5dr_definer_get_obj(struct mlx5dr_context *ctx,
		       struct mlx5dr_definer *definer)
{
	struct mlx5dr_definer_cache *cache = ctx->definer_cache;
	struct mlx5dr_cmd_definer_create_attr def_attr = {0};
	struct mlx5dr_definer_cache_item *cached_definer;
	struct mlx5dr_devx_obj *obj;

	/* Search definer cache for requested definer */
	LIST_FOREACH(cached_definer, &cache->head, next) {
		if (mlx5dr_definer_compare(&cached_definer->definer, definer))
			continue;

		/* Reuse definer and set LRU (move to be first in the list) */
		LIST_REMOVE(cached_definer, next);
		LIST_INSERT_HEAD(&cache->head, cached_definer, next);
		cached_definer->refcount++;
		return cached_definer->definer.obj;
	}

	/* Allocate and create definer based on the bitmask tag */
	def_attr.match_mask = definer->mask.jumbo;
	def_attr.dw_selector = definer->dw_selector;
	def_attr.byte_selector = definer->byte_selector;

	obj = mlx5dr_cmd_definer_create(ctx->ibv_ctx, &def_attr);
	if (!obj)
		return NULL;

	cached_definer = simple_calloc(1, sizeof(*cached_definer));
	if (!cached_definer) {
		rte_errno = ENOMEM;
		goto free_definer_obj;
	}

	memcpy(&cached_definer->definer, definer, sizeof(*definer));
	cached_definer->definer.obj = obj;
	cached_definer->refcount = 1;
	LIST_INSERT_HEAD(&cache->head, cached_definer, next);

	return obj;

free_definer_obj:
	mlx5dr_cmd_destroy_obj(obj);
	return NULL;
}

static void
mlx5dr_definer_put_obj(struct mlx5dr_context *ctx,
		       struct mlx5dr_devx_obj *obj)
{
	struct mlx5dr_definer_cache_item *cached_definer;

	LIST_FOREACH(cached_definer, &ctx->definer_cache->head, next) {
		if (cached_definer->definer.obj != obj)
			continue;

		/* Object found */
		if (--cached_definer->refcount)
			return;

		LIST_REMOVE(cached_definer, next);
		mlx5dr_cmd_destroy_obj(cached_definer->definer.obj);
		simple_free(cached_definer);
		return;
	}

	/* Programming error, object must be part of cache */
	assert(false);
}

static struct mlx5dr_definer *
mlx5dr_definer_alloc(struct mlx5dr_context *ctx,
		     struct mlx5dr_definer_fc *fc,
		     int fc_sz,
		     struct rte_flow_item *items,
		     struct mlx5dr_definer *layout,
		     bool bind_fc)
{
	struct mlx5dr_definer *definer;
	int ret;

	definer = simple_calloc(1, sizeof(*definer));
	if (!definer) {
		DR_LOG(ERR, "Failed to allocate memory for definer");
		rte_errno = ENOMEM;
		return NULL;
	}

	memcpy(definer, layout, sizeof(*definer));

	/* Align field copy array based on given layout */
	if (bind_fc) {
		ret = mlx5dr_definer_fc_bind(definer, fc, fc_sz);
		if (ret) {
			DR_LOG(ERR, "Failed to bind field copy to definer");
			goto free_definer;
		}
	}

	/* Create the tag mask used for definer creation */
	mlx5dr_definer_create_tag_mask(items, fc, fc_sz, definer->mask.jumbo);

	definer->obj = mlx5dr_definer_get_obj(ctx, definer);
	if (!definer->obj)
		goto free_definer;

	return definer;

free_definer:
	simple_free(definer);
	return NULL;
}

static void
mlx5dr_definer_free(struct mlx5dr_context *ctx,
		    struct mlx5dr_definer *definer)
{
	mlx5dr_definer_put_obj(ctx, definer->obj);
	simple_free(definer);
}

static int
mlx5dr_definer_matcher_match_init(struct mlx5dr_context *ctx,
				  struct mlx5dr_matcher *matcher,
				  struct mlx5dr_definer *match_layout)
{
	struct mlx5dr_match_template *mt = matcher->mt;
	int i;

	/* Create mendatory match definer */
	for (i = 0; i < matcher->num_of_mt; i++) {
		mt[i].definer = mlx5dr_definer_alloc(ctx,
						     mt[i].fc,
						     mt[i].fc_sz,
						     mt[i].items,
						     match_layout,
						     true);
		if (!mt[i].definer) {
			DR_LOG(ERR, "Failed to create match definer");
			goto free_definers;
		}
	}
	return 0;

free_definers:
	while (i--)
		mlx5dr_definer_free(ctx, mt[i].definer);

	return rte_errno;
}

static void
mlx5dr_definer_matcher_match_uninit(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	int i;

	for (i = 0; i < matcher->num_of_mt; i++)
		mlx5dr_definer_free(ctx, matcher->mt[i].definer);
}

static int
mlx5dr_definer_matcher_range_init(struct mlx5dr_context *ctx,
				  struct mlx5dr_matcher *matcher,
				  struct mlx5dr_definer *range_layout)
{
	struct mlx5dr_match_template *mt = matcher->mt;
	int i;

	/* Create optional range definers */
	for (i = 0; i < matcher->num_of_mt; i++) {
		/* All must use range if requested */
		bool is_range = !!mt[i].fcr_sz;
		bool has_range = matcher->flags & MLX5DR_MATCHER_FLAGS_RANGE_DEFINER;

		if (i && ((is_range && !has_range) || (!is_range && has_range))) {
			DR_LOG(ERR, "Using range and non range templates is not allowed");
			rte_errno = EINVAL;
			goto free_definers;
		}

		if (!mt[i].fcr_sz)
			continue;

		matcher->flags |= MLX5DR_MATCHER_FLAGS_RANGE_DEFINER;
		/* Create definer without fcr binding, already binded */
		mt[i].range_definer = mlx5dr_definer_alloc(ctx,
							   mt[i].fcr,
							   mt[i].fcr_sz,
							   mt[i].items,
							   range_layout,
							   false);
		if (!mt[i].range_definer) {
			DR_LOG(ERR, "Failed to create match definer");
			goto free_definers;
		}
	}
	return 0;

free_definers:
	while (i--)
		if (mt[i].range_definer)
			mlx5dr_definer_free(ctx, mt[i].range_definer);

	return rte_errno;
}

static void
mlx5dr_definer_matcher_range_uninit(struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_context *ctx = matcher->tbl->ctx;
	int i;

	for (i = 0; i < matcher->num_of_mt; i++)
		if (matcher->mt[i].range_definer)
			mlx5dr_definer_free(ctx, matcher->mt[i].range_definer);
}

static int
mlx5dr_definer_matcher_hash_init(struct mlx5dr_context *ctx,
				 struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_cmd_definer_create_attr def_attr = {0};
	struct mlx5dr_match_template *mt = matcher->mt;
	struct ibv_context *ibv_ctx = ctx->ibv_ctx;
	uint8_t *bit_mask;
	int i, j;

	for (i = 1; i < matcher->num_of_mt; i++)
		if (mlx5dr_definer_compare(mt[i].definer, mt[i - 1].definer))
			matcher->flags |= MLX5DR_MATCHER_FLAGS_HASH_DEFINER;

	if (!(matcher->flags & MLX5DR_MATCHER_FLAGS_HASH_DEFINER))
		return 0;

	/* Insert by index requires all MT using the same definer */
	if (matcher->attr.insert_mode == MLX5DR_MATCHER_INSERT_BY_INDEX) {
		DR_LOG(ERR, "Insert by index not supported with MT combination");
		rte_errno = EOPNOTSUPP;
		return rte_errno;
	}

	matcher->hash_definer = simple_calloc(1, sizeof(*matcher->hash_definer));
	if (!matcher->hash_definer) {
		DR_LOG(ERR, "Failed to allocate memory for hash definer");
		rte_errno = ENOMEM;
		return rte_errno;
	}

	/* Calculate intersection between all match templates bitmasks.
	 * We will use mt[0] as reference and intersect it with mt[1..n].
	 * From this we will get:
	 * hash_definer.selectors = mt[0].selecotrs
	 * hash_definer.mask =  mt[0].mask & mt[0].mask & ... & mt[n].mask
	 */

	/* Use first definer which should also contain intersection fields */
	memcpy(matcher->hash_definer, mt->definer, sizeof(struct mlx5dr_definer));

	/* Calculate intersection between first to all match templates bitmasks */
	for (i = 1; i < matcher->num_of_mt; i++) {
		bit_mask = (uint8_t *)&mt[i].definer->mask;
		for (j = 0; j < MLX5DR_JUMBO_TAG_SZ; j++)
			((uint8_t *)&matcher->hash_definer->mask)[j] &= bit_mask[j];
	}

	def_attr.match_mask = matcher->hash_definer->mask.jumbo;
	def_attr.dw_selector = matcher->hash_definer->dw_selector;
	def_attr.byte_selector = matcher->hash_definer->byte_selector;
	matcher->hash_definer->obj = mlx5dr_cmd_definer_create(ibv_ctx, &def_attr);
	if (!matcher->hash_definer->obj) {
		DR_LOG(ERR, "Failed to create hash definer");
		goto free_hash_definer;
	}

	return 0;

free_hash_definer:
	simple_free(matcher->hash_definer);
	return rte_errno;
}

static void
mlx5dr_definer_matcher_hash_uninit(struct mlx5dr_matcher *matcher)
{
	if (!matcher->hash_definer)
		return;

	mlx5dr_cmd_destroy_obj(matcher->hash_definer->obj);
	simple_free(matcher->hash_definer);
}

int mlx5dr_definer_matcher_init(struct mlx5dr_context *ctx,
				struct mlx5dr_matcher *matcher)
{
	struct mlx5dr_definer match_layout = {0};
	struct mlx5dr_definer range_layout = {0};
	int ret, i;

	if (matcher->flags & MLX5DR_MATCHER_FLAGS_COLLISION)
		return 0;

	ret = mlx5dr_definer_calc_layout(matcher, &match_layout, &range_layout);
	if (ret) {
		DR_LOG(DEBUG, "Failed to calculate matcher definer layout");
		return ret;
	}

	/* Calculate definers needed for exact match */
	ret = mlx5dr_definer_matcher_match_init(ctx, matcher, &match_layout);
	if (ret) {
		DR_LOG(ERR, "Failed to init match definers");
		goto free_fc;
	}

	/* Calculate definers needed for range */
	ret = mlx5dr_definer_matcher_range_init(ctx, matcher, &range_layout);
	if (ret) {
		DR_LOG(ERR, "Failed to init range definers");
		goto uninit_match_definer;
	}

	/* Calculate partial hash definer */
	ret = mlx5dr_definer_matcher_hash_init(ctx, matcher);
	if (ret) {
		DR_LOG(ERR, "Failed to init hash definer");
		goto uninit_range_definer;
	}

	return 0;

uninit_range_definer:
	mlx5dr_definer_matcher_range_uninit(matcher);
uninit_match_definer:
	mlx5dr_definer_matcher_match_uninit(matcher);
free_fc:
	for (i = 0; i < matcher->num_of_mt; i++)
		simple_free(matcher->mt[i].fc);

	return ret;
}

void mlx5dr_definer_matcher_uninit(struct mlx5dr_matcher *matcher)
{
	int i;

	if (matcher->flags & MLX5DR_MATCHER_FLAGS_COLLISION)
		return;

	mlx5dr_definer_matcher_hash_uninit(matcher);
	mlx5dr_definer_matcher_range_uninit(matcher);
	mlx5dr_definer_matcher_match_uninit(matcher);

	for (i = 0; i < matcher->num_of_mt; i++)
		simple_free(matcher->mt[i].fc);
}
