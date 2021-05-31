/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2016 6WIND S.A.
 * Copyright 2016 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_PRM_H_
#define RTE_PMD_MLX5_PRM_H_

#include <unistd.h>

#include <rte_vect.h>
#include <rte_byteorder.h>

#include <mlx5_glue.h>
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

/* Hardware index widths. */
#define MLX5_CQ_INDEX_WIDTH 24
#define MLX5_WQ_INDEX_WIDTH 16

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
#define MLX5_MPW_MAX_PACKETS 6
#define MLX5_MPW_INLINE_MAX_PACKETS 6

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

/* Missed in mlx5dv.h, should define here. */
#ifndef HAVE_MLX5_OPCODE_ENHANCED_MPSW
#define MLX5_OPCODE_ENHANCED_MPSW 0x29u
#endif

#ifndef HAVE_MLX5_OPCODE_SEND_EN
#define MLX5_OPCODE_SEND_EN 0x17u
#endif

#ifndef HAVE_MLX5_OPCODE_WAIT
#define MLX5_OPCODE_WAIT 0x0fu
#endif

#ifndef HAVE_MLX5_OPCODE_ACCESS_ASO
#define MLX5_OPCODE_ACCESS_ASO 0x2du
#endif

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

/* Byte length mask when mark is enable in miniCQE */
#define MLX5_LEN_WITH_MARK_MASK 0xffffff00

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
#define MLX5_WQE_BUF_ALIGNMENT rte_mem_page_size()

/* The alignment needed for CQ buffer. */
#define MLX5_CQE_BUF_ALIGNMENT rte_mem_page_size()

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

/*
 * WQE CSEG opcode field size is 32 bits, divided:
 * Bits 31:24 OPC_MOD
 * Bits 23:8 wqe_index
 * Bits 7:0 OPCODE
 */
#define WQE_CSEG_OPC_MOD_OFFSET		24
#define WQE_CSEG_WQE_INDEX_OFFSET	 8

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

struct mlx5_wqe_qseg {
	uint32_t reserved0;
	uint32_t reserved1;
	uint32_t max_index;
	uint32_t qpn_cqn;
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
	uint8_t rsvd3[3];
	uint32_t flow_table_metadata;
	uint8_t rsvd4[4];
	uint32_t byte_cnt;
	uint64_t timestamp;
	uint32_t sop_drop_qpn;
	uint16_t wqe_counter;
	uint8_t rsvd5;
	uint8_t op_own;
};

struct mlx5_cqe_ts {
	uint64_t timestamp;
	uint32_t sop_drop_qpn;
	uint16_t wqe_counter;
	uint8_t rsvd5;
	uint8_t op_own;
};

struct mlx5_wqe_rseg {
	uint64_t raddr;
	uint32_t rkey;
	uint32_t reserved;
} __rte_packed;

#define MLX5_UMRC_IF_OFFSET 31u
#define MLX5_UMRC_KO_OFFSET 16u
#define MLX5_UMRC_TO_BS_OFFSET 0u

struct mlx5_wqe_umr_cseg {
	uint32_t if_cf_toe_cq_res;
	uint32_t ko_to_bs;
	uint64_t mkey_mask;
	uint32_t rsvd1[8];
} __rte_packed;

struct mlx5_wqe_mkey_cseg {
	uint32_t fr_res_af_sf;
	uint32_t qpn_mkey;
	uint32_t reserved2;
	uint32_t flags_pd;
	uint64_t start_addr;
	uint64_t len;
	uint32_t bsf_octword_size;
	uint32_t reserved3[4];
	uint32_t translations_octword_size;
	uint32_t res4_lps;
	uint32_t reserved;
} __rte_packed;

enum {
	MLX5_BSF_SIZE_16B = 0x0,
	MLX5_BSF_SIZE_32B = 0x1,
	MLX5_BSF_SIZE_64B = 0x2,
	MLX5_BSF_SIZE_128B = 0x3,
};

enum {
	MLX5_BSF_P_TYPE_SIGNATURE = 0x0,
	MLX5_BSF_P_TYPE_CRYPTO = 0x1,
};

enum {
	MLX5_ENCRYPTION_ORDER_ENCRYPTED_WIRE_SIGNATURE = 0x0,
	MLX5_ENCRYPTION_ORDER_ENCRYPTED_MEMORY_SIGNATURE = 0x1,
	MLX5_ENCRYPTION_ORDER_ENCRYPTED_RAW_WIRE = 0x2,
	MLX5_ENCRYPTION_ORDER_ENCRYPTED_RAW_MEMORY = 0x3,
};

enum {
	MLX5_ENCRYPTION_STANDARD_AES_XTS = 0x0,
};

enum {
	MLX5_BLOCK_SIZE_512B	= 0x1,
	MLX5_BLOCK_SIZE_520B	= 0x2,
	MLX5_BLOCK_SIZE_4096B	= 0x3,
	MLX5_BLOCK_SIZE_4160B	= 0x4,
	MLX5_BLOCK_SIZE_1MB	= 0x5,
	MLX5_BLOCK_SIZE_4048B	= 0x6,
};

#define MLX5_BSF_SIZE_OFFSET		30
#define MLX5_BSF_P_TYPE_OFFSET		24
#define MLX5_ENCRYPTION_ORDER_OFFSET	16
#define MLX5_BLOCK_SIZE_OFFSET		24

struct mlx5_wqe_umr_bsf_seg {
	/*
	 * bs_bpt_eo_es contains:
	 * bs	bsf_size		2 bits at MLX5_BSF_SIZE_OFFSET
	 * bpt	bsf_p_type		2 bits at MLX5_BSF_P_TYPE_OFFSET
	 * eo	encryption_order	4 bits at MLX5_ENCRYPTION_ORDER_OFFSET
	 * es	encryption_standard	4 bits at offset 0
	 */
	uint32_t bs_bpt_eo_es;
	uint32_t raw_data_size;
	/*
	 * bsp_res contains:
	 * bsp	crypto_block_size_pointer	8 bits at MLX5_BLOCK_SIZE_OFFSET
	 * res	reserved 24 bits
	 */
	uint32_t bsp_res;
	uint32_t reserved0;
	uint8_t xts_initial_tweak[16];
	/*
	 * res_dp contains:
	 * res	reserved 8 bits
	 * dp	dek_pointer		24 bits at offset 0
	 */
	uint32_t res_dp;
	uint32_t reserved1;
	uint64_t keytag;
	uint32_t reserved2[4];
} __rte_packed;

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

struct mlx5_umr_wqe {
	struct mlx5_wqe_cseg ctr;
	struct mlx5_wqe_umr_cseg ucseg;
	struct mlx5_wqe_mkey_cseg mkc;
	union {
		struct mlx5_wqe_dseg kseg[0];
		struct mlx5_wqe_umr_bsf_seg bsf[0];
	};
} __rte_packed;

struct mlx5_rdma_write_wqe {
	struct mlx5_wqe_cseg ctr;
	struct mlx5_wqe_rseg rseg;
	struct mlx5_wqe_dseg dseg[0];
} __rte_packed;

#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

/* GGA */
/* MMO metadata segment */

#define	MLX5_OPCODE_MMO	0x2fu
#define	MLX5_OPC_MOD_MMO_REGEX 0x4u
#define	MLX5_OPC_MOD_MMO_COMP 0x2u
#define	MLX5_OPC_MOD_MMO_DECOMP 0x3u
#define	MLX5_OPC_MOD_MMO_DMA 0x1u

#define WQE_GGA_COMP_WIN_SIZE_OFFSET 12u
#define WQE_GGA_COMP_BLOCK_SIZE_OFFSET 16u
#define WQE_GGA_COMP_DYNAMIC_SIZE_OFFSET 20u
#define MLX5_GGA_COMP_WIN_SIZE_UNITS 1024u
#define MLX5_GGA_COMP_WIN_SIZE_MAX (32u * MLX5_GGA_COMP_WIN_SIZE_UNITS)
#define MLX5_GGA_COMP_LOG_BLOCK_SIZE_MAX 15u
#define MLX5_GGA_COMP_LOG_DYNAMIC_SIZE_MAX 15u
#define MLX5_GGA_COMP_LOG_DYNAMIC_SIZE_MIN 0u

struct mlx5_wqe_metadata_seg {
	uint32_t mmo_control_31_0; /* mmo_control_63_32 is in ctrl_seg.imm */
	uint32_t lkey;
	uint64_t addr;
};

struct mlx5_gga_wqe {
	uint32_t opcode;
	uint32_t sq_ds;
	uint32_t flags;
	uint32_t gga_ctrl1;  /* ws 12-15, bs 16-19, dyns 20-23. */
	uint32_t gga_ctrl2;
	uint32_t opaque_lkey;
	uint64_t opaque_vaddr;
	struct mlx5_wqe_dseg gather;
	struct mlx5_wqe_dseg scatter;
} __rte_packed;

struct mlx5_gga_compress_opaque {
	uint32_t syndrom;
	uint32_t reserved0;
	uint32_t scattered_length;
	uint32_t gathered_length;
	uint64_t scatter_crc;
	uint64_t gather_crc;
	uint32_t crc32;
	uint32_t adler32;
	uint8_t reserved1[216];
} __rte_packed;

struct mlx5_ifc_regexp_mmo_control_bits {
	uint8_t reserved_at_31[0x2];
	uint8_t le[0x1];
	uint8_t reserved_at_28[0x1];
	uint8_t subset_id_0[0xc];
	uint8_t reserved_at_16[0x4];
	uint8_t subset_id_1[0xc];
	uint8_t ctrl[0x4];
	uint8_t subset_id_2[0xc];
	uint8_t reserved_at_16_1[0x4];
	uint8_t subset_id_3[0xc];
};

struct mlx5_ifc_regexp_metadata_bits {
	uint8_t rof_version[0x10];
	uint8_t latency_count[0x10];
	uint8_t instruction_count[0x10];
	uint8_t primary_thread_count[0x10];
	uint8_t match_count[0x8];
	uint8_t detected_match_count[0x8];
	uint8_t status[0x10];
	uint8_t job_id[0x20];
	uint8_t reserved[0x80];
};

struct mlx5_ifc_regexp_match_tuple_bits {
	uint8_t length[0x10];
	uint8_t start_ptr[0x10];
	uint8_t rule_id[0x20];
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

/* CQ doorbell cmd types. */
#define MLX5_CQ_DBR_CMD_SOL_ONLY (1 << 24)
#define MLX5_CQ_DBR_CMD_ALL (0 << 24)

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
	MLX5_MODI_GTP_TEID = 0x6E,
};

/* Total number of metadata reg_c's. */
#define MLX5_MREG_C_NUM (MLX5_MODI_META_REG_C_7 - MLX5_MODI_META_REG_C_0 + 1)

