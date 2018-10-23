/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_PRM_H_
#define RTE_PMD_MLX5_PRM_H_

#include <assert.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_vect.h>
#include "mlx5_autoconf.h"

/* RSS hash key size. */
#define MLX5_RSS_HASH_KEY_LEN 40

/* Get CQE owner bit. */
#define MLX5_CQE_OWNER(op_own) ((op_own) & MLX5_CQE_OWNER_MASK)

/* Get CQE format. */
#define MLX5_CQE_FORMAT(op_own) (((op_own) & MLX5E_CQE_FORMAT_MASK) >> 2)

/* Get CQE opcode. */
#define MLX5_CQE_OPCODE(op_own) (((op_own) & 0xf0) >> 4)

/* Get CQE solicited event. */
#define MLX5_CQE_SE(op_own) (((op_own) >> 1) & 1)

/* Invalidate a CQE. */
#define MLX5_CQE_INVALIDATE (MLX5_CQE_INVALID << 4)

/* Maximum number of packets a multi-packet WQE can handle. */
#define MLX5_MPW_DSEG_MAX 5

/* WQE DWORD size */
#define MLX5_WQE_DWORD_SIZE 16

/* WQE size */
#define MLX5_WQE_SIZE (4 * MLX5_WQE_DWORD_SIZE)

/* Max size of a WQE session. */
#define MLX5_WQE_SIZE_MAX 960U

/* Compute the number of DS. */
#define MLX5_WQE_DS(n) \
	(((n) + MLX5_WQE_DWORD_SIZE - 1) / MLX5_WQE_DWORD_SIZE)

/* Room for inline data in multi-packet WQE. */
#define MLX5_MWQE64_INL_DATA 28

/* Default minimum number of Tx queues for inlining packets. */
#define MLX5_EMPW_MIN_TXQS 8

/* Default max packet length to be inlined. */
#define MLX5_EMPW_MAX_INLINE_LEN (4U * MLX5_WQE_SIZE)


#define MLX5_OPC_MOD_ENHANCED_MPSW 0
#define MLX5_OPCODE_ENHANCED_MPSW 0x29

/* CQE value to inform that VLAN is stripped. */
#define MLX5_CQE_VLAN_STRIPPED (1u << 0)

/* IPv4 options. */
#define MLX5_CQE_RX_IP_EXT_OPTS_PACKET (1u << 1)

/* IPv6 packet. */
#define MLX5_CQE_RX_IPV6_PACKET (1u << 2)

/* IPv4 packet. */
#define MLX5_CQE_RX_IPV4_PACKET (1u << 3)

/* TCP packet. */
#define MLX5_CQE_RX_TCP_PACKET (1u << 4)

/* UDP packet. */
#define MLX5_CQE_RX_UDP_PACKET (1u << 5)

/* IP is fragmented. */
#define MLX5_CQE_RX_IP_FRAG_PACKET (1u << 7)

/* L2 header is valid. */
#define MLX5_CQE_RX_L2_HDR_VALID (1u << 8)

/* L3 header is valid. */
#define MLX5_CQE_RX_L3_HDR_VALID (1u << 9)

/* L4 header is valid. */
#define MLX5_CQE_RX_L4_HDR_VALID (1u << 10)

/* Outer packet, 0 IPv4, 1 IPv6. */
#define MLX5_CQE_RX_OUTER_PACKET (1u << 1)

/* Tunnel packet bit in the CQE. */
#define MLX5_CQE_RX_TUNNEL_PACKET (1u << 0)

/* Inner L3 checksum offload (Tunneled packets only). */
#define MLX5_ETH_WQE_L3_INNER_CSUM (1u << 4)

/* Inner L4 checksum offload (Tunneled packets only). */
#define MLX5_ETH_WQE_L4_INNER_CSUM (1u << 5)

/* Outer L4 type is TCP. */
#define MLX5_ETH_WQE_L4_OUTER_TCP  (0u << 5)

/* Outer L4 type is UDP. */
#define MLX5_ETH_WQE_L4_OUTER_UDP  (1u << 5)

/* Outer L3 type is IPV4. */
#define MLX5_ETH_WQE_L3_OUTER_IPV4 (0u << 4)

/* Outer L3 type is IPV6. */
#define MLX5_ETH_WQE_L3_OUTER_IPV6 (1u << 4)

/* Inner L4 type is TCP. */
#define MLX5_ETH_WQE_L4_INNER_TCP (0u << 1)

