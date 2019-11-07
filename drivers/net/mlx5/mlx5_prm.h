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

/* WQE Segment sizes in bytes. */
#define MLX5_WSEG_SIZE 16u
#define MLX5_WQE_CSEG_SIZE sizeof(struct mlx5_wqe_cseg)
#define MLX5_WQE_DSEG_SIZE sizeof(struct mlx5_wqe_dseg)
#define MLX5_WQE_ESEG_SIZE sizeof(struct mlx5_wqe_eseg)

/* WQE/WQEBB size in bytes. */
#define MLX5_WQE_SIZE sizeof(struct mlx5_wqe)

/*
 * Max size of a WQE session.
 * Absolute maximum size is 63 (MLX5_DSEG_MAX) segments,
 * the WQE size field in Control Segment is 6 bits wide.
 */
#define MLX5_WQE_SIZE_MAX (60 * MLX5_WSEG_SIZE)

/*
 * Default minimum number of Tx queues for inlining packets.
 * If there are less queues as specified we assume we have
 * no enough CPU resources (cycles) to perform inlining,
 * the PCIe throughput is not supposed as bottleneck and
 * inlining is disabled.
 */
#define MLX5_INLINE_MAX_TXQS 8u
#define MLX5_INLINE_MAX_TXQS_BLUEFIELD 16u

/*
 * Default packet length threshold to be inlined with
 * enhanced MPW. If packet length exceeds the threshold
 * the data are not inlined. Should be aligned in WQEBB
 * boundary with accounting the title Control and Ethernet
 * segments.
 */
#define MLX5_EMPW_DEF_INLINE_LEN (4u * MLX5_WQE_SIZE + \
				  MLX5_DSEG_MIN_INLINE_SIZE)
/*
 * Maximal inline data length sent with enhanced MPW.
 * Is based on maximal WQE size.
 */
#define MLX5_EMPW_MAX_INLINE_LEN (MLX5_WQE_SIZE_MAX - \
				  MLX5_WQE_CSEG_SIZE - \
				  MLX5_WQE_ESEG_SIZE - \
				  MLX5_WQE_DSEG_SIZE + \
				  MLX5_DSEG_MIN_INLINE_SIZE)
/*
 * Minimal amount of packets to be sent with EMPW.
 * This limits the minimal required size of sent EMPW.
 * If there are no enough resources to built minimal
 * EMPW the sending loop exits.
 */
#define MLX5_EMPW_MIN_PACKETS (2u + 3u * 4u)
/*
 * Maximal amount of packets to be sent with EMPW.
 * This value is not recommended to exceed MLX5_TX_COMP_THRESH,
 * otherwise there might be up to MLX5_EMPW_MAX_PACKETS mbufs
 * without CQE generation request, being multiplied by
 * MLX5_TX_COMP_MAX_CQE it may cause significant latency
 * in tx burst routine at the moment of freeing multiple mbufs.
 */
#define MLX5_EMPW_MAX_PACKETS MLX5_TX_COMP_THRESH
/*
 * Default packet length threshold to be inlined with
 * ordinary SEND. Inlining saves the MR key search
 * and extra PCIe data fetch transaction, but eats the
 * CPU cycles.
 */
#define MLX5_SEND_DEF_INLINE_LEN (5U * MLX5_WQE_SIZE + \
				  MLX5_ESEG_MIN_INLINE_SIZE - \
				  MLX5_WQE_CSEG_SIZE - \
				  MLX5_WQE_ESEG_SIZE - \
				  MLX5_WQE_DSEG_SIZE)
/*
 * Maximal inline data length sent with ordinary SEND.
 * Is based on maximal WQE size.
 */
#define MLX5_SEND_MAX_INLINE_LEN (MLX5_WQE_SIZE_MAX - \
				  MLX5_WQE_CSEG_SIZE - \
				  MLX5_WQE_ESEG_SIZE - \
				  MLX5_WQE_DSEG_SIZE + \
				  MLX5_ESEG_MIN_INLINE_SIZE)

/* Missed in mlv5dv.h, should define here. */
#define MLX5_OPCODE_ENHANCED_MPSW 0x29u

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

/* Mask for LRO push flag in the CQE lro_tcppsh_abort_dupack field. */
#define MLX5_CQE_LRO_PUSH_MASK 0x40

/* Mask for L4 type in the CQE hdr_type_etc field. */
#define MLX5_CQE_L4_TYPE_MASK 0x70

/* The bit index of L4 type in CQE hdr_type_etc field. */
#define MLX5_CQE_L4_TYPE_SHIFT 0x4

/* L4 type to indicate TCP packet without acknowledgment. */
#define MLX5_L4_HDR_TYPE_TCP_EMPTY_ACK 0x3

/* L4 type to indicate TCP packet with acknowledgment. */
#define MLX5_L4_HDR_TYPE_TCP_WITH_ACL 0x4

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

/* VLAN insertion flag. */
#define MLX5_ETH_WQE_VLAN_INSERT (1u << 31)

/* Data inline segment flag. */
#define MLX5_ETH_WQE_DATA_INLINE (1u << 31)

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

/* Default mark mask for metadata legacy mode. */
#define MLX5_FLOW_MARK_MASK 0xffffff

/* Maximum number of DS in WQE. Limited by 6-bit field. */
#define MLX5_DSEG_MAX 63

/* The completion mode offset in the WQE control segment line 2. */
#define MLX5_COMP_MODE_OFFSET 2

/* Amount of data bytes in minimal inline data segment. */
#define MLX5_DSEG_MIN_INLINE_SIZE 12u

/* Amount of data bytes in minimal inline eth segment. */
#define MLX5_ESEG_MIN_INLINE_SIZE 18u

/* Amount of data bytes after eth data segment. */
#define MLX5_ESEG_EXTRA_DATA_SIZE 32u

/* The maximum log value of segments per RQ WQE. */
#define MLX5_MAX_LOG_RQ_SEGS 5u

/* The alignment needed for WQ buffer. */
#define MLX5_WQE_BUF_ALIGNMENT 512

/* Completion mode. */
enum mlx5_completion_mode {
	MLX5_COMP_ONLY_ERR = 0x0,
	MLX5_COMP_ONLY_FIRST_ERR = 0x1,
	MLX5_COMP_ALWAYS = 0x2,
	MLX5_COMP_CQE_AND_EQE = 0x3,
};