enum modify_reg {
	REG_NON = 0,
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

typedef uint64_t u64;
typedef uint32_t u32;
typedef uint16_t u16;
typedef uint8_t u8;

#define __mlx5_nullp(typ) ((struct mlx5_ifc_##typ##_bits *)0)
#define __mlx5_bit_sz(typ, fld) sizeof(__mlx5_nullp(typ)->fld)
#define __mlx5_bit_off(typ, fld) ((unsigned int)(uintptr_t) \
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
#define __mlx5_16_mask(typ, fld) (__mlx5_mask16(typ, fld) << \
				  __mlx5_16_bit_off(typ, fld))
#define MLX5_ST_SZ_BYTES(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 8)
#define MLX5_ST_SZ_DW(typ) (sizeof(struct mlx5_ifc_##typ##_bits) / 32)
#define MLX5_BYTE_OFF(typ, fld) (__mlx5_bit_off(typ, fld) / 8)
#define MLX5_ADDR_OF(typ, p, fld) ((char *)(p) + MLX5_BYTE_OFF(typ, fld))

/* insert a value to a struct */
#define MLX5_SET(typ, p, fld, v) \
	do { \
		u32 _v = v; \
		*((rte_be32_t *)(p) + __mlx5_dw_off(typ, fld)) = \
		rte_cpu_to_be_32((rte_be_to_cpu_32(*((u32 *)(p) + \
				  __mlx5_dw_off(typ, fld))) & \
				  (~__mlx5_dw_mask(typ, fld))) | \
				 (((_v) & __mlx5_mask(typ, fld)) << \
				   __mlx5_dw_bit_off(typ, fld))); \
	} while (0)

#define MLX5_SET64(typ, p, fld, v) \
	do { \
		MLX5_ASSERT(__mlx5_bit_sz(typ, fld) == 64); \
		*((rte_be64_t *)(p) + __mlx5_64_off(typ, fld)) = \
			rte_cpu_to_be_64(v); \
	} while (0)

#define MLX5_SET16(typ, p, fld, v) \
	do { \
		u16 _v = v; \
		*((rte_be16_t *)(p) + __mlx5_16_off(typ, fld)) = \
		rte_cpu_to_be_16((rte_be_to_cpu_16(*((rte_be16_t *)(p) + \
				  __mlx5_16_off(typ, fld))) & \
				  (~__mlx5_16_mask(typ, fld))) | \
				 (((_v) & __mlx5_mask16(typ, fld)) << \
				  __mlx5_16_bit_off(typ, fld))); \
	} while (0)

#define MLX5_GET_VOLATILE(typ, p, fld) \
	((rte_be_to_cpu_32(*((volatile __be32 *)(p) +\
	__mlx5_dw_off(typ, fld))) >> __mlx5_dw_bit_off(typ, fld)) & \
	__mlx5_mask(typ, fld))
#define MLX5_GET(typ, p, fld) \
	((rte_be_to_cpu_32(*((rte_be32_t *)(p) +\
	__mlx5_dw_off(typ, fld))) >> __mlx5_dw_bit_off(typ, fld)) & \
	__mlx5_mask(typ, fld))
#define MLX5_GET16(typ, p, fld) \
	((rte_be_to_cpu_16(*((rte_be16_t *)(p) + \
	  __mlx5_16_off(typ, fld))) >> __mlx5_16_bit_off(typ, fld)) & \
	 __mlx5_mask16(typ, fld))
#define MLX5_GET64(typ, p, fld) rte_be_to_cpu_64(*((rte_be64_t *)(p) + \
						   __mlx5_64_off(typ, fld)))
#define MLX5_FLD_SZ_BYTES(typ, fld) (__mlx5_bit_sz(typ, fld) / 8)
#define MLX5_UN_SZ_BYTES(typ) (sizeof(union mlx5_ifc_##typ##_bits) / 8)

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
	u8 reserved_at_e4[0x6];
	u8 geneve_tlv_option_0_exist[0x1];
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
	u8 reserved_at_c0[0x10];
	u8 ipv4_ihl[0x4];
	u8 l3_ok[0x1];
	u8 l4_ok[0x1];
	u8 ipv4_checksum_ok[0x1];
	u8 l4_checksum_ok[0x1];
	u8 ip_ttl_hoplimit[0x8];
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
	u8 geneve_tlv_option_0_data[0x20];
	u8 gtpu_teid[0x20];
	u8 gtpu_msg_type[0x08];
	u8 gtpu_msg_flags[0x08];
	u8 reserved_at_170[0x10];
	u8 gtpu_dw_2[0x20];
	u8 gtpu_first_ext_dw_0[0x20];
	u8 gtpu_dw_0[0x20];
	u8 reserved_at_240[0x20];

};

struct mlx5_ifc_fte_match_set_misc4_bits {
	u8 prog_sample_field_value_0[0x20];
	u8 prog_sample_field_id_0[0x20];
	u8 prog_sample_field_value_1[0x20];
	u8 prog_sample_field_id_1[0x20];
	u8 prog_sample_field_value_2[0x20];
	u8 prog_sample_field_id_2[0x20];
	u8 prog_sample_field_value_3[0x20];
	u8 prog_sample_field_id_3[0x20];
	u8 reserved_at_100[0x100];
};

struct mlx5_ifc_fte_match_set_misc5_bits {
	u8 macsec_tag_0[0x20];
	u8 macsec_tag_1[0x20];
	u8 macsec_tag_2[0x20];
	u8 macsec_tag_3[0x20];
	u8 tunnel_header_0[0x20];
	u8 tunnel_header_1[0x20];
	u8 tunnel_header_2[0x20];
	u8 tunnel_header_3[0x20];
	u8 reserved[0x100];
};

/* Flow matcher. */
struct mlx5_ifc_fte_match_param_bits {
	struct mlx5_ifc_fte_match_set_lyr_2_4_bits outer_headers;
	struct mlx5_ifc_fte_match_set_misc_bits misc_parameters;
	struct mlx5_ifc_fte_match_set_lyr_2_4_bits inner_headers;
	struct mlx5_ifc_fte_match_set_misc2_bits misc_parameters_2;
	struct mlx5_ifc_fte_match_set_misc3_bits misc_parameters_3;
	struct mlx5_ifc_fte_match_set_misc4_bits misc_parameters_4;
	struct mlx5_ifc_fte_match_set_misc5_bits misc_parameters_5;
/*
 * Add reserved bit to match the struct size with the size defined in PRM.
 * This extension is not required in Linux.
 */
#ifndef HAVE_INFINIBAND_VERBS_H
	u8 reserved_0[0x200];
#endif
};

struct mlx5_ifc_dest_format_struct_bits {
	u8 destination_type[0x8];
	u8 destination_id[0x18];
	u8 reserved_0[0x20];
};

enum {
	MLX5_MATCH_CRITERIA_ENABLE_OUTER_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_INNER_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC2_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC3_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC4_BIT,
	MLX5_MATCH_CRITERIA_ENABLE_MISC5_BIT,
};

enum {
	MLX5_CMD_OP_QUERY_HCA_CAP = 0x100,
	MLX5_CMD_OP_CREATE_MKEY = 0x200,
	MLX5_CMD_OP_CREATE_CQ = 0x400,
	MLX5_CMD_OP_CREATE_QP = 0x500,
	MLX5_CMD_OP_RST2INIT_QP = 0x502,
	MLX5_CMD_OP_INIT2RTR_QP = 0x503,
	MLX5_CMD_OP_RTR2RTS_QP = 0x504,
	MLX5_CMD_OP_RTS2RTS_QP = 0x505,
	MLX5_CMD_OP_SQERR2RTS_QP = 0x506,
	MLX5_CMD_OP_QP_2ERR = 0x507,
	MLX5_CMD_OP_QP_2RST = 0x50A,
	MLX5_CMD_OP_QUERY_QP = 0x50B,
	MLX5_CMD_OP_SQD2RTS_QP = 0x50C,
	MLX5_CMD_OP_INIT2INIT_QP = 0x50E,
	MLX5_CMD_OP_SUSPEND_QP = 0x50F,
	MLX5_CMD_OP_RESUME_QP = 0x510,
	MLX5_CMD_OP_QUERY_NIC_VPORT_CONTEXT = 0x754,
	MLX5_CMD_OP_ALLOC_Q_COUNTER = 0x771,
	MLX5_CMD_OP_QUERY_Q_COUNTER = 0x773,
	MLX5_CMD_OP_ALLOC_PD = 0x800,
	MLX5_CMD_OP_DEALLOC_PD = 0x801,
	MLX5_CMD_OP_ACCESS_REGISTER = 0x805,
	MLX5_CMD_OP_ALLOC_TRANSPORT_DOMAIN = 0x816,
	MLX5_CMD_OP_CREATE_TIR = 0x900,
	MLX5_CMD_OP_MODIFY_TIR = 0x901,
	MLX5_CMD_OP_CREATE_SQ = 0X904,
	MLX5_CMD_OP_MODIFY_SQ = 0X905,
	MLX5_CMD_OP_CREATE_RQ = 0x908,
	MLX5_CMD_OP_MODIFY_RQ = 0x909,
	MLX5_CMD_OP_QUERY_RQ = 0x90b,
	MLX5_CMD_OP_CREATE_TIS = 0x912,
	MLX5_CMD_OP_QUERY_TIS = 0x915,
	MLX5_CMD_OP_CREATE_RQT = 0x916,
	MLX5_CMD_OP_MODIFY_RQT = 0x917,
	MLX5_CMD_OP_ALLOC_FLOW_COUNTER = 0x939,
	MLX5_CMD_OP_QUERY_FLOW_COUNTER = 0x93b,
	MLX5_CMD_OP_CREATE_GENERAL_OBJECT = 0xa00,
	MLX5_CMD_OP_MODIFY_GENERAL_OBJECT = 0xa01,
	MLX5_CMD_OP_QUERY_GENERAL_OBJECT = 0xa02,
	MLX5_CMD_SET_REGEX_PARAMS = 0xb04,
	MLX5_CMD_QUERY_REGEX_PARAMS = 0xb05,
	MLX5_CMD_SET_REGEX_REGISTERS = 0xb06,
	MLX5_CMD_QUERY_REGEX_REGISTERS = 0xb07,
	MLX5_CMD_OP_ACCESS_REGISTER_USER = 0xb0c,
};

enum {
	MLX5_MKC_ACCESS_MODE_MTT   = 0x1,
	MLX5_MKC_ACCESS_MODE_KLM   = 0x2,
	MLX5_MKC_ACCESS_MODE_KLM_FBS = 0x3,
};

#define MLX5_ADAPTER_PAGE_SHIFT 12
#define MLX5_LOG_RQ_STRIDE_SHIFT 4
/**
 * The batch counter dcs id starts from 0x800000 and none batch counter
 * starts from 0. As currently, the counter is changed to be indexed by
 * pool index and the offset of the counter in the pool counters_raw array.
 * It means now the counter index is same for batch and none batch counter.
 * Add the 0x800000 batch counter offset to the batch counter index helps
 * indicate the counter index is from batch or none batch container pool.
 */
#define MLX5_CNT_BATCH_OFFSET 0x800000

/* The counter batch query requires ID align with 4. */
#define MLX5_CNT_BATCH_QUERY_ID_ALIGNMENT 4

