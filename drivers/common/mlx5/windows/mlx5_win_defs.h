/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C) Mellanox Technologies, Ltd. 2001-2020.
 */

#ifndef MLX5_WIN_DEFS_H
#define MLX5_WIN_DEFS_H

#include <rte_bitops.h>

enum {
	MLX5_CQE_OWNER_MASK	= 1,
	MLX5_CQE_REQ		= 0,
	MLX5_CQE_RESP_WR_IMM	= 1,
	MLX5_CQE_RESP_SEND	= 2,
	MLX5_CQE_RESP_SEND_IMM	= 3,
	MLX5_CQE_RESP_SEND_INV	= 4,
	MLX5_CQE_RESIZE_CQ	= 5,
	MLX5_CQE_NO_PACKET	= 6,
	MLX5_CQE_REQ_ERR	= 13,
	MLX5_CQE_RESP_ERR	= 14,
	MLX5_CQE_INVALID	= 15,
};

enum {
	MLX5_OPCODE_NOP			= 0x00,
	MLX5_OPCODE_SEND_INVAL		= 0x01,
	MLX5_OPCODE_RDMA_WRITE		= 0x08,
	MLX5_OPCODE_RDMA_WRITE_IMM	= 0x09,
	MLX5_OPCODE_SEND		= 0x0a,
	MLX5_OPCODE_SEND_IMM		= 0x0b,
	MLX5_OPCODE_TSO			= 0x0e,
	MLX5_OPCODE_RDMA_READ		= 0x10,
	MLX5_OPCODE_ATOMIC_CS		= 0x11,
	MLX5_OPCODE_ATOMIC_FA		= 0x12,
	MLX5_OPCODE_ATOMIC_MASKED_CS	= 0x14,
	MLX5_OPCODE_ATOMIC_MASKED_FA	= 0x15,
	MLX5_OPCODE_FMR			= 0x19,
	MLX5_OPCODE_LOCAL_INVAL		= 0x1b,
	MLX5_OPCODE_CONFIG_CMD		= 0x1f,
	MLX5_OPCODE_UMR			= 0x25,
	MLX5_OPCODE_TAG_MATCHING	= 0x28
};

enum mlx5dv_cq_init_attr_mask {
	MLX5DV_CQ_INIT_ATTR_MASK_COMPRESSED_CQE = RTE_BIT32(0),
	MLX5DV_CQ_INIT_ATTR_MASK_FLAG           = RTE_BIT32(1),
	MLX5DV_CQ_INIT_ATTR_MASK_CQE_SIZE       = RTE_BIT32(2),
};

enum mlx5dv_cqe_comp_res_format {
	MLX5DV_CQE_RES_FORMAT_HASH        = RTE_BIT32(0),
	MLX5DV_CQE_RES_FORMAT_CSUM        = RTE_BIT32(1),
	MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX = RTE_BIT32(2),
};

enum ibv_access_flags {
	IBV_ACCESS_LOCAL_WRITE   = RTE_BIT32(0),
	IBV_ACCESS_REMOTE_WRITE  = RTE_BIT32(1),
	IBV_ACCESS_REMOTE_READ   = RTE_BIT32(2),
	IBV_ACCESS_REMOTE_ATOMIC = RTE_BIT32(3),
	IBV_ACCESS_MW_BIND       = RTE_BIT32(4),
	IBV_ACCESS_ZERO_BASED    = RTE_BIT32(5),
	IBV_ACCESS_ON_DEMAND     = RTE_BIT32(6),
};

enum mlx5_ib_uapi_devx_create_event_channel_flags {
	MLX5_IB_UAPI_DEVX_CR_EV_CH_FLAGS_OMIT_DATA = RTE_BIT32(0),
};

#define MLX5DV_DEVX_CREATE_EVENT_CHANNEL_FLAGS_OMIT_EV_DATA \
	MLX5_IB_UAPI_DEVX_CR_EV_CH_FLAGS_OMIT_DATA