/* MPW mode. */
enum mlx5_mpw_mode {
	MLX5_MPW_DISABLED,
	MLX5_MPW,
	MLX5_MPW_ENHANCED, /* Enhanced Multi-Packet Send WQE, a.k.a MPWv2. */
};

/* WQE Control segment. */
struct mlx5_wqe_cseg {
	uint32_t opcode;
	uint32_t sq_ds;
	uint32_t flags;
	uint32_t misc;
} __rte_packed __rte_aligned(MLX5_WSEG_SIZE);

/* Header of data segment. Minimal size Data Segment */
struct mlx5_wqe_dseg {
	uint32_t bcount;
	union {
		uint8_t inline_data[MLX5_DSEG_MIN_INLINE_SIZE];
		struct {
			uint32_t lkey;
			uint64_t pbuf;
		} __rte_packed;
	};
} __rte_packed;

/* Subset of struct WQE Ethernet Segment. */
struct mlx5_wqe_eseg {
	union {
		struct {
			uint32_t swp_offs;
			uint8_t	cs_flags;
			uint8_t	swp_flags;
			uint16_t mss;
			uint32_t metadata;
			uint16_t inline_hdr_sz;
			union {
				uint16_t inline_data;
				uint16_t vlan_tag;
			};
		} __rte_packed;
		struct {
			uint32_t offsets;
			uint32_t flags;
			uint32_t flow_metadata;
			uint32_t inline_hdr;
		} __rte_packed;
	};
} __rte_packed;

/* The title WQEBB, header of WQE. */
struct mlx5_wqe {
	union {
		struct mlx5_wqe_cseg cseg;
		uint32_t ctrl[4];
	};
	struct mlx5_wqe_eseg eseg;
	union {
		struct mlx5_wqe_dseg dseg[2];
		uint8_t data[MLX5_ESEG_EXTRA_DATA_SIZE];
	};
} __rte_packed;

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
	uint8_t lro_tcppsh_abort_dupack;
	uint8_t lro_min_ttl;
	uint16_t lro_tcp_win;
	uint32_t lro_ack_seq_num;
	uint32_t rx_hash_res;
	uint8_t rx_hash_type;
	uint8_t rsvd1[3];
	uint16_t csum;
	uint8_t rsvd2[6];
	uint16_t hdr_type_etc;
	uint16_t vlan_info;
	uint8_t lro_num_seg;
	uint8_t rsvd3[11];
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

/* Action type of header modification. */
enum {
	MLX5_MODIFICATION_TYPE_SET = 0x1,
	MLX5_MODIFICATION_TYPE_ADD = 0x2,
	MLX5_MODIFICATION_TYPE_COPY = 0x3,
};

/* The field of packet to be modified. */
enum mlx5_modification_field {
	MLX5_MODI_OUT_NONE = -1,
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
	MLX5_MODI_OUT_FIRST_VID,
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
	MLX5_MODI_META_REG_C_0,
	MLX5_MODI_META_REG_C_1,
	MLX5_MODI_META_REG_C_2,
	MLX5_MODI_META_REG_C_3,
	MLX5_MODI_META_REG_C_4,
	MLX5_MODI_META_REG_C_5,
	MLX5_MODI_META_REG_C_6,
	MLX5_MODI_META_REG_C_7,
	MLX5_MODI_OUT_TCP_SEQ_NUM,
	MLX5_MODI_IN_TCP_SEQ_NUM,
	MLX5_MODI_OUT_TCP_ACK_NUM,
	MLX5_MODI_IN_TCP_ACK_NUM = 0x5C,
};

/* Total number of metadata reg_c's. */
#define MLX5_MREG_C_NUM (MLX5_MODI_META_REG_C_7 - MLX5_MODI_META_REG_C_0 + 1)

enum modify_reg {
	REG_NONE = 0,
	REG_A,
	REG_B,
	REG_C_0,
	REG_C_1,
	REG_C_2,
	REG_C_3,
	REG_C_4,
	REG_C_5,
	REG_C_6,
	REG_C_7,
};

/* Modification sub command. */
struct mlx5_modification_cmd {
	union {
		uint32_t data0;
		struct {
			unsigned int length:5;
			unsigned int rsvd0:3;
			unsigned int offset:5;
			unsigned int rsvd1:3;
			unsigned int field:12;
			unsigned int action_type:4;
		};
	};
	union {
		uint32_t data1;
		uint8_t data[4];
		struct {
			unsigned int rsvd2:8;
			unsigned int dst_offset:5;
			unsigned int rsvd3:3;
			unsigned int dst_field:12;
			unsigned int rsvd4:4;
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
#define __mlx5_64_off(typ, fld) (__mlx5_bit_off(typ, fld) / 64)
#define __mlx5_dw_mask(typ, fld) (__mlx5_mask(typ, fld) << \
				  __mlx5_dw_bit_off(typ, fld))
#define __mlx5_mask(typ, fld) ((u32)((1ull << __mlx5_bit_sz(typ, fld)) - 1))
#define __mlx5_16_off(typ, fld) (__mlx5_bit_off(typ, fld) / 16)
#define __mlx5_16_bit_off(typ, fld) (16 - __mlx5_bit_sz(typ, fld) - \
				    (__mlx5_bit_off(typ, fld) & 0xf))
#define __mlx5_mask16(typ, fld) ((u16)((1ull << __mlx5_bit_sz(typ, fld)) - 1))
#define MLX5_ST_SZ_BYTES(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 8)
#define MLX5_ST_SZ_DW(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 32)
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

#define MLX5_SET64(typ, p, fld, v) \
	do { \
		assert(__mlx5_bit_sz(typ, fld) == 64); \
		*((__be64 *)(p) + __mlx5_64_off(typ, fld)) = \
			rte_cpu_to_be_64(v); \
	} while (0)

#define MLX5_GET(typ, p, fld) \
	((rte_be_to_cpu_32(*((__be32 *)(p) +\
	__mlx5_dw_off(typ, fld))) >> __mlx5_dw_bit_off(typ, fld)) & \
	__mlx5_mask(typ, fld))
#define MLX5_GET16(typ, p, fld) \
	((rte_be_to_cpu_16(*((__be16 *)(p) + \
	  __mlx5_16_off(typ, fld))) >> __mlx5_16_bit_off(typ, fld)) & \
	 __mlx5_mask16(typ, fld))
#define MLX5_GET64(typ, p, fld) rte_be_to_cpu_64(*((__be64 *)(p) + \
						   __mlx5_64_off(typ, fld)))