/* Flow counters. */
struct mlx5_ifc_alloc_flow_counter_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 flow_counter_id[0x20];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_alloc_flow_counter_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 flow_counter_id[0x20];
	u8 reserved_at_40[0x18];
	u8 flow_counter_bulk[0x8];
};

struct mlx5_ifc_dealloc_flow_counter_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_dealloc_flow_counter_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 flow_counter_id[0x20];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_traffic_counter_bits {
	u8 packets[0x40];
	u8 octets[0x40];
};

struct mlx5_ifc_query_flow_counter_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	struct mlx5_ifc_traffic_counter_bits flow_statistics[];
};

struct mlx5_ifc_query_flow_counter_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x20];
	u8 mkey[0x20];
	u8 address[0x40];
	u8 clear[0x1];
	u8 dump_to_memory[0x1];
	u8 num_of_counters[0x1e];
	u8 flow_counter_id[0x20];
};

#define MLX5_MAX_KLM_BYTE_COUNT 0x80000000u
#define MLX5_MIN_KLM_FIXED_BUFFER_SIZE 0x1000u

struct mlx5_ifc_klm_bits {
	u8 byte_count[0x20];
	u8 mkey[0x20];
	u8 address[0x40];
};

struct mlx5_ifc_mkc_bits {
	u8 reserved_at_0[0x1];
	u8 free[0x1];
	u8 reserved_at_2[0x1];
	u8 access_mode_4_2[0x3];
	u8 reserved_at_6[0x7];
	u8 relaxed_ordering_write[0x1];
	u8 reserved_at_e[0x1];
	u8 small_fence_on_rdma_read_response[0x1];
	u8 umr_en[0x1];
	u8 a[0x1];
	u8 rw[0x1];
	u8 rr[0x1];
	u8 lw[0x1];
	u8 lr[0x1];
	u8 access_mode_1_0[0x2];
	u8 reserved_at_18[0x8];
	u8 qpn[0x18];
	u8 mkey_7_0[0x8];
	u8 reserved_at_40[0x20];
	u8 length64[0x1];
	u8 bsf_en[0x1];
	u8 sync_umr[0x1];
	u8 reserved_at_63[0x2];
	u8 expected_sigerr_count[0x1];
	u8 reserved_at_66[0x1];
	u8 en_rinval[0x1];
	u8 pd[0x18];
	u8 start_addr[0x40];
	u8 len[0x40];
	u8 bsf_octword_size[0x20];
	u8 reserved_at_120[0x80];
	u8 translations_octword_size[0x20];
	u8 reserved_at_1c0[0x19];
	u8 relaxed_ordering_read[0x1];
	u8 reserved_at_1da[0x1];
	u8 log_page_size[0x5];
	u8 reserved_at_1e0[0x3];
	u8 crypto_en[0x2];
	u8 reserved_at_1e5[0x1b];
};

/* Range of values for MKEY context crypto_en field. */
enum {
	MLX5_MKEY_CRYPTO_DISABLED = 0x0,
	MLX5_MKEY_CRYPTO_ENABLED = 0x1,
};

struct mlx5_ifc_create_mkey_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 mkey_index[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_create_mkey_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x20];
	u8 pg_access[0x1];
	u8 reserved_at_61[0x1f];
	struct mlx5_ifc_mkc_bits memory_key_mkey_entry;
	u8 reserved_at_280[0x80];
	u8 translations_octword_actual_size[0x20];
	u8 mkey_umem_id[0x20];
	u8 mkey_umem_offset[0x40];
	u8 reserved_at_380[0x500];
	u8 klm_pas_mtt[][0x20];
};

enum {
	MLX5_GET_HCA_CAP_OP_MOD_GENERAL_DEVICE = 0x0 << 1,
	MLX5_GET_HCA_CAP_OP_MOD_ETHERNET_OFFLOAD_CAPS = 0x1 << 1,
	MLX5_GET_HCA_CAP_OP_MOD_QOS_CAP = 0xc << 1,
	MLX5_GET_HCA_CAP_OP_MOD_ROCE = 0x4 << 1,
	MLX5_GET_HCA_CAP_OP_MOD_NIC_FLOW_TABLE = 0x7 << 1,
	MLX5_GET_HCA_CAP_OP_MOD_VDPA_EMULATION = 0x13 << 1,
	MLX5_GET_HCA_CAP_OP_MOD_GENERAL_DEVICE_2 = 0x20 << 1,
};

#define MLX5_GENERAL_OBJ_TYPES_CAP_VIRTQ_NET_Q \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_VIRTQ)
#define MLX5_GENERAL_OBJ_TYPES_CAP_VIRTIO_Q_COUNTERS \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_VIRTIO_Q_COUNTERS)
#define MLX5_GENERAL_OBJ_TYPES_CAP_PARSE_GRAPH_FLEX_NODE \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_FLEX_PARSE_GRAPH)
#define MLX5_GENERAL_OBJ_TYPES_CAP_FLOW_HIT_ASO \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_FLOW_HIT_ASO)
#define MLX5_GENERAL_OBJ_TYPES_CAP_FLOW_METER_ASO \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_FLOW_METER_ASO)
#define MLX5_GENERAL_OBJ_TYPES_CAP_GENEVE_TLV_OPT \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_GENEVE_TLV_OPT)
#define MLX5_GENERAL_OBJ_TYPES_CAP_CONN_TRACK_OFFLOAD \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_CONN_TRACK_OFFLOAD)
#define MLX5_GENERAL_OBJ_TYPES_CAP_DEK \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_DEK)
#define MLX5_GENERAL_OBJ_TYPES_CAP_IMPORT_KEK \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_IMPORT_KEK)
#define MLX5_GENERAL_OBJ_TYPES_CAP_CREDENTIAL \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_CREDENTIAL)
#define MLX5_GENERAL_OBJ_TYPES_CAP_CRYPTO_LOGIN \
			(1ULL << MLX5_GENERAL_OBJ_TYPE_CRYPTO_LOGIN)

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

/* The supported timestamp formats reported in HCA attributes. */
enum {
	MLX5_HCA_CAP_TIMESTAMP_FORMAT_FR = 0x0,
	MLX5_HCA_CAP_TIMESTAMP_FORMAT_RT = 0x1,
	MLX5_HCA_CAP_TIMESTAMP_FORMAT_FR_RT = 0x2,
};