enum {
	MLX5_CQE_SYNDROME_LOCAL_LENGTH_ERR		= 0x01,
	MLX5_CQE_SYNDROME_LOCAL_QP_OP_ERR		= 0x02,
	MLX5_CQE_SYNDROME_LOCAL_PROT_ERR		= 0x04,
	MLX5_CQE_SYNDROME_WR_FLUSH_ERR			= 0x05,
	MLX5_CQE_SYNDROME_MW_BIND_ERR			= 0x06,
	MLX5_CQE_SYNDROME_BAD_RESP_ERR			= 0x10,
	MLX5_CQE_SYNDROME_LOCAL_ACCESS_ERR		= 0x11,
	MLX5_CQE_SYNDROME_REMOTE_INVAL_REQ_ERR		= 0x12,
	MLX5_CQE_SYNDROME_REMOTE_ACCESS_ERR		= 0x13,
	MLX5_CQE_SYNDROME_REMOTE_OP_ERR			= 0x14,
	MLX5_CQE_SYNDROME_TRANSPORT_RETRY_EXC_ERR	= 0x15,
	MLX5_CQE_SYNDROME_RNR_RETRY_EXC_ERR		= 0x16,
	MLX5_CQE_SYNDROME_REMOTE_ABORTED_ERR		= 0x22,
};

enum {
	MLX5_ETH_WQE_L3_CSUM = RTE_BIT32(6),
	MLX5_ETH_WQE_L4_CSUM = RTE_BIT32(7),
};

enum {
	MLX5_WQE_CTRL_SOLICITED             = RTE_BIT32(1),
	MLX5_WQE_CTRL_CQ_UPDATE             = RTE_BIT32(3),
	MLX5_WQE_CTRL_INITIATOR_SMALL_FENCE = RTE_BIT32(5),
	MLX5_WQE_CTRL_FENCE                 = RTE_BIT32(7),
};

enum {
	MLX5_SEND_WQE_BB	= 64,
	MLX5_SEND_WQE_SHIFT	= 6,
};

/* Verbs headers do not support -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif

/*
 * RX Hash fields enable to set which incoming packet's field should
 * participates in RX Hash. Each flag represent certain packet's field,
 * when the flag is set the field that is represented by the flag will
 * participate in RX Hash calculation.
 * Note: IPV4 and IPV6 flags can't be enabled together on the same QP,
 * TCP and UDP flags can't be enabled together on the same QP.
 */
enum ibv_rx_hash_fields {
	IBV_RX_HASH_SRC_IPV4     = RTE_BIT32(0),
	IBV_RX_HASH_DST_IPV4     = RTE_BIT32(1),
	IBV_RX_HASH_SRC_IPV6     = RTE_BIT32(2),
	IBV_RX_HASH_DST_IPV6     = RTE_BIT32(3),
	IBV_RX_HASH_SRC_PORT_TCP = RTE_BIT32(4),
	IBV_RX_HASH_DST_PORT_TCP = RTE_BIT32(5),
	IBV_RX_HASH_SRC_PORT_UDP = RTE_BIT32(6),
	IBV_RX_HASH_DST_PORT_UDP = RTE_BIT32(7),
	IBV_RX_HASH_IPSEC_SPI    = RTE_BIT32(8),
	IBV_RX_HASH_INNER        = RTE_BIT32(31),
};

#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

enum {
	MLX5_RCV_DBR	= 0,
	MLX5_SND_DBR	= 1,
};

#ifndef MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TUNNEL_TO_L2
#define MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TUNNEL_TO_L2	0x0
#endif
#ifndef MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L2_TUNNEL
#define MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L2_TUNNEL	0x1
#endif
#ifndef MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L3_TUNNEL_TO_L2
#define MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L3_TUNNEL_TO_L2	0x2
#endif
#ifndef MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L3_TUNNEL
#define MLX5DV_FLOW_ACTION_PACKET_REFORMAT_TYPE_L2_TO_L3_TUNNEL	0x3
#endif

enum ibv_flow_flags {
	IBV_FLOW_ATTR_FLAGS_ALLOW_LOOP_BACK = RTE_BIT32(0),
	IBV_FLOW_ATTR_FLAGS_DONT_TRAP = RTE_BIT32(1),
	IBV_FLOW_ATTR_FLAGS_EGRESS = RTE_BIT32(2),
};

enum ibv_flow_attr_type {
	/* Steering according to rule specifications. */
	IBV_FLOW_ATTR_NORMAL		= 0x0,
	/*
	 * Default unicast and multicast rule -
	 * receive all Eth traffic which isn't steered to any QP.
	 */
	IBV_FLOW_ATTR_ALL_DEFAULT	= 0x1,
	/*
	 * Default multicast rule -
	 * receive all Eth multicast traffic which isn't steered to any QP.
	 */
	IBV_FLOW_ATTR_MC_DEFAULT	= 0x2,
	/* Sniffer rule - receive all port traffic. */
	IBV_FLOW_ATTR_SNIFFER		= 0x3,
};