#define MLX5_FLD_SZ_BYTES(typ, fld) (__mlx5_bit_sz(typ, fld) / 8)

struct mlx5_ifc_fte_match_set_misc_bits {
	u8 gre_c_present[0x1];
	u8 reserved_at_1[0x1];
	u8 gre_k_present[0x1];
	u8 gre_s_present[0x1];
	u8 source_vhci_port[0x4];
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
	u8 geneve_vni[0x18];
	u8 reserved_at_e4[0x7];
	u8 geneve_oam[0x1];
	u8 reserved_at_e0[0xc];
	u8 outer_ipv6_flow_label[0x14];
	u8 reserved_at_100[0xc];
	u8 inner_ipv6_flow_label[0x14];
	u8 reserved_at_120[0xa];
	u8 geneve_opt_len[0x6];
	u8 geneve_protocol_type[0x10];
	u8 reserved_at_140[0xc0];
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
	u8 metadata_reg_c_7[0x20];
	u8 metadata_reg_c_6[0x20];
	u8 metadata_reg_c_5[0x20];
	u8 metadata_reg_c_4[0x20];
	u8 metadata_reg_c_3[0x20];
	u8 metadata_reg_c_2[0x20];
	u8 metadata_reg_c_1[0x20];
	u8 metadata_reg_c_0[0x20];
	u8 metadata_reg_a[0x20];
	u8 metadata_reg_b[0x20];
	u8 reserved_at_1c0[0x40];
};

struct mlx5_ifc_fte_match_set_misc3_bits {
	u8 inner_tcp_seq_num[0x20];
	u8 outer_tcp_seq_num[0x20];
	u8 inner_tcp_ack_num[0x20];
	u8 outer_tcp_ack_num[0x20];
	u8 reserved_at_auto1[0x8];
	u8 outer_vxlan_gpe_vni[0x18];
	u8 outer_vxlan_gpe_next_protocol[0x8];
	u8 outer_vxlan_gpe_flags[0x8];
	u8 reserved_at_a8[0x10];
	u8 icmp_header_data[0x20];
	u8 icmpv6_header_data[0x20];
	u8 icmp_type[0x8];
	u8 icmp_code[0x8];
	u8 icmpv6_type[0x8];
	u8 icmpv6_code[0x8];
	u8 reserved_at_1a0[0xe0];
};

/* Flow matcher. */
struct mlx5_ifc_fte_match_param_bits {
	struct mlx5_ifc_fte_match_set_lyr_2_4_bits outer_headers;
	struct mlx5_ifc_fte_match_set_misc_bits misc_parameters;
	struct mlx5_ifc_fte_match_set_lyr_2_4_bits inner_headers;
	struct mlx5_ifc_fte_match_set_misc2_bits misc_parameters_2;
	struct mlx5_ifc_fte_match_set_misc3_bits misc_parameters_3;
};

enum {
	MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_INNER_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC2_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC3_BIT
};

enum {
	MLX5_CMD_OP_QUERY_HCA_CAP = 0x100,
	MLX5_CMD_OP_CREATE_MKEY = 0x200,
	MLX5_CMD_OP_QUERY_NIC_VPORT_CONTEXT = 0x754,
	MLX5_CMD_OP_ALLOC_TRANSPORT_DOMAIN = 0x816,
	MLX5_CMD_OP_CREATE_TIR = 0x900,
	MLX5_CMD_OP_CREATE_SQ = 0X904,
	MLX5_CMD_OP_MODIFY_SQ = 0X905,
	MLX5_CMD_OP_CREATE_RQ = 0x908,
	MLX5_CMD_OP_MODIFY_RQ = 0x909,
	MLX5_CMD_OP_CREATE_TIS = 0x912,
	MLX5_CMD_OP_QUERY_TIS = 0x915,
	MLX5_CMD_OP_CREATE_RQT = 0x916,
	MLX5_CMD_OP_ALLOC_FLOW_COUNTER = 0x939,
	MLX5_CMD_OP_QUERY_FLOW_COUNTER = 0x93b,
};

enum {
	MLX5_MKC_ACCESS_MODE_MTT   = 0x1,
};

/* Flow counters. */
struct mlx5_ifc_alloc_flow_counter_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];
	u8         syndrome[0x20];
	u8         flow_counter_id[0x20];
	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_alloc_flow_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];
	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];
	u8         flow_counter_id[0x20];
	u8         reserved_at_40[0x18];
	u8         flow_counter_bulk[0x8];
};

struct mlx5_ifc_dealloc_flow_counter_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];
	u8         syndrome[0x20];
	u8         reserved_at_40[0x40];
};

struct mlx5_ifc_dealloc_flow_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];
	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];
	u8         flow_counter_id[0x20];
	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_traffic_counter_bits {
	u8         packets[0x40];
	u8         octets[0x40];
};

struct mlx5_ifc_query_flow_counter_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];
	u8         syndrome[0x20];
	u8         reserved_at_40[0x40];
	struct mlx5_ifc_traffic_counter_bits flow_statistics[];
};

struct mlx5_ifc_query_flow_counter_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];
	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];
	u8         reserved_at_40[0x20];
	u8         mkey[0x20];
	u8         address[0x40];
	u8         clear[0x1];
	u8         dump_to_memory[0x1];
	u8         num_of_counters[0x1e];
	u8         flow_counter_id[0x20];
};

struct mlx5_ifc_mkc_bits {
	u8         reserved_at_0[0x1];
	u8         free[0x1];
	u8         reserved_at_2[0x1];
	u8         access_mode_4_2[0x3];
	u8         reserved_at_6[0x7];
	u8         relaxed_ordering_write[0x1];
	u8         reserved_at_e[0x1];
	u8         small_fence_on_rdma_read_response[0x1];
	u8         umr_en[0x1];
	u8         a[0x1];
	u8         rw[0x1];
	u8         rr[0x1];
	u8         lw[0x1];
	u8         lr[0x1];
	u8         access_mode_1_0[0x2];
	u8         reserved_at_18[0x8];

	u8         qpn[0x18];
	u8         mkey_7_0[0x8];

	u8         reserved_at_40[0x20];

	u8         length64[0x1];
	u8         bsf_en[0x1];
	u8         sync_umr[0x1];
	u8         reserved_at_63[0x2];
	u8         expected_sigerr_count[0x1];
	u8         reserved_at_66[0x1];
	u8         en_rinval[0x1];
	u8         pd[0x18];