/* The timestamp format attributes to configure queues (RQ/SQ/QP). */
enum {
	MLX5_QPC_TIMESTAMP_FORMAT_FREE_RUNNING = 0x0,
	MLX5_QPC_TIMESTAMP_FORMAT_DEFAULT      = 0x1,
	MLX5_QPC_TIMESTAMP_FORMAT_REAL_TIME    = 0x2,
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

/* The device steering logic format. */
#define MLX5_STEERING_LOGIC_FORMAT_CONNECTX_5 0x0
#define MLX5_STEERING_LOGIC_FORMAT_CONNECTX_6DX 0x1

struct mlx5_ifc_cmd_hca_cap_bits {
	u8 reserved_at_0[0x30];
	u8 vhca_id[0x10];
	u8 reserved_at_40[0x20];
	u8 reserved_at_60[0x3];
	u8 log_regexp_scatter_gather_size[0x5];
	u8 reserved_at_68[0x3];
	u8 log_dma_mmo_size[0x5];
	u8 reserved_at_70[0x3];
	u8 log_compress_mmo_size[0x5];
	u8 reserved_at_78[0x3];
	u8 log_decompress_mmo_size[0x5];
	u8 log_max_srq_sz[0x8];
	u8 log_max_qp_sz[0x8];
	u8 reserved_at_90[0x9];
	u8 wqe_index_ignore_cap[0x1];
	u8 dynamic_qp_allocation[0x1];
	u8 log_max_qp[0x5];
	u8 regexp[0x1];
	u8 reserved_at_a1[0x3];
	u8 regexp_num_of_engines[0x4];
	u8 reserved_at_a8[0x1];
	u8 reg_c_preserve[0x1];
	u8 reserved_at_aa[0x1];
	u8 log_max_srq[0x5];
	u8 reserved_at_b0[0x3];
	u8 regexp_log_crspace_size[0x5];
	u8 reserved_at_b8[0x3];
	u8 scatter_fcs_w_decap_disable[0x1];
	u8 reserved_at_bc[0x4];
	u8 reserved_at_c0[0x8];
	u8 log_max_cq_sz[0x8];
	u8 reserved_at_d0[0xb];
	u8 log_max_cq[0x5];
	u8 log_max_eq_sz[0x8];
	u8 relaxed_ordering_write[0x1];
	u8 relaxed_ordering_read[0x1];
	u8 access_register_user[0x1];
	u8 log_max_mkey[0x5];
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
	u8 non_wire_sq[0x1];
	u8 reserved_at_121[0x9];
	u8 log_max_ra_req_dc[0x6];
	u8 reserved_at_130[0x3];
	u8 log_max_static_sq_wq[0x5];
	u8 reserved_at_138[0x2];
	u8 log_max_ra_res_dc[0x6];
	u8 reserved_at_140[0xa];
	u8 log_max_ra_req_qp[0x6];
	u8 rtr2rts_qp_counters_set_id[0x1];
	u8 rts2rts_udp_sport[0x1];
	u8 rts2rts_lag_tx_port_affinity[0x1];
	u8 dma_mmo[0x1];
	u8 compress_min_block_size[0x4];
	u8 compress[0x1];
	u8 decompress[0x1];
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
	u8 sq_ts_format[0x2];
	u8 rq_ts_format[0x2];
	u8 steering_format_version[0x4];
	u8 reserved_at_448[0x18];
	u8 reserved_at_460[0x8];
	u8 aes_xts[0x1];
	u8 crypto[0x1];
	u8 reserved_at_46a[0x6];
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
	u8 max_geneve_tlv_options[0x8];
	u8 reserved_at_568[0x3];
	u8 max_geneve_tlv_option_data_len[0x5];
	u8 reserved_at_570[0x49];
	u8 mini_cqe_resp_l3_l4_tag[0x1];
	u8 mini_cqe_resp_flow_tag[0x1];
	u8 enhanced_cqe_compression[0x1];
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
	u8 flow_meter_old[0x1];
	u8 reserved_at_8[0x8];
	u8 log_max_flow_meter[0x8];
	u8 flow_meter_reg_id[0x8];
	u8 wqe_rate_pp[0x1];
	u8 reserved_at_25[0x7];
	u8 flow_meter[0x1];
	u8 reserved_at_2e[0x17];
	u8 packet_pacing_max_rate[0x20];
	u8 packet_pacing_min_rate[0x20];
	u8 reserved_at_80[0x10];
	u8 packet_pacing_rate_table_size[0x10];
	u8 esw_element_type[0x10];
	u8 esw_tsar_type[0x10];
	u8 reserved_at_c0[0x10];
	u8 max_qos_para_vport[0x10];
	u8 max_tsar_bw_share[0x20];
	u8 nic_element_type[0x10];
	u8 nic_tsar_type[0x10];
	u8 reserved_at_120[0x3];
	u8 log_meter_aso_granularity[0x5];
	u8 reserved_at_128[0x3];
	u8 log_meter_aso_max_alloc[0x5];
	u8 reserved_at_130[0x3];
	u8 log_max_num_meter_aso[0x5];
	u8 reserved_at_138[0x6b0];
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
	u8 reserved_at_23[0x8];
	u8 tunnel_stateless_gtp[0x1];
	u8 reserved_at_25[0x4];
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

enum {
	MLX5_VIRTQ_TYPE_SPLIT = 0,
	MLX5_VIRTQ_TYPE_PACKED = 1,
};

enum {
	MLX5_VIRTQ_EVENT_MODE_NO_MSIX = 0,
	MLX5_VIRTQ_EVENT_MODE_QP = 1,
	MLX5_VIRTQ_EVENT_MODE_MSIX = 2,
};

struct mlx5_ifc_virtio_emulation_cap_bits {
	u8 desc_tunnel_offload_type[0x1];
	u8 eth_frame_offload_type[0x1];
	u8 virtio_version_1_0[0x1];
	u8 tso_ipv4[0x1];
	u8 tso_ipv6[0x1];
	u8 tx_csum[0x1];
	u8 rx_csum[0x1];
	u8 reserved_at_7[0x1][0x9];
	u8 event_mode[0x8];
	u8 virtio_queue_type[0x8];
	u8 reserved_at_20[0x13];
	u8 log_doorbell_stride[0x5];
	u8 reserved_at_3b[0x3];
	u8 log_doorbell_bar_size[0x5];
	u8 doorbell_bar_offset[0x40];
	u8 reserved_at_80[0x8];
	u8 max_num_virtio_queues[0x18];
	u8 reserved_at_a0[0x60];
	u8 umem_1_buffer_param_a[0x20];
	u8 umem_1_buffer_param_b[0x20];
	u8 umem_2_buffer_param_a[0x20];
	u8 umem_2_buffer_param_b[0x20];
	u8 umem_3_buffer_param_a[0x20];
	u8 umem_3_buffer_param_b[0x20];
	u8 reserved_at_1c0[0x620];
};

struct mlx5_ifc_flow_table_prop_layout_bits {
	u8 ft_support[0x1];
	u8 flow_tag[0x1];
	u8 flow_counter[0x1];
	u8 flow_modify_en[0x1];
	u8 modify_root[0x1];
	u8 identified_miss_table[0x1];
	u8 flow_table_modify[0x1];
	u8 reformat[0x1];
	u8 decap[0x1];
	u8 reset_root_to_default[0x1];
	u8 pop_vlan[0x1];
	u8 push_vlan[0x1];
	u8 fpga_vendor_acceleration[0x1];
	u8 pop_vlan_2[0x1];
	u8 push_vlan_2[0x1];
	u8 reformat_and_vlan_action[0x1];
	u8 modify_and_vlan_action[0x1];
	u8 sw_owner[0x1];
	u8 reformat_l3_tunnel_to_l2[0x1];
	u8 reformat_l2_to_l3_tunnel[0x1];
	u8 reformat_and_modify_action[0x1];
	u8 reserved_at_15[0x9];
	u8 sw_owner_v2[0x1];
	u8 reserved_at_1f[0x1];
	u8 reserved_at_20[0x2];
	u8 log_max_ft_size[0x6];
	u8 log_max_modify_header_context[0x8];
	u8 max_modify_header_actions[0x8];
	u8 max_ft_level[0x8];
	u8 reserved_at_40[0x8];
	u8 log_max_ft_sampler_num[8];
	u8 metadata_reg_b_width[0x8];
	u8 metadata_reg_a_width[0x8];
	u8 reserved_at_60[0x18];
	u8 log_max_ft_num[0x8];
	u8 reserved_at_80[0x10];
	u8 log_max_flow_counter[0x8];
	u8 log_max_destination[0x8];
	u8 reserved_at_a0[0x18];
	u8 log_max_flow[0x8];
	u8 reserved_at_c0[0x140];
};

struct mlx5_ifc_roce_caps_bits {
	u8 reserved_0[0x1e];
	u8 qp_ts_format[0x2];
	u8 reserved_at_20[0x7e0];
};

/*
 * Table 1872 - Flow Table Fields Supported 2 Format
 */
struct mlx5_ifc_ft_fields_support_2_bits {
	u8 reserved_at_0[0xf];
	u8 tunnel_header_2_3[0x1];
	u8 tunnel_header_0_1[0x1];
	u8 macsec_syndrome[0x1];
	u8 macsec_tag[0x1];
	u8 outer_lrh_sl[0x1];
	u8 inner_ipv4_ihl[0x1];
	u8 outer_ipv4_ihl[0x1];
	u8 psp_syndrome[0x1];
	u8 inner_l3_ok[0x1];
	u8 inner_l4_ok[0x1];
	u8 outer_l3_ok[0x1];
	u8 outer_l4_ok[0x1];
	u8 psp_header[0x1];
	u8 inner_ipv4_checksum_ok[0x1];
	u8 inner_l4_checksum_ok[0x1];
	u8 outer_ipv4_checksum_ok[0x1];
	u8 outer_l4_checksum_ok[0x1];
	u8 reserved_at_20[0x60];
};

struct mlx5_ifc_flow_table_nic_cap_bits {
	u8 reserved_at_0[0x200];
	struct mlx5_ifc_flow_table_prop_layout_bits
		flow_table_properties_nic_receive;
	struct mlx5_ifc_flow_table_prop_layout_bits
		flow_table_properties_nic_receive_rdma;
	struct mlx5_ifc_flow_table_prop_layout_bits
		flow_table_properties_nic_receive_sniffer;
	struct mlx5_ifc_flow_table_prop_layout_bits
		flow_table_properties_nic_transmit;
	struct mlx5_ifc_flow_table_prop_layout_bits
		flow_table_properties_nic_transmit_rdma;
	struct mlx5_ifc_flow_table_prop_layout_bits
		flow_table_properties_nic_transmit_sniffer;
	u8 reserved_at_e00[0x600];
	struct mlx5_ifc_ft_fields_support_2_bits
		ft_field_support_2_nic_receive;
};

struct mlx5_ifc_cmd_hca_cap_2_bits {
	u8 reserved_at_0[0x80]; /* End of DW4. */
	u8 reserved_at_80[0xb];
	u8 log_max_num_reserved_qpn[0x5];
	u8 reserved_at_90[0x3];
	u8 log_reserved_qpn_granularity[0x5];
	u8 reserved_at_98[0x3];
	u8 log_reserved_qpn_max_alloc[0x5]; /* End of DW5. */
	u8 max_reformat_insert_size[0x8];
	u8 max_reformat_insert_offset[0x8];
	u8 max_reformat_remove_size[0x8];
	u8 max_reformat_remove_offset[0x8]; /* End of DW6. */
	u8 aso_conntrack_reg_id[0x8];
	u8 reserved_at_c8[0x3];
	u8 log_conn_track_granularity[0x5];
	u8 reserved_at_d0[0x3];
	u8 log_conn_track_max_alloc[0x5];
	u8 reserved_at_d8[0x3];
	u8 log_max_conn_track_offload[0x5];
	u8 reserved_at_e0[0x20]; /* End of DW7. */
	u8 reserved_at_100[0x700];
};

union mlx5_ifc_hca_cap_union_bits {
	struct mlx5_ifc_cmd_hca_cap_bits cmd_hca_cap;
	struct mlx5_ifc_per_protocol_networking_offload_caps_bits
	       per_protocol_networking_offload_caps;
	struct mlx5_ifc_qos_cap_bits qos_cap;
	struct mlx5_ifc_virtio_emulation_cap_bits vdpa_caps;
	struct mlx5_ifc_flow_table_nic_cap_bits flow_table_nic_cap;
	struct mlx5_ifc_roce_caps_bits roce_caps;
	u8 reserved_at_0[0x8000];
};

struct mlx5_ifc_set_action_in_bits {
	u8 action_type[0x4];
	u8 field[0xc];
	u8 reserved_at_10[0x3];
	u8 offset[0x5];
	u8 reserved_at_18[0x3];
	u8 length[0x5];
	u8 data[0x20];
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
	u8 reserved_at_f[0xB];
	u8 ts_format[0x02];
	u8 reserved_at_1c[0x4];
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

struct mlx5_ifc_query_rq_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0xc0];
	struct mlx5_ifc_rqc_bits rq_context;
};

struct mlx5_ifc_query_rq_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 rqn[0x18];
	u8 reserved_at_60[0x20];
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
	MLX5_L3_PROT_TYPE_IPV4 = 0,
	MLX5_L3_PROT_TYPE_IPV6 = 1,
};

enum {
	MLX5_L4_PROT_TYPE_TCP = 0,
	MLX5_L4_PROT_TYPE_UDP = 1,
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

enum {
	MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_LRO = 1ULL << 0,
	MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_INDIRECT_TABLE = 1ULL << 1,
	MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_HASH = 1ULL << 2,
	/* bit 3 - tunneled_offload_en modify not supported. */
	MLX5_MODIFY_TIR_IN_MODIFY_BITMASK_SELF_LB_EN = 1ULL << 4,
};

struct mlx5_ifc_modify_tir_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_modify_tir_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 tirn[0x18];
	u8 reserved_at_60[0x20];
	u8 modify_bitmask[0x40];
	u8 reserved_at_c0[0x40];
	struct mlx5_ifc_tirc_bits ctx;
};

enum {
	MLX5_INLINE_Q_TYPE_RQ = 0x0,
	MLX5_INLINE_Q_TYPE_VIRTQ = 0x1,
};

struct mlx5_ifc_rq_num_bits {
	u8 reserved_at_0[0x8];
	u8 rq_num[0x18];
};

struct mlx5_ifc_rqtc_bits {
	u8 reserved_at_0[0xa5];
	u8 list_q_type[0x3];
	u8 reserved_at_a8[0x8];
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

struct mlx5_ifc_modify_rqt_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 rqtn[0x18];
	u8 reserved_at_60[0x20];
	u8 modify_bitmask[0x40];
	u8 reserved_at_c0[0x40];
	struct mlx5_ifc_rqtc_bits rqt_context;
};
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

struct mlx5_ifc_modify_rqt_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

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
	u8 non_wire[0x1];
	u8 static_sq_wq[0x1];
	u8 reserved_at_11[0x9];
	u8 ts_format[0x02];
	u8 reserved_at_1c[0x4];
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

enum {
	MLX5_FLOW_METER_OBJ_MODIFY_FIELD_ACTIVE = (1ULL << 0),
	MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CBS = (1ULL << 1),
	MLX5_FLOW_METER_OBJ_MODIFY_FIELD_CIR = (1ULL << 2),
	MLX5_FLOW_METER_OBJ_MODIFY_FIELD_EBS = (1ULL << 3),
	MLX5_FLOW_METER_OBJ_MODIFY_FIELD_EIR = (1ULL << 4),
};