enum mlx5dv_flow_table_type {
	MLX5_IB_UAPI_FLOW_TABLE_TYPE_NIC_RX     = 0x0,
	MLX5_IB_UAPI_FLOW_TABLE_TYPE_NIC_TX	= 0x1,
	MLX5_IB_UAPI_FLOW_TABLE_TYPE_FDB	= 0x2,
	MLX5_IB_UAPI_FLOW_TABLE_TYPE_RDMA_RX	= 0x3,
};

#define MLX5DV_FLOW_TABLE_TYPE_NIC_RX	MLX5_IB_UAPI_FLOW_TABLE_TYPE_NIC_RX
#define MLX5DV_FLOW_TABLE_TYPE_NIC_TX	MLX5_IB_UAPI_FLOW_TABLE_TYPE_NIC_TX
#define MLX5DV_FLOW_TABLE_TYPE_FDB	MLX5_IB_UAPI_FLOW_TABLE_TYPE_FDB
#define MLX5DV_FLOW_TABLE_TYPE_RDMA_RX	MLX5_IB_UAPI_FLOW_TABLE_TYPE_RDMA_RX

struct mlx5dv_flow_match_parameters {
	size_t match_sz;
	uint64_t match_buf[]; /* Device spec format */
};

struct mlx5dv_flow_matcher_attr {
	enum ibv_flow_attr_type type;
	uint32_t flags; /* From enum ibv_flow_flags. */
	uint16_t priority;
	uint8_t match_criteria_enable; /* Device spec format. */
	struct mlx5dv_flow_match_parameters *match_mask;
	uint64_t comp_mask; /* Use mlx5dv_flow_matcher_attr_mask. */
	enum mlx5dv_flow_table_type ft_type;
};

/* Windows specific mlx5_matcher. */
struct mlx5_matcher {
	void *ctx;
	struct mlx5dv_flow_matcher_attr attr;
	uint64_t match_buf[];
};

/*
 * Windows mlx5_action. This struct is the
 * equivalent of rdma-core struct mlx5dv_dr_action.
 */
struct mlx5_action {
	int type;
	struct {
		uint32_t id;
	} dest_tir;
};

struct mlx5_err_cqe {
	uint8_t		rsvd0[32];
	uint32_t	srqn;
	uint8_t		rsvd1[18];
	uint8_t		vendor_err_synd;
	uint8_t		syndrome;
	uint32_t	s_wqe_opcode_qpn;
	uint16_t	wqe_counter;
	uint8_t		signature;
	uint8_t		op_own;
};

struct mlx5_wqe_srq_next_seg {
	uint8_t			rsvd0[2];
	rte_be16_t		next_wqe_index;
	uint8_t			signature;
	uint8_t			rsvd1[11];
};

enum ibv_wq_state {
	IBV_WQS_RESET,
	IBV_WQS_RDY,
	IBV_WQS_ERR,
	IBV_WQS_UNKNOWN
};

struct mlx5_wqe_data_seg {
	rte_be32_t		byte_count;
	rte_be32_t		lkey;
	rte_be64_t		addr;
};

#define MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP   RTE_BIT32(4)
#define IBV_DEVICE_RAW_IP_CSUM               RTE_BIT32(26)
#define IBV_RAW_PACKET_CAP_CVLAN_STRIPPING   RTE_BIT32(0)
#define IBV_RAW_PACKET_CAP_SCATTER_FCS       RTE_BIT32(1)
#define IBV_QPT_RAW_PACKET                   8

enum {
	MLX5_FLOW_CONTEXT_DEST_TYPE_VPORT                    = 0x0,
	MLX5_FLOW_CONTEXT_DEST_TYPE_FLOW_TABLE               = 0x1,
	MLX5_FLOW_CONTEXT_DEST_TYPE_TIR                      = 0x2,
	MLX5_FLOW_CONTEXT_DEST_TYPE_QP                       = 0x3,
};

enum {
	MLX5_MATCH_OUTER_HEADERS        = RTE_BIT32(0),
	MLX5_MATCH_MISC_PARAMETERS      = RTE_BIT32(1),
	MLX5_MATCH_INNER_HEADERS        = RTE_BIT32(2),
};

#endif /* MLX5_WIN_DEFS_H */