	u8         start_addr[0x40];

	u8         len[0x40];

	u8         bsf_octword_size[0x20];

	u8         reserved_at_120[0x80];

	u8         translations_octword_size[0x20];

	u8         reserved_at_1c0[0x1b];
	u8         log_page_size[0x5];

	u8         reserved_at_1e0[0x20];
};

struct mlx5_ifc_create_mkey_out_bits {
	u8         status[0x8];
	u8         reserved_at_8[0x18];

	u8         syndrome[0x20];

	u8         reserved_at_40[0x8];
	u8         mkey_index[0x18];

	u8         reserved_at_60[0x20];
};

struct mlx5_ifc_create_mkey_in_bits {
	u8         opcode[0x10];
	u8         reserved_at_10[0x10];

	u8         reserved_at_20[0x10];
	u8         op_mod[0x10];

	u8         reserved_at_40[0x20];

	u8         pg_access[0x1];
	u8         reserved_at_61[0x1f];

	struct mlx5_ifc_mkc_bits memory_key_mkey_entry;

	u8         reserved_at_280[0x80];

	u8         translations_octword_actual_size[0x20];

	u8         mkey_umem_id[0x20];

	u8         mkey_umem_offset[0x40];

	u8         reserved_at_380[0x500];

	u8         klm_pas_mtt[][0x20];
};

enum {
	MLX5_GET_HCA_CAP_OP_MOD_GENERAL_DEVICE = 0x0 << 1,
	MLX5_GET_HCA_CAP_OP_MOD_ETHERNET_OFFLOAD_CAPS = 0x1 << 1,
	MLX5_GET_HCA_CAP_OP_MOD_QOS_CAP = 0xc << 1,
};

enum {
	MLX5_HCA_CAP_OPMOD_GET_MAX   = 0,
	MLX5_HCA_CAP_OPMOD_GET_CUR   = 1,
};

enum {
	MLX5_CAP_INLINE_MODE_L2,
	MLX5_CAP_INLINE_MODE_VPORT_CONTEXT,
	MLX5_CAP_INLINE_MODE_NOT_REQUIRED,
};

enum {
	MLX5_INLINE_MODE_NONE,
	MLX5_INLINE_MODE_L2,
	MLX5_INLINE_MODE_IP,
	MLX5_INLINE_MODE_TCP_UDP,
	MLX5_INLINE_MODE_RESERVED4,
	MLX5_INLINE_MODE_INNER_L2,
	MLX5_INLINE_MODE_INNER_IP,
	MLX5_INLINE_MODE_INNER_TCP_UDP,
};

/* HCA bit masks indicating which Flex parser protocols are already enabled. */
#define MLX5_HCA_FLEX_IPV4_OVER_VXLAN_ENABLED (1UL << 0)
#define MLX5_HCA_FLEX_IPV6_OVER_VXLAN_ENABLED (1UL << 1)
#define MLX5_HCA_FLEX_IPV6_OVER_IP_ENABLED (1UL << 2)
#define MLX5_HCA_FLEX_GENEVE_ENABLED (1UL << 3)
#define MLX5_HCA_FLEX_CW_MPLS_OVER_GRE_ENABLED (1UL << 4)
#define MLX5_HCA_FLEX_CW_MPLS_OVER_UDP_ENABLED (1UL << 5)
#define MLX5_HCA_FLEX_P_BIT_VXLAN_GPE_ENABLED (1UL << 6)
#define MLX5_HCA_FLEX_VXLAN_GPE_ENABLED (1UL << 7)
#define MLX5_HCA_FLEX_ICMP_ENABLED (1UL << 8)
#define MLX5_HCA_FLEX_ICMPV6_ENABLED (1UL << 9)