struct mlx5_ifc_flow_meter_parameters_bits {
	u8 valid[0x1];
	u8 bucket_overflow[0x1];
	u8 start_color[0x2];
	u8 both_buckets_on_green[0x1];
	u8 meter_mode[0x2];
	u8 reserved_at_1[0x19];
	u8 reserved_at_2[0x20];
	u8 reserved_at_3[0x3];
	u8 cbs_exponent[0x5];
	u8 cbs_mantissa[0x8];
	u8 reserved_at_4[0x3];
	u8 cir_exponent[0x5];
	u8 cir_mantissa[0x8];
	u8 reserved_at_5[0x20];
	u8 reserved_at_6[0x3];
	u8 ebs_exponent[0x5];
	u8 ebs_mantissa[0x8];
	u8 reserved_at_7[0x3];
	u8 eir_exponent[0x5];
	u8 eir_mantissa[0x8];
	u8 reserved_at_8[0x60];
};
#define MLX5_IFC_FLOW_METER_PARAM_MASK UINT64_C(0x80FFFFFF)
#define MLX5_IFC_FLOW_METER_DISABLE_CBS_CIR_VAL 0x14BF00C8

enum {
	MLX5_METER_MODE_IP_LEN = 0x0,
	MLX5_METER_MODE_L2_LEN = 0x1,
	MLX5_METER_MODE_L2_IPG_LEN = 0x2,
	MLX5_METER_MODE_PKT = 0x3,
};

enum {
	MLX5_CQE_SIZE_64B = 0x0,
	MLX5_CQE_SIZE_128B = 0x1,
};

struct mlx5_ifc_cqc_bits {
	u8 status[0x4];
	u8 as_notify[0x1];
	u8 initiator_src_dct[0x1];
	u8 dbr_umem_valid[0x1];
	u8 reserved_at_7[0x1];
	u8 cqe_sz[0x3];
	u8 cc[0x1];
	u8 reserved_at_c[0x1];
	u8 scqe_break_moderation_en[0x1];
	u8 oi[0x1];
	u8 cq_period_mode[0x2];
	u8 cqe_comp_en[0x1];
	u8 mini_cqe_res_format[0x2];
	u8 st[0x4];
	u8 reserved_at_18[0x1];
	u8 cqe_comp_layout[0x7];
	u8 dbr_umem_id[0x20];
	u8 reserved_at_40[0x14];
	u8 page_offset[0x6];
	u8 reserved_at_5a[0x2];
	u8 mini_cqe_res_format_ext[0x2];
	u8 cq_timestamp_format[0x2];
	u8 reserved_at_60[0x3];
	u8 log_cq_size[0x5];
	u8 uar_page[0x18];
	u8 reserved_at_80[0x4];
	u8 cq_period[0xc];
	u8 cq_max_count[0x10];
	u8 reserved_at_a0[0x18];
	u8 c_eqn[0x8];
	u8 reserved_at_c0[0x3];
	u8 log_page_size[0x5];
	u8 reserved_at_c8[0x18];
	u8 reserved_at_e0[0x20];
	u8 reserved_at_100[0x8];
	u8 last_notified_index[0x18];
	u8 reserved_at_120[0x8];
	u8 last_solicit_index[0x18];
	u8 reserved_at_140[0x8];
	u8 consumer_counter[0x18];
	u8 reserved_at_160[0x8];
	u8 producer_counter[0x18];
	u8 local_partition_id[0xc];
	u8 process_id[0x14];
	u8 reserved_at_1A0[0x20];
	u8 dbr_addr[0x40];
};

struct mlx5_ifc_health_buffer_bits {
	u8 reserved_0[0x100];
	u8 assert_existptr[0x20];
	u8 assert_callra[0x20];
	u8 reserved_1[0x40];
	u8 fw_version[0x20];
	u8 hw_id[0x20];
	u8 reserved_2[0x20];
	u8 irisc_index[0x8];
	u8 synd[0x8];
	u8 ext_synd[0x10];
};

struct mlx5_ifc_initial_seg_bits {
	u8 fw_rev_minor[0x10];
	u8 fw_rev_major[0x10];
	u8 cmd_interface_rev[0x10];
	u8 fw_rev_subminor[0x10];
	u8 reserved_0[0x40];
	u8 cmdq_phy_addr_63_32[0x20];
	u8 cmdq_phy_addr_31_12[0x14];
	u8 reserved_1[0x2];
	u8 nic_interface[0x2];
	u8 log_cmdq_size[0x4];
	u8 log_cmdq_stride[0x4];
	u8 command_doorbell_vector[0x20];
	u8 reserved_2[0xf00];
	u8 initializing[0x1];
	u8 nic_interface_supported[0x7];
	u8 reserved_4[0x18];
	struct mlx5_ifc_health_buffer_bits health_buffer;
	u8 no_dram_nic_offset[0x20];
	u8 reserved_5[0x6de0];
	u8 internal_timer_h[0x20];
	u8 internal_timer_l[0x20];
	u8 reserved_6[0x20];
	u8 reserved_7[0x1f];
	u8 clear_int[0x1];
	u8 health_syndrome[0x8];
	u8 health_counter[0x18];
	u8 reserved_8[0x17fc0];
};

struct mlx5_ifc_create_cq_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 cqn[0x18];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_create_cq_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x40];
	struct mlx5_ifc_cqc_bits cq_context;
	u8 cq_umem_offset[0x40];
	u8 cq_umem_id[0x20];
	u8 cq_umem_valid[0x1];
	u8 reserved_at_2e1[0x1f];
	u8 reserved_at_300[0x580];
	u8 pas[];
};

enum {
	MLX5_GENERAL_OBJ_TYPE_GENEVE_TLV_OPT = 0x000b,
	MLX5_GENERAL_OBJ_TYPE_DEK = 0x000c,
	MLX5_GENERAL_OBJ_TYPE_VIRTQ = 0x000d,
	MLX5_GENERAL_OBJ_TYPE_VIRTIO_Q_COUNTERS = 0x001c,
	MLX5_GENERAL_OBJ_TYPE_IMPORT_KEK = 0x001d,
	MLX5_GENERAL_OBJ_TYPE_CREDENTIAL = 0x001e,
	MLX5_GENERAL_OBJ_TYPE_CRYPTO_LOGIN = 0x001f,
	MLX5_GENERAL_OBJ_TYPE_FLEX_PARSE_GRAPH = 0x0022,
	MLX5_GENERAL_OBJ_TYPE_FLOW_METER_ASO = 0x0024,
	MLX5_GENERAL_OBJ_TYPE_FLOW_HIT_ASO = 0x0025,
	MLX5_GENERAL_OBJ_TYPE_CONN_TRACK_OFFLOAD = 0x0031,
};

struct mlx5_ifc_general_obj_in_cmd_hdr_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x20];
	u8 obj_type[0x10];
	u8 obj_id[0x20];
	u8 reserved_at_60[0x3];
	u8 log_obj_range[0x5];
	u8 reserved_at_58[0x18];
};

struct mlx5_ifc_general_obj_out_cmd_hdr_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 obj_id[0x20];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_virtio_q_counters_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_40[0x40];
	u8 received_desc[0x40];
	u8 completed_desc[0x40];
	u8 error_cqes[0x20];
	u8 bad_desc_errors[0x20];
	u8 exceed_max_chain[0x20];
	u8 invalid_buffer[0x20];
	u8 reserved_at_180[0x50];
};

struct mlx5_ifc_geneve_tlv_option_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_40[0x18];
	u8 geneve_option_fte_index[0x8];
	u8 option_class[0x10];
	u8 option_type[0x8];
	u8 reserved_at_78[0x3];
	u8 option_data_length[0x5];
	u8 reserved_at_80[0x180];
};

struct mlx5_ifc_create_virtio_q_counters_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_virtio_q_counters_bits virtio_q_counters;
};

struct mlx5_ifc_query_virtio_q_counters_out_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_virtio_q_counters_bits virtio_q_counters;
};

struct mlx5_ifc_create_geneve_tlv_option_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_geneve_tlv_option_bits geneve_tlv_opt;
};

enum {
	MLX5_CRYPTO_KEY_SIZE_128b = 0x0,
	MLX5_CRYPTO_KEY_SIZE_256b = 0x1,
};

enum {
	MLX5_CRYPTO_KEY_PURPOSE_TLS	= 0x1,
	MLX5_CRYPTO_KEY_PURPOSE_IPSEC	= 0x2,
	MLX5_CRYPTO_KEY_PURPOSE_AES_XTS	= 0x3,
	MLX5_CRYPTO_KEY_PURPOSE_MACSEC	= 0x4,
	MLX5_CRYPTO_KEY_PURPOSE_GCM	= 0x5,
	MLX5_CRYPTO_KEY_PURPOSE_PSP	= 0x6,
};

struct mlx5_ifc_dek_bits {
	u8 modify_field_select[0x40];
	u8 state[0x8];
	u8 reserved_at_48[0xc];
	u8 key_size[0x4];
	u8 has_keytag[0x1];
	u8 reserved_at_59[0x3];
	u8 key_purpose[0x4];
	u8 reserved_at_60[0x8];
	u8 pd[0x18];
	u8 reserved_at_80[0x100];
	u8 opaque[0x40];
	u8 reserved_at_1c0[0x40];
	u8 key[0x400];
	u8 reserved_at_600[0x200];
};

struct mlx5_ifc_create_dek_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_dek_bits dek;
};

struct mlx5_ifc_import_kek_bits {
	u8 modify_field_select[0x40];
	u8 state[0x8];
	u8 reserved_at_48[0xc];
	u8 key_size[0x4];
	u8 reserved_at_58[0x1a8];
	u8 key[0x400];
	u8 reserved_at_600[0x200];
};

struct mlx5_ifc_create_import_kek_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_import_kek_bits import_kek;
};

enum {
	MLX5_CREDENTIAL_ROLE_OFFICER = 0x0,
	MLX5_CREDENTIAL_ROLE_USER = 0x1,
};

struct mlx5_ifc_credential_bits {
	u8 modify_field_select[0x40];
	u8 state[0x8];
	u8 reserved_at_48[0x10];
	u8 credential_role[0x8];
	u8 reserved_at_60[0x1a0];
	u8 credential[0x180];
	u8 reserved_at_380[0x480];
};

struct mlx5_ifc_create_credential_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_credential_bits credential;
};

struct mlx5_ifc_crypto_login_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_40[0x48];
	u8 credential_pointer[0x18];
	u8 reserved_at_a0[0x8];
	u8 session_import_kek_ptr[0x18];
	u8 reserved_at_c0[0x140];
	u8 credential[0x180];
	u8 reserved_at_380[0x480];
};

struct mlx5_ifc_create_crypto_login_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_crypto_login_bits crypto_login;
};

enum {
	MLX5_VIRTQ_STATE_INIT = 0,
	MLX5_VIRTQ_STATE_RDY = 1,
	MLX5_VIRTQ_STATE_SUSPEND = 2,
	MLX5_VIRTQ_STATE_ERROR = 3,
};