/* Inner L4 type is UDP. */
#define MLX5_ETH_WQE_L4_INNER_UDP (1u << 1)

/* Inner L3 type is IPV4. */
#define MLX5_ETH_WQE_L3_INNER_IPV4 (0u << 0)

/* Inner L3 type is IPV6. */
#define MLX5_ETH_WQE_L3_INNER_IPV6 (1u << 0)

/* Is flow mark valid. */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
#define MLX5_FLOW_MARK_IS_VALID(val) ((val) & 0xffffff00)
#else
#define MLX5_FLOW_MARK_IS_VALID(val) ((val) & 0xffffff)
#endif

/* INVALID is used by packets matching no flow rules. */
#define MLX5_FLOW_MARK_INVALID 0

/* Maximum allowed value to mark a packet. */
#define MLX5_FLOW_MARK_MAX 0xfffff0

/* Default mark value used when none is provided. */
#define MLX5_FLOW_MARK_DEFAULT 0xffffff

/* Maximum number of DS in WQE. */
#define MLX5_DSEG_MAX 63

/* Subset of struct mlx5_wqe_eth_seg. */
struct mlx5_wqe_eth_seg_small {
	uint32_t rsvd0;
	uint8_t	cs_flags;
	uint8_t	rsvd1;
	uint16_t mss;
	uint32_t flow_table_metadata;
	uint16_t inline_hdr_sz;
	uint8_t inline_hdr[2];
} __rte_aligned(MLX5_WQE_DWORD_SIZE);

struct mlx5_wqe_inl_small {
	uint32_t byte_cnt;
	uint8_t raw;
} __rte_aligned(MLX5_WQE_DWORD_SIZE);

struct mlx5_wqe_ctrl {
	uint32_t ctrl0;
	uint32_t ctrl1;
	uint32_t ctrl2;
	uint32_t ctrl3;
} __rte_aligned(MLX5_WQE_DWORD_SIZE);

/* Small common part of the WQE. */
struct mlx5_wqe {
	uint32_t ctrl[4];
	struct mlx5_wqe_eth_seg_small eseg;
};

/* Vectorize WQE header. */
struct mlx5_wqe_v {
	rte_v128u32_t ctrl;
	rte_v128u32_t eseg;
};

/* WQE. */
struct mlx5_wqe64 {
	struct mlx5_wqe hdr;
	uint8_t raw[32];
} __rte_aligned(MLX5_WQE_SIZE);

/* MPW mode. */
enum mlx5_mpw_mode {
	MLX5_MPW_DISABLED,
	MLX5_MPW,
	MLX5_MPW_ENHANCED, /* Enhanced Multi-Packet Send WQE, a.k.a MPWv2. */
};

/* MPW session status. */
enum mlx5_mpw_state {
	MLX5_MPW_STATE_OPENED,
	MLX5_MPW_INL_STATE_OPENED,
	MLX5_MPW_ENHANCED_STATE_OPENED,
	MLX5_MPW_STATE_CLOSED,
};

/* MPW session descriptor. */
struct mlx5_mpw {
	enum mlx5_mpw_state state;
	unsigned int pkts_n;
	unsigned int len;
	unsigned int total_len;
	volatile struct mlx5_wqe *wqe;
	union {
		volatile struct mlx5_wqe_data_seg *dseg[MLX5_MPW_DSEG_MAX];
		volatile uint8_t *raw;
	} data;
};

/* WQE for Multi-Packet RQ. */
struct mlx5_wqe_mprq {
	struct mlx5_wqe_srq_next_seg next_seg;
	struct mlx5_wqe_data_seg dseg;
};

#define MLX5_MPRQ_LEN_MASK 0x000ffff
#define MLX5_MPRQ_LEN_SHIFT 0
#define MLX5_MPRQ_STRIDE_NUM_MASK 0x3fff0000
#define MLX5_MPRQ_STRIDE_NUM_SHIFT 16
#define MLX5_MPRQ_FILLER_MASK 0x80000000
#define MLX5_MPRQ_FILLER_SHIFT 31

#define MLX5_MPRQ_STRIDE_SHIFT_BYTE 2

/* CQ element structure - should be equal to the cache line size */
struct mlx5_cqe {
#if (RTE_CACHE_LINE_SIZE == 128)
	uint8_t padding[64];
#endif
	uint8_t pkt_info;
	uint8_t rsvd0;
	uint16_t wqe_id;
	uint8_t rsvd3[8];
	uint32_t rx_hash_res;
	uint8_t rx_hash_type;
	uint8_t rsvd1[11];
	uint16_t hdr_type_etc;
	uint16_t vlan_info;
	uint8_t rsvd2[12];
	uint32_t byte_cnt;
	uint64_t timestamp;
	uint32_t sop_drop_qpn;
	uint16_t wqe_counter;
	uint8_t rsvd4;
	uint8_t op_own;
};