struct mlx5_ifc_cmd_hca_cap_bits {
	u8 reserved_at_0[0x30];
	u8 vhca_id[0x10];
	u8 reserved_at_40[0x40];
	u8 log_max_srq_sz[0x8];
	u8 log_max_qp_sz[0x8];
	u8 reserved_at_90[0xb];
	u8 log_max_qp[0x5];
	u8 reserved_at_a0[0xb];
	u8 log_max_srq[0x5];
	u8 reserved_at_b0[0x10];
	u8 reserved_at_c0[0x8];
	u8 log_max_cq_sz[0x8];
	u8 reserved_at_d0[0xb];
	u8 log_max_cq[0x5];
	u8 log_max_eq_sz[0x8];
	u8 reserved_at_e8[0x2];
	u8 log_max_mkey[0x6];
	u8 reserved_at_f0[0x8];
	u8 dump_fill_mkey[0x1];
	u8 reserved_at_f9[0x3];
	u8 log_max_eq[0x4];
	u8 max_indirection[0x8];
	u8 fixed_buffer_size[0x1];
	u8 log_max_mrw_sz[0x7];
	u8 force_teardown[0x1];
	u8 reserved_at_111[0x1];
	u8 log_max_bsf_list_size[0x6];
	u8 umr_extended_translation_offset[0x1];
	u8 null_mkey[0x1];
	u8 log_max_klm_list_size[0x6];
	u8 reserved_at_120[0xa];
	u8 log_max_ra_req_dc[0x6];
	u8 reserved_at_130[0xa];
	u8 log_max_ra_res_dc[0x6];
	u8 reserved_at_140[0xa];
	u8 log_max_ra_req_qp[0x6];
	u8 reserved_at_150[0xa];
	u8 log_max_ra_res_qp[0x6];
	u8 end_pad[0x1];
	u8 cc_query_allowed[0x1];
	u8 cc_modify_allowed[0x1];
	u8 start_pad[0x1];
	u8 cache_line_128byte[0x1];
	u8 reserved_at_165[0xa];
	u8 qcam_reg[0x1];
	u8 gid_table_size[0x10];
	u8 out_of_seq_cnt[0x1];
	u8 vport_counters[0x1];
	u8 retransmission_q_counters[0x1];
	u8 debug[0x1];
	u8 modify_rq_counter_set_id[0x1];
	u8 rq_delay_drop[0x1];
	u8 max_qp_cnt[0xa];
	u8 pkey_table_size[0x10];
	u8 vport_group_manager[0x1];
	u8 vhca_group_manager[0x1];
	u8 ib_virt[0x1];
	u8 eth_virt[0x1];
	u8 vnic_env_queue_counters[0x1];
	u8 ets[0x1];
	u8 nic_flow_table[0x1];
	u8 eswitch_manager[0x1];
	u8 device_memory[0x1];
	u8 mcam_reg[0x1];
	u8 pcam_reg[0x1];
	u8 local_ca_ack_delay[0x5];
	u8 port_module_event[0x1];
	u8 enhanced_error_q_counters[0x1];
	u8 ports_check[0x1];
	u8 reserved_at_1b3[0x1];
	u8 disable_link_up[0x1];
	u8 beacon_led[0x1];
	u8 port_type[0x2];
	u8 num_ports[0x8];
	u8 reserved_at_1c0[0x1];
	u8 pps[0x1];
	u8 pps_modify[0x1];
	u8 log_max_msg[0x5];
	u8 reserved_at_1c8[0x4];
	u8 max_tc[0x4];
	u8 temp_warn_event[0x1];
	u8 dcbx[0x1];
	u8 general_notification_event[0x1];
	u8 reserved_at_1d3[0x2];
	u8 fpga[0x1];
	u8 rol_s[0x1];
	u8 rol_g[0x1];
	u8 reserved_at_1d8[0x1];
	u8 wol_s[0x1];
	u8 wol_g[0x1];
	u8 wol_a[0x1];
	u8 wol_b[0x1];
	u8 wol_m[0x1];
	u8 wol_u[0x1];
	u8 wol_p[0x1];
	u8 stat_rate_support[0x10];
	u8 reserved_at_1f0[0xc];
	u8 cqe_version[0x4];
	u8 compact_address_vector[0x1];
	u8 striding_rq[0x1];
	u8 reserved_at_202[0x1];
	u8 ipoib_enhanced_offloads[0x1];
	u8 ipoib_basic_offloads[0x1];
	u8 reserved_at_205[0x1];
	u8 repeated_block_disabled[0x1];
	u8 umr_modify_entity_size_disabled[0x1];
	u8 umr_modify_atomic_disabled[0x1];
	u8 umr_indirect_mkey_disabled[0x1];
	u8 umr_fence[0x2];
	u8 reserved_at_20c[0x3];
	u8 drain_sigerr[0x1];
	u8 cmdif_checksum[0x2];
	u8 sigerr_cqe[0x1];
	u8 reserved_at_213[0x1];
	u8 wq_signature[0x1];
	u8 sctr_data_cqe[0x1];
	u8 reserved_at_216[0x1];
	u8 sho[0x1];
	u8 tph[0x1];
	u8 rf[0x1];
	u8 dct[0x1];
	u8 qos[0x1];
	u8 eth_net_offloads[0x1];
	u8 roce[0x1];
	u8 atomic[0x1];
	u8 reserved_at_21f[0x1];
	u8 cq_oi[0x1];
	u8 cq_resize[0x1];
	u8 cq_moderation[0x1];
	u8 reserved_at_223[0x3];
	u8 cq_eq_remap[0x1];
	u8 pg[0x1];
	u8 block_lb_mc[0x1];
	u8 reserved_at_229[0x1];
	u8 scqe_break_moderation[0x1];
	u8 cq_period_start_from_cqe[0x1];
	u8 cd[0x1];
	u8 reserved_at_22d[0x1];
	u8 apm[0x1];
	u8 vector_calc[0x1];
	u8 umr_ptr_rlky[0x1];
	u8 imaicl[0x1];
	u8 reserved_at_232[0x4];
	u8 qkv[0x1];
	u8 pkv[0x1];
	u8 set_deth_sqpn[0x1];
	u8 reserved_at_239[0x3];
	u8 xrc[0x1];
	u8 ud[0x1];
	u8 uc[0x1];
	u8 rc[0x1];
	u8 uar_4k[0x1];
	u8 reserved_at_241[0x9];
	u8 uar_sz[0x6];
	u8 reserved_at_250[0x8];
	u8 log_pg_sz[0x8];
	u8 bf[0x1];
	u8 driver_version[0x1];
	u8 pad_tx_eth_packet[0x1];
	u8 reserved_at_263[0x8];
	u8 log_bf_reg_size[0x5];
	u8 reserved_at_270[0xb];
	u8 lag_master[0x1];
	u8 num_lag_ports[0x4];
	u8 reserved_at_280[0x10];
	u8 max_wqe_sz_sq[0x10];
	u8 reserved_at_2a0[0x10];
	u8 max_wqe_sz_rq[0x10];
	u8 max_flow_counter_31_16[0x10];
	u8 max_wqe_sz_sq_dc[0x10];
	u8 reserved_at_2e0[0x7];
	u8 max_qp_mcg[0x19];
	u8 reserved_at_300[0x10];
	u8 flow_counter_bulk_alloc[0x08];
	u8 log_max_mcg[0x8];
	u8 reserved_at_320[0x3];
	u8 log_max_transport_domain[0x5];
	u8 reserved_at_328[0x3];
	u8 log_max_pd[0x5];
	u8 reserved_at_330[0xb];
	u8 log_max_xrcd[0x5];
	u8 nic_receive_steering_discard[0x1];
	u8 receive_discard_vport_down[0x1];
	u8 transmit_discard_vport_down[0x1];
	u8 reserved_at_343[0x5];
	u8 log_max_flow_counter_bulk[0x8];
	u8 max_flow_counter_15_0[0x10];
	u8 modify_tis[0x1];
	u8 flow_counters_dump[0x1];
	u8 reserved_at_360[0x1];
	u8 log_max_rq[0x5];
	u8 reserved_at_368[0x3];
	u8 log_max_sq[0x5];
	u8 reserved_at_370[0x3];
	u8 log_max_tir[0x5];
	u8 reserved_at_378[0x3];
	u8 log_max_tis[0x5];
	u8 basic_cyclic_rcv_wqe[0x1];
	u8 reserved_at_381[0x2];
	u8 log_max_rmp[0x5];
	u8 reserved_at_388[0x3];
	u8 log_max_rqt[0x5];
	u8 reserved_at_390[0x3];
	u8 log_max_rqt_size[0x5];
	u8 reserved_at_398[0x3];
	u8 log_max_tis_per_sq[0x5];
	u8 ext_stride_num_range[0x1];
	u8 reserved_at_3a1[0x2];
	u8 log_max_stride_sz_rq[0x5];
	u8 reserved_at_3a8[0x3];
	u8 log_min_stride_sz_rq[0x5];
	u8 reserved_at_3b0[0x3];
	u8 log_max_stride_sz_sq[0x5];
	u8 reserved_at_3b8[0x3];
	u8 log_min_stride_sz_sq[0x5];
	u8 hairpin[0x1];
	u8 reserved_at_3c1[0x2];
	u8 log_max_hairpin_queues[0x5];
	u8 reserved_at_3c8[0x3];
	u8 log_max_hairpin_wq_data_sz[0x5];
	u8 reserved_at_3d0[0x3];
	u8 log_max_hairpin_num_packets[0x5];
	u8 reserved_at_3d8[0x3];
	u8 log_max_wq_sz[0x5];
	u8 nic_vport_change_event[0x1];
	u8 disable_local_lb_uc[0x1];
	u8 disable_local_lb_mc[0x1];
	u8 log_min_hairpin_wq_data_sz[0x5];
	u8 reserved_at_3e8[0x3];
	u8 log_max_vlan_list[0x5];
	u8 reserved_at_3f0[0x3];
	u8 log_max_current_mc_list[0x5];
	u8 reserved_at_3f8[0x3];
	u8 log_max_current_uc_list[0x5];
	u8 general_obj_types[0x40];
	u8 reserved_at_440[0x20];
	u8 reserved_at_460[0x10];
	u8 max_num_eqs[0x10];
	u8 reserved_at_480[0x3];
	u8 log_max_l2_table[0x5];
	u8 reserved_at_488[0x8];
	u8 log_uar_page_sz[0x10];
	u8 reserved_at_4a0[0x20];
	u8 device_frequency_mhz[0x20];
	u8 device_frequency_khz[0x20];
	u8 reserved_at_500[0x20];
	u8 num_of_uars_per_page[0x20];
	u8 flex_parser_protocols[0x20];
	u8 reserved_at_560[0x20];
	u8 reserved_at_580[0x3c];
	u8 mini_cqe_resp_stride_index[0x1];
	u8 cqe_128_always[0x1];
	u8 cqe_compression_128[0x1];
	u8 cqe_compression[0x1];
	u8 cqe_compression_timeout[0x10];
	u8 cqe_compression_max_num[0x10];
	u8 reserved_at_5e0[0x10];
	u8 tag_matching[0x1];
	u8 rndv_offload_rc[0x1];
	u8 rndv_offload_dc[0x1];
	u8 log_tag_matching_list_sz[0x5];
	u8 reserved_at_5f8[0x3];
	u8 log_max_xrq[0x5];
	u8 affiliate_nic_vport_criteria[0x8];
	u8 native_port_num[0x8];
	u8 num_vhca_ports[0x8];
	u8 reserved_at_618[0x6];
	u8 sw_owner_id[0x1];
	u8 reserved_at_61f[0x1e1];
};