enum {
	MLX5_VIRTQ_MODIFY_TYPE_STATE = (1UL << 0),
	MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_PARAMS = (1UL << 3),
	MLX5_VIRTQ_MODIFY_TYPE_DIRTY_BITMAP_DUMP_ENABLE = (1UL << 4),
};

struct mlx5_ifc_virtio_q_bits {
	u8 virtio_q_type[0x8];
	u8 reserved_at_8[0x5];
	u8 event_mode[0x3];
	u8 queue_index[0x10];
	u8 full_emulation[0x1];
	u8 virtio_version_1_0[0x1];
	u8 reserved_at_22[0x2];
	u8 offload_type[0x4];
	u8 event_qpn_or_msix[0x18];
	u8 doorbell_stride_idx[0x10];
	u8 queue_size[0x10];
	u8 device_emulation_id[0x20];
	u8 desc_addr[0x40];
	u8 used_addr[0x40];
	u8 available_addr[0x40];
	u8 virtio_q_mkey[0x20];
	u8 reserved_at_160[0x18];
	u8 error_type[0x8];
	u8 umem_1_id[0x20];
	u8 umem_1_size[0x20];
	u8 umem_1_offset[0x40];
	u8 umem_2_id[0x20];
	u8 umem_2_size[0x20];
	u8 umem_2_offset[0x40];
	u8 umem_3_id[0x20];
	u8 umem_3_size[0x20];
	u8 umem_3_offset[0x40];
	u8 counter_set_id[0x20];
	u8 reserved_at_320[0x8];
	u8 pd[0x18];
	u8 reserved_at_340[0x2];
	u8 queue_period_mode[0x2];
	u8 queue_period_us[0xc];
	u8 queue_max_count[0x10];
	u8 reserved_at_360[0xa0];
};

struct mlx5_ifc_virtio_net_q_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_40[0x40];
	u8 tso_ipv4[0x1];
	u8 tso_ipv6[0x1];
	u8 tx_csum[0x1];
	u8 rx_csum[0x1];
	u8 reserved_at_84[0x6];
	u8 dirty_bitmap_dump_enable[0x1];
	u8 vhost_log_page[0x5];
	u8 reserved_at_90[0xc];
	u8 state[0x4];
	u8 reserved_at_a0[0x8];
	u8 tisn_or_qpn[0x18];
	u8 dirty_bitmap_mkey[0x20];
	u8 dirty_bitmap_size[0x20];
	u8 dirty_bitmap_addr[0x40];
	u8 hw_available_index[0x10];
	u8 hw_used_index[0x10];
	u8 reserved_at_160[0xa0];
	struct mlx5_ifc_virtio_q_bits virtio_q_context;
};

struct mlx5_ifc_create_virtq_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_virtio_net_q_bits virtq;
};

struct mlx5_ifc_query_virtq_out_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_virtio_net_q_bits virtq;
};

struct mlx5_ifc_flow_hit_aso_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_40[0x48];
	u8 access_pd[0x18];
	u8 reserved_at_a0[0x160];
	u8 flag[0x200];
};

struct mlx5_ifc_create_flow_hit_aso_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_flow_hit_aso_bits flow_hit_aso;
};

struct mlx5_ifc_flow_meter_aso_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_40[0x48];
	u8 access_pd[0x18];
	u8 reserved_at_a0[0x160];
	u8 parameters[0x200];
};

struct mlx5_ifc_create_flow_meter_aso_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_flow_meter_aso_bits flow_meter_aso;
};

struct mlx5_ifc_tcp_window_params_bits {
	u8 max_ack[0x20];
	u8 max_win[0x20];
	u8 reply_end[0x20];
	u8 sent_end[0x20];
};

struct mlx5_ifc_conn_track_aso_bits {
	struct mlx5_ifc_tcp_window_params_bits reply_dir; /* End of DW3. */
	struct mlx5_ifc_tcp_window_params_bits original_dir; /* End of DW7. */
	u8 last_end[0x20]; /* End of DW8. */
	u8 last_ack[0x20]; /* End of DW9. */
	u8 last_seq[0x20]; /* End of DW10. */
	u8 last_win[0x10];
	u8 reserved_at_170[0xa];
	u8 last_dir[0x1];
	u8 last_index[0x5]; /* End of DW11. */
	u8 reserved_at_180[0x40]; /* End of DW13. */
	u8 reply_direction_tcp_scale[0x4];
	u8 reply_direction_tcp_close_initiated[0x1];
	u8 reply_direction_tcp_liberal_enabled[0x1];
	u8 reply_direction_tcp_data_unacked[0x1];
	u8 reply_direction_tcp_max_ack[0x1];
	u8 reserved_at_1c8[0x8];
	u8 original_direction_tcp_scale[0x4];
	u8 original_direction_tcp_close_initiated[0x1];
	u8 original_direction_tcp_liberal_enabled[0x1];
	u8 original_direction_tcp_data_unacked[0x1];
	u8 original_direction_tcp_max_ack[0x1];
	u8 reserved_at_1d8[0x8]; /* End of DW14. */
	u8 valid[0x1];
	u8 state[0x3];
	u8 freeze_track[0x1];
	u8 reserved_at_1e5[0xb];
	u8 reserved_at_1f0[0x1];
	u8 connection_assured[0x1];
	u8 sack_permitted[0x1];
	u8 challenged_acked[0x1];
	u8 heartbeat[0x1];
	u8 max_ack_window[0x3];
	u8 reserved_at_1f8[0x1];
	u8 retransmission_counter[0x3];
	u8 retranmission_limit_exceeded[0x1];
	u8 retranmission_limit[0x3]; /* End of DW15. */
};

struct mlx5_ifc_conn_track_offload_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_40[0x40];
	u8 reserved_at_80[0x8];
	u8 conn_track_aso_access_pd[0x18];
	u8 reserved_at_a0[0x160];
	struct mlx5_ifc_conn_track_aso_bits conn_track_aso;
};

struct mlx5_ifc_create_conn_track_aso_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_conn_track_offload_bits conn_track_offload;
};

enum mlx5_access_aso_opc_mod {
	ASO_OPC_MOD_IPSEC = 0x0,
	ASO_OPC_MOD_CONNECTION_TRACKING = 0x1,
	ASO_OPC_MOD_POLICER = 0x2,
	ASO_OPC_MOD_RACE_AVOIDANCE = 0x3,
	ASO_OPC_MOD_FLOW_HIT = 0x4,
};

#define ASO_CSEG_DATA_MASK_MODE_OFFSET	30

enum mlx5_aso_data_mask_mode {
	BITWISE_64BIT = 0x0,
	BYTEWISE_64BYTE = 0x1,
	CALCULATED_64BYTE = 0x2,
};

#define ASO_CSEG_COND_0_OPER_OFFSET	20
#define ASO_CSEG_COND_1_OPER_OFFSET	16

enum mlx5_aso_pre_cond_op {
	ASO_OP_ALWAYS_FALSE = 0x0,
	ASO_OP_ALWAYS_TRUE = 0x1,
	ASO_OP_EQUAL = 0x2,
	ASO_OP_NOT_EQUAL = 0x3,
	ASO_OP_GREATER_OR_EQUAL = 0x4,
	ASO_OP_LESSER_OR_EQUAL = 0x5,
	ASO_OP_LESSER = 0x6,
	ASO_OP_GREATER = 0x7,
	ASO_OP_CYCLIC_GREATER = 0x8,
	ASO_OP_CYCLIC_LESSER = 0x9,
};

#define ASO_CSEG_COND_OPER_OFFSET	6

enum mlx5_aso_op {
	ASO_OPER_LOGICAL_AND = 0x0,
	ASO_OPER_LOGICAL_OR = 0x1,
};

/* ASO WQE CTRL segment. */
struct mlx5_aso_cseg {
	uint32_t va_h;
	uint32_t va_l_r;
	uint32_t lkey;
	uint32_t operand_masks;
	uint32_t condition_0_data;
	uint32_t condition_0_mask;
	uint32_t condition_1_data;
	uint32_t condition_1_mask;
	uint64_t bitwise_data;
	uint64_t data_mask;
} __rte_packed;

/* A meter data segment - 2 per ASO WQE. */
struct mlx5_aso_mtr_dseg {
	uint32_t v_bo_sc_bbog_mm;
	/*
	 * bit 31: valid, 30: bucket overflow, 28-29: start color,
	 * 27: both buckets on green, 24-25: meter mode.
	 */
	uint32_t reserved;
	uint32_t cbs_cir;
	/*
	 * bit 24-28: cbs_exponent, bit 16-23 cbs_mantissa,
	 * bit 8-12: cir_exponent, bit 0-7 cir_mantissa.
	 */
	uint32_t c_tokens;
	uint32_t ebs_eir;
	/*
	 * bit 24-28: ebs_exponent, bit 16-23 ebs_mantissa,
	 * bit 8-12: eir_exponent, bit 0-7 eir_mantissa.
	 */
	uint32_t e_tokens;
	uint64_t timestamp;
} __rte_packed;

#define ASO_DSEG_VALID_OFFSET 31
#define ASO_DSEG_BO_OFFSET 30
#define ASO_DSEG_SC_OFFSET 28
#define ASO_DSEG_BBOG_OFFSET 27
#define ASO_DSEG_MTR_MODE 24
#define ASO_DSEG_CBS_EXP_OFFSET 24
#define ASO_DSEG_CBS_MAN_OFFSET 16
#define ASO_DSEG_XIR_EXP_MASK 0x1F
#define ASO_DSEG_XIR_EXP_OFFSET 8
#define ASO_DSEG_EBS_EXP_OFFSET 24
#define ASO_DSEG_EBS_MAN_OFFSET 16
#define ASO_DSEG_EXP_MASK 0x1F
#define ASO_DSEG_MAN_MASK 0xFF

#define MLX5_ASO_WQE_DSEG_SIZE	0x40
#define MLX5_ASO_METERS_PER_WQE 2
#define MLX5_ASO_MTRS_PER_POOL 128

/* ASO WQE data segment. */
struct mlx5_aso_dseg {
	union {
		uint8_t data[MLX5_ASO_WQE_DSEG_SIZE];
		struct mlx5_aso_mtr_dseg mtrs[MLX5_ASO_METERS_PER_WQE];
	};
} __rte_packed;

/* ASO WQE. */
struct mlx5_aso_wqe {
	struct mlx5_wqe_cseg general_cseg;
	struct mlx5_aso_cseg aso_cseg;
	struct mlx5_aso_dseg aso_dseg;
} __rte_packed;

enum {
	MLX5_EVENT_TYPE_OBJECT_CHANGE = 0x27,
};

enum {
	MLX5_QP_ST_RC = 0x0,
};

enum {
	MLX5_QP_PM_MIGRATED = 0x3,
};

enum {
	MLX5_NON_ZERO_RQ = 0x0,
	MLX5_SRQ_RQ = 0x1,
	MLX5_CRQ_RQ = 0x2,
	MLX5_ZERO_LEN_RQ = 0x3,
};