/* Adding direct verbs to data-path. */

/* CQ sequence number mask. */
#define MLX5_CQ_SQN_MASK 0x3

/* CQ sequence number index. */
#define MLX5_CQ_SQN_OFFSET 28

/* CQ doorbell index mask. */
#define MLX5_CI_MASK 0xffffff

/* CQ doorbell offset. */
#define MLX5_CQ_ARM_DB 1

/* CQ doorbell offset*/
#define MLX5_CQ_DOORBELL 0x20

/* CQE format value. */
#define MLX5_COMPRESSED 0x3

/* The field of packet to be modified. */
enum mlx5_modificaiton_field {
	MLX5_MODI_OUT_SMAC_47_16 = 1,
	MLX5_MODI_OUT_SMAC_15_0,
	MLX5_MODI_OUT_ETHERTYPE,
	MLX5_MODI_OUT_DMAC_47_16,
	MLX5_MODI_OUT_DMAC_15_0,
	MLX5_MODI_OUT_IP_DSCP,
	MLX5_MODI_OUT_TCP_FLAGS,
	MLX5_MODI_OUT_TCP_SPORT,
	MLX5_MODI_OUT_TCP_DPORT,
	MLX5_MODI_OUT_IPV4_TTL,
	MLX5_MODI_OUT_UDP_SPORT,
	MLX5_MODI_OUT_UDP_DPORT,
	MLX5_MODI_OUT_SIPV6_127_96,
	MLX5_MODI_OUT_SIPV6_95_64,
	MLX5_MODI_OUT_SIPV6_63_32,
	MLX5_MODI_OUT_SIPV6_31_0,
	MLX5_MODI_OUT_DIPV6_127_96,
	MLX5_MODI_OUT_DIPV6_95_64,
	MLX5_MODI_OUT_DIPV6_63_32,
	MLX5_MODI_OUT_DIPV6_31_0,
	MLX5_MODI_OUT_SIPV4,
	MLX5_MODI_OUT_DIPV4,
	MLX5_MODI_IN_SMAC_47_16 = 0x31,
	MLX5_MODI_IN_SMAC_15_0,
	MLX5_MODI_IN_ETHERTYPE,
	MLX5_MODI_IN_DMAC_47_16,
	MLX5_MODI_IN_DMAC_15_0,
	MLX5_MODI_IN_IP_DSCP,
	MLX5_MODI_IN_TCP_FLAGS,
	MLX5_MODI_IN_TCP_SPORT,
	MLX5_MODI_IN_TCP_DPORT,
	MLX5_MODI_IN_IPV4_TTL,
	MLX5_MODI_IN_UDP_SPORT,
	MLX5_MODI_IN_UDP_DPORT,
	MLX5_MODI_IN_SIPV6_127_96,
	MLX5_MODI_IN_SIPV6_95_64,
	MLX5_MODI_IN_SIPV6_63_32,
	MLX5_MODI_IN_SIPV6_31_0,
	MLX5_MODI_IN_DIPV6_127_96,
	MLX5_MODI_IN_DIPV6_95_64,
	MLX5_MODI_IN_DIPV6_63_32,
	MLX5_MODI_IN_DIPV6_31_0,
	MLX5_MODI_IN_SIPV4,
	MLX5_MODI_IN_DIPV4,
	MLX5_MODI_OUT_IPV6_HOPLIMIT,
	MLX5_MODI_IN_IPV6_HOPLIMIT,
	MLX5_MODI_META_DATA_REG_A,
	MLX5_MODI_META_DATA_REG_B = 0x50,
};

/* Modification sub command. */
struct mlx5_modification_cmd {
	union {
		uint32_t data0;
		struct {
			unsigned int bits:5;
			unsigned int rsvd0:3;
			unsigned int src_offset:5; /* Start bit offset. */
			unsigned int rsvd1:3;
			unsigned int src_field:12;
			unsigned int type:4;
		};
	};
	union {
		uint32_t data1;
		uint8_t data[4];
		struct {
			unsigned int rsvd2:8;
			unsigned int dst_offset:8;
			unsigned int dst_field:12;
			unsigned int rsvd3:4;
		};
	};
};

typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#define __mlx5_nullp(typ) ((struct mlx5_ifc_##typ##_bits *)0)
#define __mlx5_bit_sz(typ, fld) sizeof(__mlx5_nullp(typ)->fld)
#define __mlx5_bit_off(typ, fld) ((unsigned int)(unsigned long) \
				  (&(__mlx5_nullp(typ)->fld)))
#define __mlx5_dw_bit_off(typ, fld) (32 - __mlx5_bit_sz(typ, fld) - \
				    (__mlx5_bit_off(typ, fld) & 0x1f))
#define __mlx5_dw_off(typ, fld) (__mlx5_bit_off(typ, fld) / 32)
#define __mlx5_dw_mask(typ, fld) (__mlx5_mask(typ, fld) << \
				  __mlx5_dw_bit_off(typ, fld))
#define __mlx5_mask(typ, fld) ((u32)((1ull << __mlx5_bit_sz(typ, fld)) - 1))
#define __mlx5_16_off(typ, fld) (__mlx5_bit_off(typ, fld) / 16)
#define __mlx5_16_bit_off(typ, fld) (16 - __mlx5_bit_sz(typ, fld) - \
				    (__mlx5_bit_off(typ, fld) & 0xf))
#define __mlx5_mask16(typ, fld) ((u16)((1ull << __mlx5_bit_sz(typ, fld)) - 1))
#define MLX5_ST_SZ_DW(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 32)
#define MLX5_ST_SZ_DB(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 8)
#define MLX5_BYTE_OFF(typ, fld) (__mlx5_bit_off(typ, fld) / 8)
#define MLX5_ADDR_OF(typ, p, fld) ((char *)(p) + MLX5_BYTE_OFF(typ, fld))

/* insert a value to a struct */
#define MLX5_SET(typ, p, fld, v) \
	do { \
		u32 _v = v; \
		*((__be32 *)(p) + __mlx5_dw_off(typ, fld)) = \
		rte_cpu_to_be_32((rte_be_to_cpu_32(*((u32 *)(p) + \
				  __mlx5_dw_off(typ, fld))) & \
				  (~__mlx5_dw_mask(typ, fld))) | \
				 (((_v) & __mlx5_mask(typ, fld)) << \
				   __mlx5_dw_bit_off(typ, fld))); \
	} while (0)
#define MLX5_GET16(typ, p, fld) \
	((rte_be_to_cpu_16(*((__be16 *)(p) + \
	  __mlx5_16_off(typ, fld))) >> __mlx5_16_bit_off(typ, fld)) & \
	 __mlx5_mask16(typ, fld))
#define MLX5_FLD_SZ_BYTES(typ, fld) (__mlx5_bit_sz(typ, fld) / 8)

struct mlx5_ifc_fte_match_set_misc_bits {
	u8 reserved_at_0[0x8];
	u8 source_sqn[0x18];
	u8 reserved_at_20[0x10];
	u8 source_port[0x10];
	u8 outer_second_prio[0x3];
	u8 outer_second_cfi[0x1];
	u8 outer_second_vid[0xc];
	u8 inner_second_prio[0x3];
	u8 inner_second_cfi[0x1];
	u8 inner_second_vid[0xc];
	u8 outer_second_cvlan_tag[0x1];
	u8 inner_second_cvlan_tag[0x1];
	u8 outer_second_svlan_tag[0x1];
	u8 inner_second_svlan_tag[0x1];
	u8 reserved_at_64[0xc];
	u8 gre_protocol[0x10];
	u8 gre_key_h[0x18];
	u8 gre_key_l[0x8];
	u8 vxlan_vni[0x18];
	u8 reserved_at_b8[0x8];
	u8 reserved_at_c0[0x20];
	u8 reserved_at_e0[0xc];
	u8 outer_ipv6_flow_label[0x14];
	u8 reserved_at_100[0xc];
	u8 inner_ipv6_flow_label[0x14];
	u8 reserved_at_120[0xe0];
};

struct mlx5_ifc_ipv4_layout_bits {
	u8 reserved_at_0[0x60];
	u8 ipv4[0x20];
};

struct mlx5_ifc_ipv6_layout_bits {
	u8 ipv6[16][0x8];
};

union mlx5_ifc_ipv6_layout_ipv4_layout_auto_bits {
	struct mlx5_ifc_ipv6_layout_bits ipv6_layout;
	struct mlx5_ifc_ipv4_layout_bits ipv4_layout;
	u8 reserved_at_0[0x80];
};