struct mlx5_ifc_qos_cap_bits {
	u8 packet_pacing[0x1];
	u8 esw_scheduling[0x1];
	u8 esw_bw_share[0x1];
	u8 esw_rate_limit[0x1];
	u8 reserved_at_4[0x1];
	u8 packet_pacing_burst_bound[0x1];
	u8 packet_pacing_typical_size[0x1];
	u8 flow_meter_srtcm[0x1];
	u8 reserved_at_8[0x8];
	u8 log_max_flow_meter[0x8];
	u8 flow_meter_reg_id[0x8];
	u8 reserved_at_25[0x20];
	u8 packet_pacing_max_rate[0x20];
	u8 packet_pacing_min_rate[0x20];
	u8 reserved_at_80[0x10];
	u8 packet_pacing_rate_table_size[0x10];
	u8 esw_element_type[0x10];
	u8 esw_tsar_type[0x10];
	u8 reserved_at_c0[0x10];
	u8 max_qos_para_vport[0x10];
	u8 max_tsar_bw_share[0x20];
	u8 reserved_at_100[0x6e8];
};

struct mlx5_ifc_per_protocol_networking_offload_caps_bits {
	u8 csum_cap[0x1];
	u8 vlan_cap[0x1];
	u8 lro_cap[0x1];
	u8 lro_psh_flag[0x1];
	u8 lro_time_stamp[0x1];
	u8 lro_max_msg_sz_mode[0x2];
	u8 wqe_vlan_insert[0x1];
	u8 self_lb_en_modifiable[0x1];
	u8 self_lb_mc[0x1];
	u8 self_lb_uc[0x1];
	u8 max_lso_cap[0x5];
	u8 multi_pkt_send_wqe[0x2];
	u8 wqe_inline_mode[0x2];
	u8 rss_ind_tbl_cap[0x4];
	u8 reg_umr_sq[0x1];
	u8 scatter_fcs[0x1];
	u8 enhanced_multi_pkt_send_wqe[0x1];
	u8 tunnel_lso_const_out_ip_id[0x1];
	u8 tunnel_lro_gre[0x1];
	u8 tunnel_lro_vxlan[0x1];
	u8 tunnel_stateless_gre[0x1];
	u8 tunnel_stateless_vxlan[0x1];
	u8 swp[0x1];
	u8 swp_csum[0x1];
	u8 swp_lso[0x1];
	u8 reserved_at_23[0xd];
	u8 max_vxlan_udp_ports[0x8];
	u8 reserved_at_38[0x6];
	u8 max_geneve_opt_len[0x1];
	u8 tunnel_stateless_geneve_rx[0x1];
	u8 reserved_at_40[0x10];
	u8 lro_min_mss_size[0x10];
	u8 reserved_at_60[0x120];
	u8 lro_timer_supported_periods[4][0x20];
	u8 reserved_at_200[0x600];
};

union mlx5_ifc_hca_cap_union_bits {
	struct mlx5_ifc_cmd_hca_cap_bits cmd_hca_cap;
	struct mlx5_ifc_per_protocol_networking_offload_caps_bits
	       per_protocol_networking_offload_caps;
	struct mlx5_ifc_qos_cap_bits qos_cap;
	u8 reserved_at_0[0x8000];
};

struct mlx5_ifc_query_hca_cap_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	union mlx5_ifc_hca_cap_union_bits capability;
};

struct mlx5_ifc_query_hca_cap_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_mac_address_layout_bits {
	u8 reserved_at_0[0x10];
	u8 mac_addr_47_32[0x10];
	u8 mac_addr_31_0[0x20];
};