struct mlx5_ifc_ads_bits {
	u8 fl[0x1];
	u8 free_ar[0x1];
	u8 reserved_at_2[0xe];
	u8 pkey_index[0x10];
	u8 reserved_at_20[0x8];
	u8 grh[0x1];
	u8 mlid[0x7];
	u8 rlid[0x10];
	u8 ack_timeout[0x5];
	u8 reserved_at_45[0x3];
	u8 src_addr_index[0x8];
	u8 reserved_at_50[0x4];
	u8 stat_rate[0x4];
	u8 hop_limit[0x8];
	u8 reserved_at_60[0x4];
	u8 tclass[0x8];
	u8 flow_label[0x14];
	u8 rgid_rip[16][0x8];
	u8 reserved_at_100[0x4];
	u8 f_dscp[0x1];
	u8 f_ecn[0x1];
	u8 reserved_at_106[0x1];
	u8 f_eth_prio[0x1];
	u8 ecn[0x2];
	u8 dscp[0x6];
	u8 udp_sport[0x10];
	u8 dei_cfi[0x1];
	u8 eth_prio[0x3];
	u8 sl[0x4];
	u8 vhca_port_num[0x8];
	u8 rmac_47_32[0x10];
	u8 rmac_31_0[0x20];
};

struct mlx5_ifc_qpc_bits {
	u8 state[0x4];
	u8 lag_tx_port_affinity[0x4];
	u8 st[0x8];
	u8 reserved_at_10[0x3];
	u8 pm_state[0x2];
	u8 reserved_at_15[0x1];
	u8 req_e2e_credit_mode[0x2];
	u8 offload_type[0x4];
	u8 end_padding_mode[0x2];
	u8 reserved_at_1e[0x2];
	u8 wq_signature[0x1];
	u8 block_lb_mc[0x1];
	u8 atomic_like_write_en[0x1];
	u8 latency_sensitive[0x1];
	u8 reserved_at_24[0x1];
	u8 drain_sigerr[0x1];
	u8 reserved_at_26[0x2];
	u8 pd[0x18];
	u8 mtu[0x3];
	u8 log_msg_max[0x5];
	u8 reserved_at_48[0x1];
	u8 log_rq_size[0x4];
	u8 log_rq_stride[0x3];
	u8 no_sq[0x1];
	u8 log_sq_size[0x4];
	u8 reserved_at_55[0x3];
	u8 ts_format[0x2];
	u8 reserved_at_5a[0x1];
	u8 rlky[0x1];
	u8 ulp_stateless_offload_mode[0x4];
	u8 counter_set_id[0x8];
	u8 uar_page[0x18];
	u8 reserved_at_80[0x8];
	u8 user_index[0x18];
	u8 reserved_at_a0[0x3];
	u8 log_page_size[0x5];
	u8 remote_qpn[0x18];
	struct mlx5_ifc_ads_bits primary_address_path;
	struct mlx5_ifc_ads_bits secondary_address_path;
	u8 log_ack_req_freq[0x4];
	u8 reserved_at_384[0x4];
	u8 log_sra_max[0x3];
	u8 reserved_at_38b[0x2];
	u8 retry_count[0x3];
	u8 rnr_retry[0x3];
	u8 reserved_at_393[0x1];
	u8 fre[0x1];
	u8 cur_rnr_retry[0x3];
	u8 cur_retry_count[0x3];
	u8 reserved_at_39b[0x5];
	u8 reserved_at_3a0[0x20];
	u8 reserved_at_3c0[0x8];
	u8 next_send_psn[0x18];
	u8 reserved_at_3e0[0x8];
	u8 cqn_snd[0x18];
	u8 reserved_at_400[0x8];
	u8 deth_sqpn[0x18];
	u8 reserved_at_420[0x20];
	u8 reserved_at_440[0x8];
	u8 last_acked_psn[0x18];
	u8 reserved_at_460[0x8];
	u8 ssn[0x18];
	u8 reserved_at_480[0x8];
	u8 log_rra_max[0x3];
	u8 reserved_at_48b[0x1];
	u8 atomic_mode[0x4];
	u8 rre[0x1];
	u8 rwe[0x1];
	u8 rae[0x1];
	u8 reserved_at_493[0x1];
	u8 page_offset[0x6];
	u8 reserved_at_49a[0x3];
	u8 cd_slave_receive[0x1];
	u8 cd_slave_send[0x1];
	u8 cd_master[0x1];
	u8 reserved_at_4a0[0x3];
	u8 min_rnr_nak[0x5];
	u8 next_rcv_psn[0x18];
	u8 reserved_at_4c0[0x8];
	u8 xrcd[0x18];
	u8 reserved_at_4e0[0x8];
	u8 cqn_rcv[0x18];
	u8 dbr_addr[0x40];
	u8 q_key[0x20];
	u8 reserved_at_560[0x5];
	u8 rq_type[0x3];
	u8 srqn_rmpn_xrqn[0x18];
	u8 reserved_at_580[0x8];
	u8 rmsn[0x18];
	u8 hw_sq_wqebb_counter[0x10];
	u8 sw_sq_wqebb_counter[0x10];
	u8 hw_rq_counter[0x20];
	u8 sw_rq_counter[0x20];
	u8 reserved_at_600[0x20];
	u8 reserved_at_620[0xf];
	u8 cgs[0x1];
	u8 cs_req[0x8];
	u8 cs_res[0x8];
	u8 dc_access_key[0x40];
	u8 reserved_at_680[0x3];
	u8 dbr_umem_valid[0x1];
	u8 reserved_at_684[0x9c];
	u8 dbr_umem_id[0x20];
};

struct mlx5_ifc_create_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
};

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct mlx5_ifc_create_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x40];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 wq_umem_offset[0x40];
	u8 wq_umem_id[0x20];
	u8 wq_umem_valid[0x1];
	u8 reserved_at_861[0x1f];
	u8 pas[0][0x40];
};
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

struct mlx5_ifc_sqerr2rts_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_sqerr2rts_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
};

struct mlx5_ifc_sqd2rts_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_sqd2rts_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
};

struct mlx5_ifc_rts2rts_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_rts2rts_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
};

struct mlx5_ifc_rtr2rts_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_rtr2rts_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
};

struct mlx5_ifc_rst2init_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_rst2init_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
};

struct mlx5_ifc_init2rtr_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_init2rtr_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
};

struct mlx5_ifc_init2init_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_init2init_qp_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
};

struct mlx5_ifc_dealloc_pd_out_bits {
	u8 status[0x8];
	u8 reserved_0[0x18];
	u8 syndrome[0x20];
	u8 reserved_1[0x40];
};

struct mlx5_ifc_dealloc_pd_in_bits {
	u8 opcode[0x10];
	u8 reserved_0[0x10];
	u8 reserved_1[0x10];
	u8 op_mod[0x10];
	u8 reserved_2[0x8];
	u8 pd[0x18];
	u8 reserved_3[0x20];
};

struct mlx5_ifc_alloc_pd_out_bits {
	u8 status[0x8];
	u8 reserved_0[0x18];
	u8 syndrome[0x20];
	u8 reserved_1[0x8];
	u8 pd[0x18];
	u8 reserved_2[0x20];
};

struct mlx5_ifc_alloc_pd_in_bits {
	u8 opcode[0x10];
	u8 reserved_0[0x10];
	u8 reserved_1[0x10];
	u8 op_mod[0x10];
	u8 reserved_2[0x40];
};

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct mlx5_ifc_query_qp_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	u8 opt_param_mask[0x20];
	u8 reserved_at_a0[0x20];
	struct mlx5_ifc_qpc_bits qpc;
	u8 reserved_at_800[0x80];
	u8 pas[0][0x40];
};
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

struct mlx5_ifc_query_qp_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x8];
	u8 qpn[0x18];
	u8 reserved_at_60[0x20];
};

enum {
	MLX5_DATA_RATE = 0x0,
	MLX5_WQE_RATE = 0x1,
};

struct mlx5_ifc_set_pp_rate_limit_context_bits {
	u8 rate_limit[0x20];
	u8 burst_upper_bound[0x20];
	u8 reserved_at_40[0xC];
	u8 rate_mode[0x4];
	u8 typical_packet_size[0x10];
	u8 reserved_at_60[0x120];
};

#define MLX5_ACCESS_REGISTER_DATA_DWORD_MAX 8u

#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
struct mlx5_ifc_access_register_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	u8 register_data[0][0x20];
};

struct mlx5_ifc_access_register_in_bits {
	u8 opcode[0x10];
	u8 reserved_at_10[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x10];
	u8 register_id[0x10];
	u8 argument[0x20];
	u8 register_data[0][0x20];
};
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

enum {
	MLX5_ACCESS_REGISTER_IN_OP_MOD_WRITE  = 0x0,
	MLX5_ACCESS_REGISTER_IN_OP_MOD_READ   = 0x1,
};

enum {
	MLX5_REGISTER_ID_MTUTC  = 0x9055,
	MLX5_CRYPTO_OPERATIONAL_REGISTER_ID = 0xC002,
	MLX5_CRYPTO_COMMISSIONING_REGISTER_ID = 0xC003,
	MLX5_IMPORT_KEK_HANDLE_REGISTER_ID = 0xC004,
	MLX5_CREDENTIAL_HANDLE_REGISTER_ID = 0xC005,
};

struct mlx5_ifc_register_mtutc_bits {
	u8 time_stamp_mode[0x2];
	u8 time_stamp_state[0x2];
	u8 reserved_at_4[0x18];
	u8 operation[0x4];
	u8 freq_adjustment[0x20];
	u8 reserved_at_40[0x40];
	u8 utc_sec[0x20];
	u8 utc_nsec[0x20];
	u8 time_adjustment[0x20];
};

#define MLX5_MTUTC_TIMESTAMP_MODE_INTERNAL_TIMER 0
#define MLX5_MTUTC_TIMESTAMP_MODE_REAL_TIME 1

struct mlx5_ifc_crypto_operational_register_bits {
	u8 wrapped_crypto_operational[0x1];
	u8 reserved_at_1[0x1b];
	u8 kek_size[0x4];
	u8 reserved_at_20[0x20];
	u8 credential[0x140];
	u8 kek[0x100];
	u8 reserved_at_280[0x180];
};

struct mlx5_ifc_crypto_commissioning_register_bits {
	u8 token[0x1]; /* TODO: add size after PRM update */
};

struct mlx5_ifc_import_kek_handle_register_bits {
	struct mlx5_ifc_crypto_login_bits crypto_login_object;
	struct mlx5_ifc_import_kek_bits import_kek_object;
	u8 reserved_at_200[0x4];
	u8 write_operation[0x4];
	u8 import_kek_id[0x18];
	u8 reserved_at_220[0xe0];
};

struct mlx5_ifc_credential_handle_register_bits {
	struct mlx5_ifc_crypto_login_bits crypto_login_object;
	struct mlx5_ifc_credential_bits credential_object;
	u8 reserved_at_200[0x4];
	u8 write_operation[0x4];
	u8 credential_id[0x18];
	u8 reserved_at_220[0xe0];
};