struct mlx5_ifc_fte_match_set_lyr_2_4_bits {
	u8 smac_47_16[0x20];
	u8 smac_15_0[0x10];
	u8 ethertype[0x10];
	u8 dmac_47_16[0x20];
	u8 dmac_15_0[0x10];
	u8 first_prio[0x3];
	u8 first_cfi[0x1];
	u8 first_vid[0xc];
	u8 ip_protocol[0x8];
	u8 ip_dscp[0x6];
	u8 ip_ecn[0x2];
	u8 cvlan_tag[0x1];
	u8 svlan_tag[0x1];
	u8 frag[0x1];
	u8 ip_version[0x4];
	u8 tcp_flags[0x9];
	u8 tcp_sport[0x10];
	u8 tcp_dport[0x10];
	u8 reserved_at_c0[0x20];
	u8 udp_sport[0x10];
	u8 udp_dport[0x10];
	union mlx5_ifc_ipv6_layout_ipv4_layout_auto_bits src_ipv4_src_ipv6;
	union mlx5_ifc_ipv6_layout_ipv4_layout_auto_bits dst_ipv4_dst_ipv6;
};

struct mlx5_ifc_fte_match_mpls_bits {
	u8 mpls_label[0x14];
	u8 mpls_exp[0x3];
	u8 mpls_s_bos[0x1];
	u8 mpls_ttl[0x8];
};

struct mlx5_ifc_fte_match_set_misc2_bits {
	struct mlx5_ifc_fte_match_mpls_bits outer_first_mpls;
	struct mlx5_ifc_fte_match_mpls_bits inner_first_mpls;
	struct mlx5_ifc_fte_match_mpls_bits outer_first_mpls_over_gre;
	struct mlx5_ifc_fte_match_mpls_bits outer_first_mpls_over_udp;
	u8 reserved_at_80[0x100];
	u8 metadata_reg_a[0x20];
	u8 reserved_at_1a0[0x60];
};

/* Flow matcher. */
struct mlx5_ifc_fte_match_param_bits {
	struct mlx5_ifc_fte_match_set_lyr_2_4_bits outer_headers;
	struct mlx5_ifc_fte_match_set_misc_bits misc_parameters;
	struct mlx5_ifc_fte_match_set_lyr_2_4_bits inner_headers;
	struct mlx5_ifc_fte_match_set_misc2_bits misc_parameters_2;
	u8 reserved_at_800[0x800];
};

enum {
	MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_INNER_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC2_BIT
};

/* CQE format mask. */
#define MLX5E_CQE_FORMAT_MASK 0xc

/* MPW opcode. */
#define MLX5_OPC_MOD_MPW 0x01

/* Compressed Rx CQE structure. */
struct mlx5_mini_cqe8 {
	union {
		uint32_t rx_hash_result;
		struct {
			uint16_t checksum;
			uint16_t stride_idx;
		};
		struct {
			uint16_t wqe_counter;
			uint8_t  s_wqe_opcode;
			uint8_t  reserved;
		} s_wqe_info;
	};
	uint32_t byte_cnt;
};

/**
 * Convert a user mark to flow mark.
 *
 * @param val
 *   Mark value to convert.
 *
 * @return
 *   Converted mark value.
 */
static inline uint32_t
mlx5_flow_mark_set(uint32_t val)
{
	uint32_t ret;

	/*
	 * Add one to the user value to differentiate un-marked flows from
	 * marked flows, if the ID is equal to MLX5_FLOW_MARK_DEFAULT it
	 * remains untouched.
	 */
	if (val != MLX5_FLOW_MARK_DEFAULT)
		++val;
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	/*
	 * Mark is 24 bits (minus reserved values) but is stored on a 32 bit
	 * word, byte-swapped by the kernel on little-endian systems. In this
	 * case, left-shifting the resulting big-endian value ensures the
	 * least significant 24 bits are retained when converting it back.
	 */
	ret = rte_cpu_to_be_32(val) >> 8;
#else
	ret = val;
#endif
	return ret;
}

/**
 * Convert a mark to user mark.
 *
 * @param val
 *   Mark value to convert.
 *
 * @return
 *   Converted mark value.
 */
static inline uint32_t
mlx5_flow_mark_get(uint32_t val)
{
	/*
	 * Subtract one from the retrieved value. It was added by
	 * mlx5_flow_mark_set() to distinguish unmarked flows.
	 */
#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
	return (val >> 8) - 1;
#else
	return val - 1;
#endif
}

#endif /* RTE_PMD_MLX5_PRM_H_ */