struct mlx5_ifc_nic_vport_context_bits {
	u8 reserved_at_0[0x5];
	u8 min_wqe_inline_mode[0x3];
	u8 reserved_at_8[0x15];
	u8 disable_mc_local_lb[0x1];
	u8 disable_uc_local_lb[0x1];
	u8 roce_en[0x1];
	u8 arm_change_event[0x1];
	u8 reserved_at_21[0x1a];
	u8 event_on_mtu[0x1];
	u8 event_on_promisc_change[0x1];
	u8 event_on_vlan_change[0x1];
	u8 event_on_mc_address_change[0x1];
	u8 event_on_uc_address_change[0x1];
	u8 reserved_at_40[0xc];
	u8 affiliation_criteria[0x4];
	u8 affiliated_vhca_id[0x10];
	u8 reserved_at_60[0xd0];
	u8 mtu[0x10];
	u8 system_image_guid[0x40];
	u8 port_guid[0x40];
	u8 node_guid[0x40];
	u8 reserved_at_200[0x140];
	u8 qkey_violation_counter[0x10];
	u8 reserved_at_350[0x430];
	u8 promisc_uc[0x1];
	u8 promisc_mc[0x1];
	u8 promisc_all[0x1];
	u8 reserved_at_783[0x2];
	u8 allowed_list_type[0x3];
	u8 reserved_at_788[0xc];
	u8 allowed_list_size[0xc];
	struct mlx5_ifc_mac_address_layout_bits permanent_address;
	u8 reserved_at_7e0[0x20];
};

struct mlx5_ifc_query_nic_vport_context_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	struct mlx5_ifc_nic_vport_context_bits nic_vport_context;
};

struct mlx5_ifc_query_nic_vport_context_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 other_vport[0x1];
	u8 reserved_at_41[0xf];
	u8 vport_number[0x10];
	u8 reserved_at_60[0x5];
	u8 allowed_list_type[0x3];
	u8 reserved_at_68[0x18];
};

struct mlx5_ifc_tisc_bits {
	u8 strict_lag_tx_port_affinity[0x1];
	u8 reserved_at_1[0x3];
	u8 lag_tx_port_affinity[0x04];
	u8 reserved_at_8[0x4];
	u8 prio[0x4];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x100];
	u8 reserved_at_120[0x8];
	u8 transport_domain[0x18];
	u8 reserved_at_140[0x8];
	u8 underlay_qpn[0x18];
	u8 reserved_at_160[0x3a0];
};

struct mlx5_ifc_query_tis_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	struct mlx5_ifc_tisc_bits tis_context;
};

struct mlx5_ifc_query_tis_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 tisn[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_alloc_transport_domain_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 transport_domain[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_alloc_transport_domain_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x40];
};

enum {
	MLX5_WQ_TYPE_LINKED_LIST                = 0x0,
	MLX5_WQ_TYPE_CYCLIC                     = 0x1,
	MLX5_WQ_TYPE_LINKED_LIST_STRIDING_RQ    = 0x2,
	MLX5_WQ_TYPE_CYCLIC_STRIDING_RQ         = 0x3,
};

enum {
	MLX5_WQ_END_PAD_MODE_NONE  = 0x0,
	MLX5_WQ_END_PAD_MODE_ALIGN = 0x1,
};

struct mlx5_ifc_wq_bits {
	u8 wq_type[0x4];
	u8 wq_signature[0x1];
	u8 end_padding_mode[0x2];
	u8 cd_slave[0x1];
	u8 reserved_at_8[0x18];
	u8 hds_skip_first_sge[0x1];
	u8 log2_hds_buf_size[0x3];
	u8 reserved_at_24[0x7];
	u8 page_offset[0x5];
	u8 lwm[0x10];
	u8 reserved_at_40[0x8];
	u8 pd[0x18];
	u8 reserved_at_60[0x8];
	u8 uar_page[0x18];
	u8 dbr_addr[0x40];
	u8 hw_counter[0x20];
	u8 sw_counter[0x20];
	u8 reserved_at_100[0xc];
	u8 log_wq_stride[0x4];
	u8 reserved_at_110[0x3];
	u8 log_wq_pg_sz[0x5];
	u8 reserved_at_118[0x3];
	u8 log_wq_sz[0x5];
	u8 dbr_umem_valid[0x1];
	u8 wq_umem_valid[0x1];
	u8 reserved_at_122[0x1];
	u8 log_hairpin_num_packets[0x5];
	u8 reserved_at_128[0x3];
	u8 log_hairpin_data_sz[0x5];
	u8 reserved_at_130[0x4];
	u8 single_wqe_log_num_of_strides[0x4];
	u8 two_byte_shift_en[0x1];
	u8 reserved_at_139[0x4];
	u8 single_stride_log_num_of_bytes[0x3];
	u8 dbr_umem_id[0x20];
	u8 wq_umem_id[0x20];
	u8 wq_umem_offset[0x40];
	u8 reserved_at_1c0[0x440];
};

enum {
	MLX5_RQC_MEM_RQ_TYPE_MEMORY_RQ_INLINE  = 0x0,
	MLX5_RQC_MEM_RQ_TYPE_MEMORY_RQ_RMP     = 0x1,
};

enum {
	MLX5_RQC_STATE_RST  = 0x0,
	MLX5_RQC_STATE_RDY  = 0x1,
	MLX5_RQC_STATE_ERR  = 0x3,
};

struct mlx5_ifc_rqc_bits {
	u8 rlky[0x1];
	u8 delay_drop_en[0x1];
	u8 scatter_fcs[0x1];
	u8 vsd[0x1];
	u8 mem_rq_type[0x4];
	u8 state[0x4];
	u8 reserved_at_c[0x1];
	u8 flush_in_error_en[0x1];
	u8 hairpin[0x1];
	u8 reserved_at_f[0x11];
	u8 reserved_at_20[0x8];
	u8 user_index[0x18];
	u8 reserved_at_40[0x8];
	u8 cqn[0x18];
	u8 counter_set_id[0x8];
	u8 reserved_at_68[0x18];
	u8 reserved_at_80[0x8];
	u8 rmpn[0x18];
	u8 reserved_at_a0[0x8];
	u8 hairpin_peer_sq[0x18];
	u8 reserved_at_c0[0x10];
	u8 hairpin_peer_vhca[0x10];
	u8 reserved_at_e0[0xa0];
	struct mlx5_ifc_wq_bits wq; /* Not used in LRO RQ. */
};

struct mlx5_ifc_create_rq_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 rqn[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_create_rq_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0xc0];
	struct mlx5_ifc_rqc_bits ctx;
};