enum {
	MLX5_REGISTER_ADD_OPERATION = 0x1,
	MLX5_REGISTER_DELETE_OPERATION = 0x2,
};

struct mlx5_ifc_parse_graph_arc_bits {
	u8 start_inner_tunnel[0x1];
	u8 reserved_at_1[0x7];
	u8 arc_parse_graph_node[0x8];
	u8 compare_condition_value[0x10];
	u8 parse_graph_node_handle[0x20];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_parse_graph_flow_match_sample_bits {
	u8 flow_match_sample_en[0x1];
	u8 reserved_at_1[0x3];
	u8 flow_match_sample_offset_mode[0x4];
	u8 reserved_at_5[0x8];
	u8 flow_match_sample_field_offset[0x10];
	u8 reserved_at_32[0x4];
	u8 flow_match_sample_field_offset_shift[0x4];
	u8 flow_match_sample_field_base_offset[0x8];
	u8 reserved_at_48[0xd];
	u8 flow_match_sample_tunnel_mode[0x3];
	u8 flow_match_sample_field_offset_mask[0x20];
	u8 flow_match_sample_field_id[0x20];
};

struct mlx5_ifc_parse_graph_flex_bits {
	u8 modify_field_select[0x40];
	u8 reserved_at_64[0x20];
	u8 header_length_base_value[0x10];
	u8 reserved_at_112[0x4];
	u8 header_length_field_shift[0x4];
	u8 reserved_at_120[0x4];
	u8 header_length_mode[0x4];
	u8 header_length_field_offset[0x10];
	u8 next_header_field_offset[0x10];
	u8 reserved_at_160[0x1b];
	u8 next_header_field_size[0x5];
	u8 header_length_field_mask[0x20];
	u8 reserved_at_224[0x20];
	struct mlx5_ifc_parse_graph_flow_match_sample_bits sample_table[0x8];
	struct mlx5_ifc_parse_graph_arc_bits input_arc[0x8];
	struct mlx5_ifc_parse_graph_arc_bits output_arc[0x8];
};

struct mlx5_ifc_create_flex_parser_in_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_parse_graph_flex_bits flex;
};

struct mlx5_ifc_create_flex_parser_out_bits {
	struct mlx5_ifc_general_obj_in_cmd_hdr_bits hdr;
	struct mlx5_ifc_parse_graph_flex_bits flex;
};

struct mlx5_ifc_parse_graph_flex_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	struct mlx5_ifc_parse_graph_flex_bits capability;
};

struct regexp_params_field_select_bits {
	u8 reserved_at_0[0x1e];
	u8 stop_engine[0x1];
	u8 db_umem_id[0x1];
};

struct mlx5_ifc_regexp_params_bits {
	u8 reserved_at_0[0x1f];
	u8 stop_engine[0x1];
	u8 db_umem_id[0x20];
	u8 db_umem_offset[0x40];
	u8 reserved_at_80[0x100];
};

struct mlx5_ifc_set_regexp_params_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x18];
	u8 engine_id[0x8];
	struct regexp_params_field_select_bits field_select;
	struct mlx5_ifc_regexp_params_bits regexp_params;
};

struct mlx5_ifc_set_regexp_params_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_18[0x40];
};

struct mlx5_ifc_query_regexp_params_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x18];
	u8 engine_id[0x8];
	u8 reserved[0x20];
};

struct mlx5_ifc_query_regexp_params_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved[0x40];
	struct mlx5_ifc_regexp_params_bits regexp_params;
};

struct mlx5_ifc_set_regexp_register_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x18];
	u8 engine_id[0x8];
	u8 register_address[0x20];
	u8 register_data[0x20];
	u8 reserved[0x60];
};

struct mlx5_ifc_set_regexp_register_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved[0x40];
};

struct mlx5_ifc_query_regexp_register_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x18];
	u8 engine_id[0x8];
	u8 register_address[0x20];
};

struct mlx5_ifc_query_regexp_register_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved[0x20];
	u8 register_data[0x20];
};

/* Queue counters. */
struct mlx5_ifc_alloc_q_counter_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x18];
	u8 counter_set_id[0x8];
	u8 reserved_at_60[0x20];
};

struct mlx5_ifc_alloc_q_counter_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x40];
};

struct mlx5_ifc_query_q_counter_out_bits {
	u8 status[0x8];
	u8 reserved_at_8[0x18];
	u8 syndrome[0x20];
	u8 reserved_at_40[0x40];
	u8 rx_write_requests[0x20];
	u8 reserved_at_a0[0x20];
	u8 rx_read_requests[0x20];
	u8 reserved_at_e0[0x20];
	u8 rx_atomic_requests[0x20];
	u8 reserved_at_120[0x20];
	u8 rx_dct_connect[0x20];
	u8 reserved_at_160[0x20];
	u8 out_of_buffer[0x20];
	u8 reserved_at_1a0[0x20];
	u8 out_of_sequence[0x20];
	u8 reserved_at_1e0[0x20];
	u8 duplicate_request[0x20];
	u8 reserved_at_220[0x20];
	u8 rnr_nak_retry_err[0x20];
	u8 reserved_at_260[0x20];
	u8 packet_seq_err[0x20];
	u8 reserved_at_2a0[0x20];
	u8 implied_nak_seq_err[0x20];
	u8 reserved_at_2e0[0x20];
	u8 local_ack_timeout_err[0x20];
	u8 reserved_at_320[0xa0];
	u8 resp_local_length_error[0x20];
	u8 req_local_length_error[0x20];
	u8 resp_local_qp_error[0x20];
	u8 local_operation_error[0x20];
	u8 resp_local_protection[0x20];
	u8 req_local_protection[0x20];
	u8 resp_cqe_error[0x20];
	u8 req_cqe_error[0x20];
	u8 req_mw_binding[0x20];
	u8 req_bad_response[0x20];
	u8 req_remote_invalid_request[0x20];
	u8 resp_remote_invalid_request[0x20];
	u8 req_remote_access_errors[0x20];
	u8 resp_remote_access_errors[0x20];
	u8 req_remote_operation_errors[0x20];
	u8 req_transport_retries_exceeded[0x20];
	u8 cq_overflow[0x20];
	u8 resp_cqe_flush_error[0x20];
	u8 req_cqe_flush_error[0x20];
	u8 reserved_at_620[0x1e0];
};

struct mlx5_ifc_query_q_counter_in_bits {
	u8 opcode[0x10];
	u8 uid[0x10];
	u8 reserved_at_20[0x10];
	u8 op_mod[0x10];
	u8 reserved_at_40[0x80];
	u8 clear[0x1];
	u8 reserved_at_c1[0x1f];
	u8 reserved_at_e0[0x18];
	u8 counter_set_id[0x8];
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
			union {
				uint16_t checksum;
				uint16_t flow_tag_high;
				struct {
					uint8_t reserved;
					uint8_t hdr_type;
				};
			};
			uint16_t stride_idx;
		};
		struct {
			uint16_t wqe_counter;
			uint8_t  s_wqe_opcode;
			uint8_t  reserved;
		} s_wqe_info;
	};
	union {
		uint32_t byte_cnt_flow;
		uint32_t byte_cnt;
	};
};

/* Mini CQE responder format. */
enum {
	MLX5_CQE_RESP_FORMAT_HASH = 0x0,
	MLX5_CQE_RESP_FORMAT_CSUM = 0x1,
	MLX5_CQE_RESP_FORMAT_FTAG_STRIDX = 0x2,
	MLX5_CQE_RESP_FORMAT_CSUM_STRIDX = 0x3,
	MLX5_CQE_RESP_FORMAT_L34H_STRIDX = 0x4,
};

/* srTCM PRM flow meter parameters. */
enum {
	MLX5_FLOW_COLOR_RED = 0,
	MLX5_FLOW_COLOR_YELLOW,
	MLX5_FLOW_COLOR_GREEN,
	MLX5_FLOW_COLOR_UNDEFINED,
};

/* Maximum value of srTCM & trTCM metering parameters. */
#define MLX5_SRTCM_XBS_MAX (0xFF * (1ULL << 0x1F))
#define MLX5_SRTCM_XIR_MAX (8 * (1ULL << 30) * 0xFF)

/* The bits meter color use. */
#define MLX5_MTR_COLOR_BITS 8

/* The bit size of one register. */
#define MLX5_REG_BITS 32

/* Idle bits for non-color usage in color register. */
#define MLX5_MTR_IDLE_BITS_IN_COLOR_REG (MLX5_REG_BITS - MLX5_MTR_COLOR_BITS)

/* Length mode of dynamic flex parser graph node. */
enum mlx5_parse_graph_node_len_mode {
	MLX5_GRAPH_NODE_LEN_FIXED = 0x0,
	MLX5_GRAPH_NODE_LEN_FIELD = 0x1,
	MLX5_GRAPH_NODE_LEN_BITMASK = 0x2,
};

/* Offset mode of the samples of flex parser. */
enum mlx5_parse_graph_flow_match_sample_offset_mode {
	MLX5_GRAPH_SAMPLE_OFFSET_FIXED = 0x0,
	MLX5_GRAPH_SAMPLE_OFFSET_FIELD = 0x1,
	MLX5_GRAPH_SAMPLE_OFFSET_BITMASK = 0x2,
};

/* Node index for an input / output arc of the flex parser graph. */
enum mlx5_parse_graph_arc_node_index {
	MLX5_GRAPH_ARC_NODE_NULL = 0x0,
	MLX5_GRAPH_ARC_NODE_HEAD = 0x1,
	MLX5_GRAPH_ARC_NODE_MAC = 0x2,
	MLX5_GRAPH_ARC_NODE_IP = 0x3,
	MLX5_GRAPH_ARC_NODE_GRE = 0x4,
	MLX5_GRAPH_ARC_NODE_UDP = 0x5,
	MLX5_GRAPH_ARC_NODE_MPLS = 0x6,
	MLX5_GRAPH_ARC_NODE_TCP = 0x7,
	MLX5_GRAPH_ARC_NODE_VXLAN_GPE = 0x8,
	MLX5_GRAPH_ARC_NODE_GENEVE = 0x9,
	MLX5_GRAPH_ARC_NODE_IPSEC_ESP = 0xa,
	MLX5_GRAPH_ARC_NODE_PROGRAMMABLE = 0x1f,
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

/**
 * Convert a timestamp format to configure settings in the queue context.
 *
 * @param val
 *   timestamp format supported by the queue.
 *
 * @return
 *   Converted timstamp format settings.
 */
static inline uint32_t
mlx5_ts_format_conv(uint32_t ts_format)
{
	return ts_format == MLX5_HCA_CAP_TIMESTAMP_FORMAT_FR ?
			MLX5_QPC_TIMESTAMP_FORMAT_FREE_RUNNING :
			MLX5_QPC_TIMESTAMP_FORMAT_DEFAULT;
}

#endif /* RTE_PMD_MLX5_PRM_H_ */