struct mlx5_ifc_modify_rq_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_create_tis_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 tisn[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_create_tis_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0xc0];
	struct mlx5_ifc_tisc_bits ctx;
};

enum {
	MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_WQ_LWM = 1ULL << 0,
	MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_VSD = 1ULL << 1,
	MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_SCATTER_FCS = 1ULL << 2,
	MLX5_MODIFY_RQ_IN_MODIFY_BITMASK_RQ_COUNTER_SET_ID = 1ULL << 3,
};

struct mlx5_ifc_modify_rq_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 rq_state[0x4];
	u8 reserved_at_44[0x4];
	u8 rqn[0x18];
	u8 reserved_at_60[0x20];
	u8 modify_bitmask[0x40];
	u8 reserved_at_c0[0x40];
	struct mlx5_ifc_rqc_bits ctx;
};

enum {
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_SRC_IP     = 0x0,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_DST_IP     = 0x1,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_SPORT   = 0x2,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_L4_DPORT   = 0x3,
	MLX5_RX_HASH_FIELD_SELECT_SELECTED_FIELDS_IPSEC_SPI  = 0x4,
};

struct mlx5_ifc_rx_hash_field_select_bits {
	u8 l3_prot_type[0x1];
	u8 l4_prot_type[0x1];
	u8 selected_fields[0x1e];
};

enum {
	MLX5_TIRC_DISP_TYPE_DIRECT    = 0x0,
	MLX5_TIRC_DISP_TYPE_INDIRECT  = 0x1,
};

enum {
	MLX5_TIRC_LRO_ENABLE_MASK_IPV4_LRO  = 0x1,
	MLX5_TIRC_LRO_ENABLE_MASK_IPV6_LRO  = 0x2,
};

enum {
	MLX5_RX_HASH_FN_NONE           = 0x0,
	MLX5_RX_HASH_FN_INVERTED_XOR8  = 0x1,
	MLX5_RX_HASH_FN_TOEPLITZ       = 0x2,
};

enum {
	MLX5_TIRC_SELF_LB_BLOCK_BLOCK_UNICAST    = 0x1,
	MLX5_TIRC_SELF_LB_BLOCK_BLOCK_MULTICAST  = 0x2,
};

enum {
	MLX5_LRO_MAX_MSG_SIZE_START_FROM_L4    = 0x0,
	MLX5_LRO_MAX_MSG_SIZE_START_FROM_L2  = 0x1,
};

struct mlx5_ifc_tirc_bits {
	u8 reserved_at_0[0x20];
	u8 disp_type[0x4];
	u8 reserved_at_24[0x1c];
	u8 reserved_at_40[0x40];
	u8 reserved_at_80[0x4];
	u8 lro_timeout_period_usecs[0x10];
	u8 lro_enable_mask[0x4];
	u8 lro_max_msg_sz[0x8];
	u8 reserved_at_a0[0x40];
	u8 reserved_at_e0[0x8];
	u8 inline_rqn[0x18];
	u8 rx_hash_symmetric[0x1];
	u8 reserved_at_101[0x1];
	u8 tunneled_offload_en[0x1];
	u8 reserved_at_103[0x5];
	u8 indirect_table[0x18];
	u8 rx_hash_fn[0x4];
	u8 reserved_at_124[0x2];
	u8 self_lb_block[0x2];
	u8 transport_domain[0x18];
	u8 rx_hash_toeplitz_key[10][0x20];
	struct mlx5_ifc_rx_hash_field_select_bits rx_hash_field_selector_outer;
	struct mlx5_ifc_rx_hash_field_select_bits rx_hash_field_selector_inner;
	u8 reserved_at_2c0[0x4c0];
};

struct mlx5_ifc_create_tir_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 tirn[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_create_tir_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0xc0];
	struct mlx5_ifc_tirc_bits ctx;
};

struct mlx5_ifc_rq_num_bits {
	u8 reserved_at_0[0x8];
	u8 rq_num[0x18];
};

struct mlx5_ifc_rqtc_bits {
	u8 reserved_at_0[0xa0];
	u8 reserved_at_a0[0x10];
	u8 rqt_max_size[0x10];
	u8 reserved_at_c0[0x10];
	u8 rqt_actual_size[0x10];
	u8 reserved_at_e0[0x6a0];
	struct mlx5_ifc_rq_num_bits rq_num[];
};

struct mlx5_ifc_create_rqt_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 rqtn[0x18];
	u8 reserved_at_60[0x20];
};

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct mlx5_ifc_create_rqt_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0xc0];
	struct mlx5_ifc_rqtc_bits rqt_context;
};
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

enum {
	MLX5_SQC_STATE_RST  = 0x0,
	MLX5_SQC_STATE_RDY  = 0x1,
	MLX5_SQC_STATE_ERR  = 0x3,
};

struct mlx5_ifc_sqc_bits {
	u8 rlky[0x1];
	u8 cd_master[0x1];
	u8 fre[0x1];
	u8 flush_in_error_en[0x1];
	u8 allow_multi_pkt_send_wqe[0x1];
	u8 min_wqe_inline_mode[0x3];
	u8 state[0x4];
	u8 reg_umr[0x1];
	u8 allow_swp[0x1];
	u8 hairpin[0x1];
	u8 reserved_at_f[0x11];
	u8 reserved_at_20[0x8];
	u8 user_index[0x18];
	u8 reserved_at_40[0x8];
	u8 cqn[0x18];
	u8 reserved_at_60[0x8];
	u8 hairpin_peer_rq[0x18];
	u8 reserved_at_80[0x10];
	u8 hairpin_peer_vhca[0x10];
	u8 reserved_at_a0[0x50];
	u8 packet_pacing_rate_limit_index[0x10];
	u8 tis_lst_sz[0x10];
	u8 reserved_at_110[0x10];
	u8 reserved_at_120[0x40];
	u8 reserved_at_160[0x8];
	u8 tis_num_0[0x18];
	struct mlx5_ifc_wq_bits wq;
};

struct mlx5_ifc_query_sq_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 sqn[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_modify_sq_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_modify_sq_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 sq_state[0x4];
	u8 reserved_at_44[0x4];
	u8 sqn[0x18];
	u8 reserved_at_60[0x20];
	u8 modify_bitmask[0x40];
	u8 reserved_at_c0[0x40];
	struct mlx5_ifc_sqc_bits ctx;
};

struct mlx5_ifc_create_sq_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 sqn[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_create_sq_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0xc0];
	struct mlx5_ifc_sqc_bits ctx;
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
