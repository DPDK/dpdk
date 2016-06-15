/*-
 *   BSD LICENSE
 *
 *   Copyright(c) Broadcom Limited.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Broadcom Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _HSI_STRUCT_DEF_EXTERNAL_H_
#define _HSI_STRUCT_DEF_EXTERNAL_H_

/*
 * per-context HW statistics -- chip view
 */

struct ctx_hw_stats64 {
	uint64_t rx_ucast_pkts;
	uint64_t rx_mcast_pkts;
	uint64_t rx_bcast_pkts;
	uint64_t rx_drop_pkts;
	uint64_t rx_err_pkts;
	uint64_t rx_ucast_bytes;
	uint64_t rx_mcast_bytes;
	uint64_t rx_bcast_bytes;

	uint64_t tx_ucast_pkts;
	uint64_t tx_mcast_pkts;
	uint64_t tx_bcast_pkts;
	uint64_t tx_drop_pkts;
	uint64_t tx_err_pkts;
	uint64_t tx_ucast_bytes;
	uint64_t tx_mcast_bytes;
	uint64_t tx_bcast_bytes;

	uint64_t tpa_pkts;
	uint64_t tpa_bytes;
	uint64_t tpa_events;
	uint64_t tpa_aborts;
} ctx_hw_stats64_t;

/* HW Resource Manager Specification 1.2.0 */
#define HWRM_VERSION_MAJOR	1
#define HWRM_VERSION_MINOR	2
#define HWRM_VERSION_UPDATE	0

/*
 * Following is the signature for HWRM message field that indicates not
 * applicable (All F's). Need to cast it the size of the field if needed.
 */
#define HWRM_NA_SIGNATURE        ((uint32_t)(-1))
#define HWRM_MAX_REQ_LEN	(128)  /* hwrm_func_buf_rgtr */
#define HWRM_MAX_RESP_LEN	(176)  /* hwrm_func_qstats */
#define HW_HASH_INDEX_SIZE      0x80    /* 7 bit indirection table index. */
#define HW_HASH_KEY_SIZE        40
#define HWRM_RESP_VALID_KEY	1 /* valid key for HWRM response */

/*
 * Request types
 */
#define HWRM_VER_GET			(UINT32_C(0x0))
#define HWRM_FUNC_QCAPS			(UINT32_C(0x15))
#define HWRM_FUNC_DRV_UNRGTR		(UINT32_C(0x1a))
#define HWRM_FUNC_DRV_RGTR		(UINT32_C(0x1d))
#define HWRM_PORT_PHY_CFG		(UINT32_C(0x20))
#define HWRM_QUEUE_QPORTCFG		(UINT32_C(0x30))
#define HWRM_CFA_L2_FILTER_ALLOC	(UINT32_C(0x90))
#define HWRM_CFA_L2_FILTER_FREE		(UINT32_C(0x91))
#define HWRM_CFA_L2_FILTER_CFG		(UINT32_C(0x92))
#define HWRM_CFA_L2_SET_RX_MASK		(UINT32_C(0x93))
#define HWRM_STAT_CTX_CLR_STATS		(UINT32_C(0xb3))
#define HWRM_EXEC_FWD_RESP		(UINT32_C(0xd0))

/* Return Codes */
#define HWRM_ERR_CODE_INVALID_PARAMS                      (UINT32_C(0x2))
#define HWRM_ERR_CODE_RESOURCE_ACCESS_DENIED              (UINT32_C(0x3))

/* HWRM Forwarded Request (16 bytes) */
struct hwrm_fwd_req_cmpl {
	/* Length of forwarded request in bytes. */
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records.
	 */
	#define HWRM_FWD_REQ_CMPL_TYPE_MASK		UINT32_C(0x3f)
	#define HWRM_FWD_REQ_CMPL_TYPE_SFT		0
		/* Forwarded HWRM Request */
	#define HWRM_FWD_REQ_CMPL_TYPE_HWRM_FWD_REQ	(UINT32_C(0x22) << 0)
	/* Length of forwarded request in bytes. */
	#define HWRM_FWD_REQ_CMPL_REQ_LEN_MASK		UINT32_C(0xffc0)
	#define HWRM_FWD_REQ_CMPL_REQ_LEN_SFT		6
	uint16_t req_len_type;

	/*
	 * Source ID of this request. Typically used in forwarding requests and
	 * responses. 0x0 - 0xFFF8 - Used for function ids 0xFFF8 - 0xFFFE -
	 * Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t source_id;

	uint32_t unused_0;

	/* Address of forwarded request. */
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define HWRM_FWD_REQ_CMPL_V			UINT32_C(0x1)
	/* Address of forwarded request. */
	#define HWRM_FWD_REQ_CMPL_REQ_BUF_ADDR_MASK	UINT32_C(0xfffffffe)
	#define HWRM_FWD_REQ_CMPL_REQ_BUF_ADDR_SFT	1
	uint64_t req_buf_addr_v;
} __attribute__((packed));

/* HWRM Asynchronous Event Completion Record (16 bytes) */
struct hwrm_async_event_cmpl {
	/*
	 * This field indicates the exact type of the completion. By convention,
	 * the LSB identifies the length of the record in 16B units. Even values
	 * indicate 16B records. Odd values indicate 32B records.
	 */
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_MASK		UINT32_C(0x3f)
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_SFT		0
		/* HWRM Asynchronous Event Information */
	#define HWRM_ASYNC_EVENT_CMPL_TYPE_HWRM_ASYNC_EVENT \
							(UINT32_C(0x2e) << 0)
	uint16_t type;

	/* Identifiers of events. */
		/* Link status changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_STATUS_CHANGE \
							(UINT32_C(0x0) << 0)
		/* Link MTU changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_MTU_CHANGE \
							(UINT32_C(0x1) << 0)
		/* Link speed changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CHANGE \
							(UINT32_C(0x2) << 0)
		/* DCB Configuration changed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_DCB_CONFIG_CHANGE \
							(UINT32_C(0x3) << 0)
		/* Port connection not allowed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PORT_CONN_NOT_ALLOWED \
							(UINT32_C(0x4) << 0)
		/* Link speed configuration was not allowed */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_LINK_SPEED_CFG_NOT_ALLOWED \
							(UINT32_C(0x5) << 0)
		/* Function driver unloaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_UNLOAD \
							(UINT32_C(0x10) << 0)
		/* Function driver loaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_FUNC_DRVR_LOAD \
							(UINT32_C(0x11) << 0)
		/* PF driver unloaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_UNLOAD \
							(UINT32_C(0x20) << 0)
		/* PF driver loaded */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_DRVR_LOAD \
							(UINT32_C(0x21) << 0)
		/* VF Function Level Reset (FLR) */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_FLR	(UINT32_C(0x30) << 0)
		/* VF MAC Address Change */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_VF_MAC_ADDR_CHANGE \
							(UINT32_C(0x31) << 0)
		/* PF-VF communication channel status change. */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_PF_VF_COMM_STATUS_CHANGE \
							(UINT32_C(0x32) << 0)
		/* HWRM Error */
	#define HWRM_ASYNC_EVENT_CMPL_EVENT_ID_HWRM_ERROR \
							(UINT32_C(0xff) << 0)
	uint16_t event_id;

	/* Event specific data */
	uint32_t event_data2;

	/* opaque is 7 b */
	/*
	 * This value is written by the NIC such that it will be different for
	 * each pass through the completion queue. The even passes will write 1.
	 * The odd passes will write 0.
	 */
	#define HWRM_ASYNC_EVENT_CMPL_V				UINT32_C(0x1)
	/* opaque is 7 b */
	#define HWRM_ASYNC_EVENT_CMPL_OPAQUE_MASK		UINT32_C(0xfe)
	#define HWRM_ASYNC_EVENT_CMPL_OPAQUE_SFT		1
	uint8_t opaque_v;

	/* 8-lsb timestamp from POR (100-msec resolution) */
	uint8_t timestamp_lo;

	/* 16-lsb timestamp from POR (100-msec resolution) */
	uint16_t timestamp_hi;

	/* Event specific data */
	uint32_t event_data1;
} __attribute__((packed));

/*
 * Note: The Hardware Resource Manager (HWRM) manages various hardware resources
 * inside the chip. The HWRM is implemented in firmware, and runs on embedded
 * processors inside the chip. This firmware is vital part of the chip's
 * hardware. The chip can not be used by driver without it.
 */

/* Input (16 bytes) */
struct input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;
} __attribute__((packed));

/* Output (8 bytes) */
struct output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;
} __attribute__((packed));

/* hwrm_cfa_l2_filter_alloc */
/*
 * A filter is used to identify traffic that contains a matching set of
 * parameters like unicast or broadcast MAC address or a VLAN tag amongst
 * other things which then allows the ASIC to direct the  incoming traffic
 * to an appropriate VNIC or Rx ring.
 */

/* Input (96 bytes) */
struct hwrm_cfa_l2_filter_alloc_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both TX and RX
	 * paths of the chip.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH \
							UINT32_C(0x1)
		/* tx path */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_TX \
							(UINT32_C(0x0) << 0)
		/* rx path */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX \
							(UINT32_C(0x1) << 0)
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_LAST \
				HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_PATH_RX
	/*
	 * Setting of this flag indicates the applicability to the loopback
	 * path.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_LOOPBACK \
							UINT32_C(0x2)
	/*
	 * Setting of this flag indicates drop action. If this flag is not set,
	 * then it should be considered accept action.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_DROP \
							UINT32_C(0x4)
	/*
	 * If this flag is set, all t_l2_* fields are invalid and they should
	 * not be specified. If this flag is set, then l2_* fields refer to
	 * fields of outermost L2 header.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_FLAGS_OUTERMOST \
							UINT32_C(0x8)
	uint32_t flags;

	/* This bit must be '1' for the l2_addr field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR \
							UINT32_C(0x1)
	/* This bit must be '1' for the l2_addr_mask field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_ADDR_MASK \
							UINT32_C(0x2)
	/* This bit must be '1' for the l2_ovlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN \
							UINT32_C(0x4)
	/* This bit must be '1' for the l2_ovlan_mask field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_OVLAN_MASK \
							UINT32_C(0x8)
	/* This bit must be '1' for the l2_ivlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN \
							UINT32_C(0x10)
	/* This bit must be '1' for the l2_ivlan_mask field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_L2_IVLAN_MASK \
							UINT32_C(0x20)
	/* This bit must be '1' for the t_l2_addr field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_ADDR \
							UINT32_C(0x40)
	/*
	 * This bit must be '1' for the t_l2_addr_mask field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_ADDR_MASK \
							UINT32_C(0x80)
	/* This bit must be '1' for the t_l2_ovlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_OVLAN \
							UINT32_C(0x100)
	/*
	 * This bit must be '1' for the t_l2_ovlan_mask field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_OVLAN_MASK \
							UINT32_C(0x200)
	/* This bit must be '1' for the t_l2_ivlan field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_IVLAN \
							UINT32_C(0x400)
	/*
	 * This bit must be '1' for the t_l2_ivlan_mask field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_T_L2_IVLAN_MASK \
							UINT32_C(0x800)
	/* This bit must be '1' for the src_type field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_TYPE \
							UINT32_C(0x1000)
	/* This bit must be '1' for the src_id field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_SRC_ID \
							UINT32_C(0x2000)
	/* This bit must be '1' for the tunnel_type field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_TUNNEL_TYPE \
							UINT32_C(0x4000)
	/* This bit must be '1' for the dst_id field to be configured. */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_DST_ID \
							UINT32_C(0x8000)
	/*
	 * This bit must be '1' for the mirror_vnic_id field to be configured.
	 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_ENABLES_MIRROR_VNIC_ID \
							UINT32_C(0x10000)
	uint32_t enables;

	/*
	 * This value sets the match value for the L2 MAC address. Destination
	 * MAC address for RX path. Source MAC address for TX path.
	 */
	uint8_t l2_addr[6];

	uint8_t unused_0;
	uint8_t unused_1;

	/*
	 * This value sets the mask value for the L2 address. A value of 0 will
	 * mask the corresponding bit from compare.
	 */
	uint8_t l2_addr_mask[6];

	/* This value sets VLAN ID value for outer VLAN. */
	uint16_t l2_ovlan;

	/*
	 * This value sets the mask value for the ovlan id. A value of 0 will
	 * mask the corresponding bit from compare.
	 */
	uint16_t l2_ovlan_mask;

	/* This value sets VLAN ID value for inner VLAN. */
	uint16_t l2_ivlan;

	/*
	 * This value sets the mask value for the ivlan id. A value of 0 will
	 * mask the corresponding bit from compare.
	 */
	uint16_t l2_ivlan_mask;

	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This value sets the match value for the tunnel L2 MAC address.
	 * Destination MAC address for RX path. Source MAC address for TX path.
	 */
	uint8_t t_l2_addr[6];

	uint8_t unused_4;
	uint8_t unused_5;

	/*
	 * This value sets the mask value for the tunnel L2 address. A value of
	 * 0 will mask the corresponding bit from compare.
	 */
	uint8_t t_l2_addr_mask[6];

	/* This value sets VLAN ID value for tunnel outer VLAN. */
	uint16_t t_l2_ovlan;

	/*
	 * This value sets the mask value for the tunnel ovlan id. A value of 0
	 * will mask the corresponding bit from compare.
	 */
	uint16_t t_l2_ovlan_mask;

	/* This value sets VLAN ID value for tunnel inner VLAN. */
	uint16_t t_l2_ivlan;

	/*
	 * This value sets the mask value for the tunnel ivlan id. A value of 0
	 * will mask the corresponding bit from compare.
	 */
	uint16_t t_l2_ivlan_mask;

	/* This value identifies the type of source of the packet. */
		/* Network port */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_NPORT \
							(UINT32_C(0x0) << 0)
		/* Physical function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_PF \
							(UINT32_C(0x1) << 0)
		/* Virtual function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_VF \
							(UINT32_C(0x2) << 0)
		/* Virtual NIC of a function */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_VNIC \
							(UINT32_C(0x3) << 0)
		/* Embedded processor for CFA management */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_KONG \
							(UINT32_C(0x4) << 0)
		/* Embedded processor for OOB management */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_APE \
							(UINT32_C(0x5) << 0)
		/* Embedded processor for RoCE */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_BONO \
							(UINT32_C(0x6) << 0)
		/* Embedded processor for network proxy functions */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_SRC_TYPE_TANG \
							(UINT32_C(0x7) << 0)
	uint8_t src_type;

	uint8_t unused_6;
	/*
	 * This value is the id of the source. For a network port, it represents
	 * port_id. For a physical function, it represents fid. For a virtual
	 * function, it represents vf_id. For a vnic, it represents vnic_id. For
	 * embedded processors, this id is not valid. Notes: 1. The function ID
	 * is implied if it src_id is not provided for a src_type that is either
	 */
	uint32_t src_id;

	/* Tunnel Type. */
		/* Non-tunnel */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NONTUNNEL \
							(UINT32_C(0x0) << 0)
		/* Virtual eXtensible Local Area Network (VXLAN) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_VXLAN \
							(UINT32_C(0x1) << 0)
		/*
		 * Network Virtualization Generic Routing Encapsulation (NVGRE)
		 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_NVGRE \
							(UINT32_C(0x2) << 0)
		/*
		 * Generic Routing Encapsulation (GRE) inside Ethernet payload
		 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_L2GRE \
							(UINT32_C(0x3) << 0)
		/* IP in IP */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPIP \
							(UINT32_C(0x4) << 0)
		/* Generic Network Virtualization Encapsulation (Geneve) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_GENEVE \
							(UINT32_C(0x5) << 0)
		/* Multi-Protocol Lable Switching (MPLS) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_MPLS \
							(UINT32_C(0x6) << 0)
		/* Stateless Transport Tunnel (STT) */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_STT \
							(UINT32_C(0x7) << 0)
		/*
		 * Generic Routing Encapsulation (GRE) inside IP datagram
		 * payload
		 */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_IPGRE \
							(UINT32_C(0x8) << 0)
		/* Any tunneled traffic */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_TUNNEL_TYPE_ANYTUNNEL \
							(UINT32_C(0xff) << 0)
	uint8_t tunnel_type;

	uint8_t unused_7;

	/*
	 * If set, this value shall represent the Logical VNIC ID of the
	 * destination VNIC for the RX path and network port id of the
	 * destination port for the TX path.
	 */
	uint16_t dst_id;

	/* Logical VNIC ID of the VNIC where traffic is mirrored. */
	uint16_t mirror_vnic_id;

	/*
	 * This hint is provided to help in placing the filter in the filter
	 * table.
	 */
		/* No preference */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_NO_PREFER \
							(UINT32_C(0x0) << 0)
		/* Above the given filter */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_ABOVE_FILTER \
							(UINT32_C(0x1) << 0)
		/* Below the given filter */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_BELOW_FILTER \
							(UINT32_C(0x2) << 0)
		/* As high as possible */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_MAX \
							(UINT32_C(0x3) << 0)
		/* As low as possible */
	#define HWRM_CFA_L2_FILTER_ALLOC_INPUT_PRI_HINT_MIN \
							(UINT32_C(0x4) << 0)
	uint8_t pri_hint;

	uint8_t unused_8;
	uint32_t unused_9;

	/*
	 * This is the ID of the filter that goes along with the pri_hint. This
	 * field is valid only for the following values. 1 - Above the given
	 * filter 2 - Below the given filter
	 */
	uint64_t l2_filter_id_hint;
} __attribute__((packed));

/* Output (24 bytes) */
struct hwrm_cfa_l2_filter_alloc_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/*
	 * This value identifies a set of CFA data structures used for an L2
	 * context.
	 */
	uint64_t l2_filter_id;

	/*
	 * This is the ID of the flow associated with this filter. This value
	 * shall be used to match and associate the flow identifier returned in
	 * completion records. A value of 0xFFFFFFFF shall indicate no flow id.
	 */
	uint32_t flow_id;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_cfa_l2_filter_free */
/*
 * Description: Free a L2 filter. The HWRM shall free all associated filter
 * resources with the L2 filter.
 */

/* Input (24 bytes) */
struct hwrm_cfa_l2_filter_free_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * This value identifies a set of CFA data structures used for an L2
	 * context.
	 */
	uint64_t l2_filter_id;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_cfa_l2_filter_free_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_exec_fwd_resp */
/*
 * Description: This command is used to send an encapsulated request to the
 * HWRM. This command instructs the HWRM to execute the request and forward the
 * response of the encapsulated request to the location specified in the
 * original request that is encapsulated. The target id of this command shall be
 * set to 0xFFFF (HWRM). The response location in this command shall be used to
 * acknowledge the receipt of the encapsulated request and forwarding of the
 * response.
 */

/* Input (128 bytes) */
struct hwrm_exec_fwd_resp_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * This is an encapsulated request. This request should be executed by
	 * the HWRM and the response should be provided in the response buffer
	 * inside the encapsulated request.
	 */
	uint32_t encap_request[26];

	/*
	 * This value indicates the target id of the response to the
	 * encapsulated request. 0x0 - 0xFFF8 - Used for function ids 0xFFF8 -
	 * 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t encap_resp_target_id;

	uint16_t unused_0[3];
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_exec_fwd_resp_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_func_qcaps */
/*
 * Description: This command returns capabilities of a function. The input FID
 * value is used to indicate what function is being queried. This allows a
 * physical function driver to query virtual functions that are children of the
 * physical function. The output FID value is needed to configure Rings and
 * MSI-X vectors so their DMA operations appear correctly on the PCI bus.
 */

/* Input (24 bytes) */
struct hwrm_func_qcaps_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * Function ID of the function that is being queried. 0xFF... (All Fs)
	 * if the query is for the requesting function.
	 */
	uint16_t fid;

	uint16_t unused_0[3];
} __attribute__((packed));

/* Output (80 bytes) */
struct hwrm_func_qcaps_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/*
	 * FID value. This value is used to identify operations on the PCI bus
	 * as belonging to a particular PCI function.
	 */
	uint16_t fid;

	/*
	 * Port ID of port that this function is associated with. Valid only for
	 * the PF. 0xFF... (All Fs) if this function is not associated with any
	 * port. 0xFF... (All Fs) if this function is called from a VF.
	 */
	uint16_t port_id;

	/* If 1, then Push mode is supported on this function. */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PUSH_MODE_SUPPORTED   UINT32_C(0x1)
	/*
	 * If 1, then the global MSI-X auto-masking is enabled for the device.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_GLOBAL_MSIX_AUTOMASKING \
								UINT32_C(0x2)
	/*
	 * If 1, then the Precision Time Protocol (PTP) processing is supported
	 * on this function. The HWRM should enable PTP on only a single
	 * Physical Function (PF) per port.
	 */
	#define HWRM_FUNC_QCAPS_OUTPUT_FLAGS_PTP_SUPPORTED         UINT32_C(0x4)
	uint32_t flags;

	/*
	 * This value is current MAC address configured for this function. A
	 * value of 00-00-00-00-00-00 indicates no MAC address is currently
	 * configured.
	 */
	uint8_t perm_mac_address[6];

	/*
	 * The maximum number of RSS/COS contexts that can be allocated to the
	 * function.
	 */
	uint16_t max_rsscos_ctx;

	/*
	 * The maximum number of completion rings that can be allocated to the
	 * function.
	 */
	uint16_t max_cmpl_rings;

	/*
	 * The maximum number of transmit rings that can be allocated to the
	 * function.
	 */
	uint16_t max_tx_rings;

	/*
	 * The maximum number of receive rings that can be allocated to the
	 * function.
	 */
	uint16_t max_rx_rings;

	/*
	 * The maximum number of L2 contexts that can be allocated to the
	 * function.
	 */
	uint16_t max_l2_ctxs;

	/* The maximum number of VNICs that can be allocated to the function. */
	uint16_t max_vnics;

	/*
	 * The identifier for the first VF enabled on a PF. This is valid only
	 * on the PF with SR-IOV enabled. 0xFF... (All Fs) if this command is
	 * called on a PF with SR-IOV disabled or on a VF.
	 */
	uint16_t first_vf_id;

	/*
	 * The maximum number of VFs that can be allocated to the function. This
	 * is valid only on the PF with SR-IOV enabled. 0xFF... (All Fs) if this
	 * command is called on a PF with SR-IOV disabled or on a VF.
	 */
	uint16_t max_vfs;

	/*
	 * The maximum number of statistic contexts that can be allocated to the
	 * function.
	 */
	uint16_t max_stat_ctx;

	/*
	 * The maximum number of Encapsulation records that can be offloaded by
	 * this function.
	 */
	uint32_t max_encap_records;

	/*
	 * The maximum number of decapsulation records that can be offloaded by
	 * this function.
	 */
	uint32_t max_decap_records;

	/*
	 * The maximum number of Exact Match (EM) flows that can be offloaded by
	 * this function on the TX side.
	 */
	uint32_t max_tx_em_flows;

	/*
	 * The maximum number of Wildcard Match (WM) flows that can be offloaded
	 * by this function on the TX side.
	 */
	uint32_t max_tx_wm_flows;

	/*
	 * The maximum number of Exact Match (EM) flows that can be offloaded by
	 * this function on the RX side.
	 */
	uint32_t max_rx_em_flows;

	/*
	 * The maximum number of Wildcard Match (WM) flows that can be offloaded
	 * by this function on the RX side.
	 */
	uint32_t max_rx_wm_flows;

	/*
	 * The maximum number of multicast filters that can be supported by this
	 * function on the RX side.
	 */
	uint32_t max_mcast_filters;

	/*
	 * The maximum value of flow_id that can be supported in completion
	 * records.
	 */
	uint32_t max_flow_id;

	/*
	 * The maximum number of HW ring groups that can be supported on this
	 * function.
	 */
	uint32_t max_hw_ring_grps;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_port_phy_cfg */
/*
 * Description: This command configures the PHY device for the port. It allows
 * setting of the most generic settings for the PHY. The HWRM shall complete
 * this command as soon as PHY settings are configured. They may not be applied
 * when the command response is provided. A VF driver shall not be allowed to
 * configure PHY using this command. In a network partition mode, a PF driver
 * shall not be allowed to configure PHY using this command.
 */

/* Input (56 bytes) */
struct hwrm_port_phy_cfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is set to '1', the PHY for the port shall be reset. #
	 * If this bit is set to 1, then the HWRM shall reset the PHY after
	 * applying PHY configuration changes specified in this command. # In
	 * order to guarantee that PHY configuration changes specified in this
	 * command take effect, the HWRM client should set this flag to 1. # If
	 * this bit is not set to 1, then the HWRM may reset the PHY depending
	 * on the current PHY configuration and settings specified in this
	 * command.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESET_PHY            UINT32_C(0x1)
	/*
	 * When this bit is set to '1', the link shall be forced to be taken
	 * down. # When this bit is set to '1", all other command input settings
	 * related to the link speed shall be ignored. Once the link state is
	 * forced down, it can be explicitly cleared from that state by setting
	 * this flag to '0'. # If this flag is set to '0', then the link shall
	 * be cleared from forced down state if the link is in forced down
	 * state. There may be conditions (e.g. out-of-band or sideband
	 * configuration changes for the link) outside the scope of the HWRM
	 * implementation that may clear forced down link state.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE_LINK_DOWN      UINT32_C(0x2)
	/*
	 * When this bit is set to '1', the link shall be forced to the
	 * force_link_speed value. When this bit is set to '1', the HWRM client
	 * should not enable any of the auto negotiation related fields
	 * represented by auto_XXX fields in this command. When this bit is set
	 * to '1' and the HWRM client has enabled a auto_XXX field in this
	 * command, then the HWRM shall ignore the enabled auto_XXX field. When
	 * this bit is set to zero, the link shall be allowed to autoneg.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_FORCE                UINT32_C(0x4)
	/*
	 * When this bit is set to '1', the auto-negotiation process shall be
	 * restarted on the link.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_RESTART_AUTONEG      UINT32_C(0x8)
	/*
	 * When this bit is set to '1', Energy Efficient Ethernet (EEE) is
	 * requested to be enabled on this link. If EEE is not supported on this
	 * port, then this flag shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_ENABLE	UINT32_C(0x10)
	/*
	 * When this bit is set to '1', Energy Efficient Ethernet (EEE) is
	 * requested to be disabled on this link. If EEE is not supported on
	 * this port, then this flag shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_DISABLE	UINT32_C(0x20)
	/*
	 * When this bit is set to '1' and EEE is enabled on this link, then TX
	 * LPI is requested to be enabled on the link. If EEE is not supported
	 * on this port, then this flag shall be ignored by the HWRM. If EEE is
	 * disabled on this port, then this flag shall be ignored by the HWRM.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FLAGS_EEE_TX_LPI	UINT32_C(0x40)
	uint32_t flags;

	/* This bit must be '1' for the auto_mode field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_MODE          UINT32_C(0x1)
	/* This bit must be '1' for the auto_duplex field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_DUPLEX        UINT32_C(0x2)
	/* This bit must be '1' for the auto_pause field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_PAUSE         UINT32_C(0x4)
	/*
	 * This bit must be '1' for the auto_link_speed field to be configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED    UINT32_C(0x8)
	/*
	 * This bit must be '1' for the auto_link_speed_mask field to be
	 * configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_AUTO_LINK_SPEED_MASK \
								UINT32_C(0x10)
	/* This bit must be '1' for the wirespeed field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_WIRESPEED	UINT32_C(0x20)
	/* This bit must be '1' for the lpbk field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_LPBK		UINT32_C(0x40)
	/* This bit must be '1' for the preemphasis field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_PREEMPHASIS	UINT32_C(0x80)
	/* This bit must be '1' for the force_pause field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_FORCE_PAUSE	UINT32_C(0x100)
	/*
	 * This bit must be '1' for the eee_link_speed_mask field to be
	 * configured.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_EEE_LINK_SPEED_MASK \
								UINT32_C(0x200)
	/* This bit must be '1' for the tx_lpi_timer field to be configured. */
	#define HWRM_PORT_PHY_CFG_INPUT_ENABLES_TX_LPI_TIMER	UINT32_C(0x400)
	uint32_t enables;

	/* Port ID of port that is to be configured. */
	uint16_t port_id;

	/*
	 * This is the speed that will be used if the force bit is '1'. If
	 * unsupported speed is selected, an error will be generated.
	 */
		/* 100Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100MB \
							(UINT32_C(0x1) << 0)
		/* 1Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_1GB \
							(UINT32_C(0xa) << 0)
		/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2GB \
							(UINT32_C(0x14) << 0)
		/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_2_5GB \
							(UINT32_C(0x19) << 0)
		/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10GB \
							(UINT32_C(0x64) << 0)
		/* 20Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_20GB \
							(UINT32_C(0xc8) << 0)
		/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_25GB \
							(UINT32_C(0xfa) << 0)
		/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_40GB \
							(UINT32_C(0x190) << 0)
		/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_50GB \
							(UINT32_C(0x1f4) << 0)
		/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_100GB \
							(UINT32_C(0x3e8) << 0)
		/* 10Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_LINK_SPEED_10MB \
							(UINT32_C(0xffff) << 0)
	uint16_t force_link_speed;

	/*
	 * This value is used to identify what autoneg mode is used when the
	 * link speed is not being forced.
	 */
		/*
		 * Disable autoneg or autoneg disabled. No speeds are selected.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_NONE	(UINT32_C(0x0) << 0)
		/* Select all possible speeds for autoneg mode. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ALL_SPEEDS \
							(UINT32_C(0x1) << 0)
		/*
		 * Select only the auto_link_speed speed for autoneg mode. This
		 * mode has been DEPRECATED. An HWRM client should not use this
		 * mode.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ONE_SPEED \
							(UINT32_C(0x2) << 0)
		/*
		 * Select the auto_link_speed or any speed below that speed for
		 * autoneg. This mode has been DEPRECATED. An HWRM client should
		 * not use this mode.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_ONE_OR_BELOW \
							(UINT32_C(0x3) << 0)
		/*
		 * Select the speeds based on the corresponding link speed mask
		 * value that is provided.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_MODE_SPEED_MASK \
							(UINT32_C(0x4) << 0)
	uint8_t auto_mode;

	/*
	 * This is the duplex setting that will be used if the autoneg_mode is
	 * "one_speed" or "one_or_below".
	 */
		/* Half Duplex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_HALF \
							(UINT32_C(0x0) << 0)
		/* Full duplex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_FULL \
							(UINT32_C(0x1) << 0)
		/* Both Half and Full dupex will be requested. */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_DUPLEX_BOTH \
							(UINT32_C(0x2) << 0)
	uint8_t auto_duplex;

	/*
	 * This value is used to configure the pause that will be used for
	 * autonegotiation. Add text on the usage of auto_pause and force_pause.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_TX              UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages has been
	 * requested. Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_RX              UINT32_C(0x2)
	/*
	 * When set to 1, the advertisement of pause is enabled. # When the
	 * auto_mode is not set to none and this flag is set to 1, then the
	 * auto_pause bits on this port are being advertised and autoneg pause
	 * results are being interpreted. # When the auto_mode is not set to
	 * none and this flag is set to 0, the pause is forced as indicated in
	 * force_pause, and also advertised as auto_pause bits, but the autoneg
	 * results are not interpreted since the pause configuration is being
	 * forced. # When the auto_mode is set to none and this flag is set to
	 * 1, auto_pause bits should be ignored and should be set to 0.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_PAUSE_AUTONEG_PAUSE   UINT32_C(0x4)
	uint8_t auto_pause;

	uint8_t unused_0;

	/*
	 * This is the speed that will be used if the autoneg_mode is
	 * "one_speed" or "one_or_below". If an unsupported speed is selected,
	 * an error will be generated.
	 */
		/* 100Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100MB \
							(UINT32_C(0x1) << 0)
		/* 1Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_1GB \
							(UINT32_C(0xa) << 0)
		/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2GB \
							(UINT32_C(0x14) << 0)
		/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_2_5GB \
							(UINT32_C(0x19) << 0)
		/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_10GB \
							(UINT32_C(0x64) << 0)
		/* 20Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_20GB \
							(UINT32_C(0xc8) << 0)
		/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_25GB \
							(UINT32_C(0xfa) << 0)
		/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_40GB \
							(UINT32_C(0x190) << 0)
		/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_50GB \
							(UINT32_C(0x1f4) << 0)
		/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_100GB \
							(UINT32_C(0x3e8) << 0)
		/* 10Mb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_10MB \
							(UINT32_C(0xffff) << 0)
	uint16_t auto_link_speed;

	/*
	 * This is a mask of link speeds that will be used if autoneg_mode is
	 * "mask". If unsupported speed is enabled an error will be generated.
	 */
	/* 100Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MBHD \
							UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100MB \
							UINT32_C(0x2)
	/* 1Gb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GBHD \
							UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_1GB \
							UINT32_C(0x8)
	/* 2Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2GB \
							UINT32_C(0x10)
	/* 2.5Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_2_5GB \
							UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10GB \
							UINT32_C(0x40)
	/* 20Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_20GB \
							UINT32_C(0x80)
	/* 25Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_25GB \
							UINT32_C(0x100)
	/* 40Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_40GB \
							UINT32_C(0x200)
	/* 50Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_50GB \
							UINT32_C(0x400)
	/* 100Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_100GB \
							UINT32_C(0x800)
	/* 10Mb link speed (Half-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10MBHD \
							UINT32_C(0x1000)
	/* 10Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_AUTO_LINK_SPEED_MASK_10MB \
							UINT32_C(0x2000)
	uint16_t auto_link_speed_mask;

	/* This value controls the wirespeed feature. */
		/* Wirespeed feature is disabled. */
	#define HWRM_PORT_PHY_CFG_INPUT_WIRESPEED_OFF	(UINT32_C(0x0) << 0)
		/* Wirespeed feature is enabled. */
	#define HWRM_PORT_PHY_CFG_INPUT_WIRESPEED_ON	(UINT32_C(0x1) << 0)
	uint8_t wirespeed;

	/* This value controls the loopback setting for the PHY. */
		/* No loopback is selected. Normal operation. */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_NONE	(UINT32_C(0x0) << 0)
		/*
		 * The HW will be configured with local loopback such that host
		 * data is sent back to the host without modification.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_LOCAL	(UINT32_C(0x1) << 0)
		/*
		 * The HW will be configured with remote loopback such that port
		 * logic will send packets back out the transmitter that are
		 * received.
		 */
	#define HWRM_PORT_PHY_CFG_INPUT_LPBK_REMOTE	(UINT32_C(0x2) << 0)
	uint8_t lpbk;

	/*
	 * This value is used to configure the pause that will be used for force
	 * mode.
	 */
	/*
	 * When this bit is '1', Generation of tx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_TX             UINT32_C(0x1)
	/*
	 * When this bit is '1', Reception of rx pause messages is supported.
	 * Disabled otherwise.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_FORCE_PAUSE_RX             UINT32_C(0x2)
	uint8_t force_pause;

	uint8_t unused_1;

	/*
	 * This value controls the pre-emphasis to be used for the link. Driver
	 * should not set this value (use enable.preemphasis = 0) unless driver
	 * is sure of setting. Normally HWRM FW will determine proper pre-
	 * emphasis.
	 */
	uint32_t preemphasis;

	/*
	 * Setting for link speed mask that is used to advertise speeds during
	 * autonegotiation when EEE is enabled. This field is valid only when
	 * EEE is enabled. The speeds specified in this field shall be a subset
	 * of speeds specified in auto_link_speed_mask. If EEE is enabled,then
	 * at least one speed shall be provided in this mask.
	 */
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD1  UINT32_C(0x1)
	/* 100Mb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_100MB  UINT32_C(0x2)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD2  UINT32_C(0x4)
	/* 1Gb link speed (Full-duplex) */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_1GB    UINT32_C(0x8)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD3 \
								UINT32_C(0x10)
	/* Reserved */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_RSVD4 \
								UINT32_C(0x20)
	/* 10Gb link speed */
	#define HWRM_PORT_PHY_CFG_INPUT_EEE_LINK_SPEED_MASK_10GB \
								UINT32_C(0x40)
	uint16_t eee_link_speed_mask;

	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * Reuested setting of TX LPI timer in microseconds. This field is valid
	 * only when EEE is enabled and TX LPI is enabled.
	 */
	#define HWRM_PORT_PHY_CFG_INPUT_TX_LPI_TIMER_MASK \
							UINT32_C(0xffffff)
	#define HWRM_PORT_PHY_CFG_INPUT_TX_LPI_TIMER_SFT           0
	uint32_t tx_lpi_timer;

	uint32_t unused_4;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_port_phy_cfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_ver_get */
/*
 * Description: This function is called by a driver to determine the HWRM
 * interface version supported by the HWRM firmware, the version of HWRM
 * firmware implementation, the name of HWRM firmware, the versions of other
 * embedded firmwares, and the names of other embedded firmwares, etc. Any
 * interface or firmware version with major = 0, minor = 0, and update = 0 shall
 * be considered an invalid version.
 */

/* Input (24 bytes) */
struct hwrm_ver_get_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * This field represents the major version of HWRM interface
	 * specification supported by the driver HWRM implementation. The
	 * interface major version is intended to change only when non backward
	 * compatible changes are made to the HWRM interface specification.
	 */
	uint8_t hwrm_intf_maj;

	/*
	 * This field represents the minor version of HWRM interface
	 * specification supported by the driver HWRM implementation. A change
	 * in interface minor version is used to reflect significant backward
	 * compatible modification to HWRM interface specification. This can be
	 * due to addition or removal of functionality. HWRM interface
	 * specifications with the same major version but different minor
	 * versions are compatible.
	 */
	uint8_t hwrm_intf_min;

	/*
	 * This field represents the update version of HWRM interface
	 * specification supported by the driver HWRM implementation. The
	 * interface update version is used to reflect minor changes or bug
	 * fixes to a released HWRM interface specification.
	 */
	uint8_t hwrm_intf_upd;

	uint8_t unused_0[5];
} __attribute__((packed));

/* Output (128 bytes) */
struct hwrm_ver_get_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/*
	 * This field represents the major version of HWRM interface
	 * specification supported by the HWRM implementation. The interface
	 * major version is intended to change only when non backward compatible
	 * changes are made to the HWRM interface specification. A HWRM
	 * implementation that is compliant with this specification shall
	 * provide value of 1 in this field.
	 */
	uint8_t hwrm_intf_maj;

	/*
	 * This field represents the minor version of HWRM interface
	 * specification supported by the HWRM implementation. A change in
	 * interface minor version is used to reflect significant backward
	 * compatible modification to HWRM interface specification. This can be
	 * due to addition or removal of functionality. HWRM interface
	 * specifications with the same major version but different minor
	 * versions are compatible. A HWRM implementation that is compliant with
	 * this specification shall provide value of 0 in this field.
	 */
	uint8_t hwrm_intf_min;

	/*
	 * This field represents the update version of HWRM interface
	 * specification supported by the HWRM implementation. The interface
	 * update version is used to reflect minor changes or bug fixes to a
	 * released HWRM interface specification. A HWRM implementation that is
	 * compliant with this specification shall provide value of 1 in this
	 * field.
	 */
	uint8_t hwrm_intf_upd;

	uint8_t hwrm_intf_rsvd;

	/*
	 * This field represents the major version of HWRM firmware. A change in
	 * firmware major version represents a major firmware release.
	 */
	uint8_t hwrm_fw_maj;

	/*
	 * This field represents the minor version of HWRM firmware. A change in
	 * firmware minor version represents significant firmware functionality
	 * changes.
	 */
	uint8_t hwrm_fw_min;

	/*
	 * This field represents the build version of HWRM firmware. A change in
	 * firmware build version represents bug fixes to a released firmware.
	 */
	uint8_t hwrm_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version of the HWRM firmware.
	 */
	uint8_t hwrm_fw_rsvd;

	/*
	 * This field represents the major version of mgmt firmware. A change in
	 * major version represents a major release.
	 */
	uint8_t mgmt_fw_maj;

	/*
	 * This field represents the minor version of mgmt firmware. A change in
	 * minor version represents significant functionality changes.
	 */
	uint8_t mgmt_fw_min;

	/*
	 * This field represents the build version of mgmt firmware. A change in
	 * update version represents bug fixes.
	 */
	uint8_t mgmt_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version
	 */
	uint8_t mgmt_fw_rsvd;

	/*
	 * This field represents the major version of network control firmware.
	 * A change in major version represents a major release.
	 */
	uint8_t netctrl_fw_maj;

	/*
	 * This field represents the minor version of network control firmware.
	 * A change in minor version represents significant functionality
	 * changes.
	 */
	uint8_t netctrl_fw_min;

	/*
	 * This field represents the build version of network control firmware.
	 * A change in update version represents bug fixes.
	 */
	uint8_t netctrl_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version
	 */
	uint8_t netctrl_fw_rsvd;

	/*
	 * This field is reserved for future use. The responder should set it to
	 * 0. The requester should ignore this field.
	 */
	uint32_t reserved1;

	/*
	 * This field represents the major version of RoCE firmware. A change in
	 * major version represents a major release.
	 */
	uint8_t roce_fw_maj;

	/*
	 * This field represents the minor version of RoCE firmware. A change in
	 * minor version represents significant functionality changes.
	 */
	uint8_t roce_fw_min;

	/*
	 * This field represents the build version of RoCE firmware. A change in
	 * update version represents bug fixes.
	 */
	uint8_t roce_fw_bld;

	/*
	 * This field is a reserved field. This field can be used to represent
	 * firmware branches or customer specific releases tied to a specific
	 * (major,minor,update) version
	 */
	uint8_t roce_fw_rsvd;

	/*
	 * This field represents the name of HWRM FW (ASCII chars without NULL
	 * at the end).
	 */
	char hwrm_fw_name[16];

	/*
	 * This field represents the name of mgmt FW (ASCII chars without NULL
	 * at the end).
	 */
	char mgmt_fw_name[16];

	/*
	 * This field represents the name of network control firmware (ASCII
	 * chars without NULL at the end).
	 */
	char netctrl_fw_name[16];

	/*
	 * This field is reserved for future use. The responder should set it to
	 * 0. The requester should ignore this field.
	 */
	uint32_t reserved2[4];

	/*
	 * This field represents the name of RoCE FW (ASCII chars without NULL
	 * at the end).
	 */
	char roce_fw_name[16];

	/* This field returns the chip number. */
	uint16_t chip_num;

	/* This field returns the revision of chip. */
	uint8_t chip_rev;

	/* This field returns the chip metal number. */
	uint8_t chip_metal;

	/* This field returns the bond id of the chip. */
	uint8_t chip_bond_id;

	/*
	 * This value indicates the type of platform used for chip
	 * implementation.
	 */
	/* ASIC */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_ASIC \
							(UINT32_C(0x0) << 0)
	/* FPGA platform of the chip. */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_FPGA \
							(UINT32_C(0x1) << 0)
	/* Palladium platform of the chip. */
	#define HWRM_VER_GET_OUTPUT_CHIP_PLATFORM_TYPE_PALLADIUM \
							(UINT32_C(0x2) << 0)
	uint8_t chip_platform_type;

	/*
	 * This field returns the maximum value of request window that is
	 * supported by the HWRM. The request window is mapped into device
	 * address space using MMIO.
	 */
	uint16_t max_req_win_len;

	/*
	 * This field returns the maximum value of response buffer in bytes. If
	 * a request specifies the response buffer length that is greater than
	 * this value, then the HWRM should fail it. The value of this field
	 * shall be 4KB or more.
	 */
	uint16_t max_resp_len;

	/*
	 * This field returns the default request timeout value in milliseconds.
	 */
	uint16_t def_req_timeout;

	uint8_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_queue_qportcfg */
/*
 * Description: This function is called by a driver to query queue configuration
 * of a port. # The HWRM shall at least advertise one queue with lossy service
 * profile. # The driver shall use this command to query queue ids before
 * configuring or using any queues. # If a service profile is not set for a
 * queue, then the driver shall not use that queue without configuring a service
 * profile for it. # If the driver is not allowed to configure service profiles,
 * then the driver shall only use queues for which service profiles are pre-
 * configured.
 */

/* Input (24 bytes) */
struct hwrm_queue_qportcfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * Enumeration denoting the RX, TX type of the resource. This
	 * enumeration is used for resources that are similar for both TX and RX
	 * paths of the chip.
	 */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH \
							UINT32_C(0x1)
		/* tx path */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_TX \
							(UINT32_C(0x0) << 0)
		/* rx path */
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX \
							(UINT32_C(0x1) << 0)
	#define HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_LAST \
					HWRM_QUEUE_QPORTCFG_INPUT_FLAGS_PATH_RX
	uint32_t flags;

	/*
	 * Port ID of port for which the queue configuration is being queried.
	 * This field is only required when sent by IPC.
	 */
	uint16_t port_id;

	uint16_t unused_0;
} __attribute__((packed));

/* hwrm_stat_ctx_clr_stats */
/* Description: This command clears statistics of a context. */

/* Input (24 bytes) */
struct hwrm_stat_ctx_clr_stats_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/* ID of the statistics context that is being queried. */
	uint32_t stat_ctx_id;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_stat_ctx_clr_stats_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_vnic_rss_cfg */
/* Description: This function is used to enable RSS configuration. */

/* Input (48 bytes) */
struct hwrm_vnic_rss_cfg_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', the RSS hash shall be computed over source and
	 * destination IPv4 addresses of IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV4		UINT32_C(0x1)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv4 addresses and source/destination ports of
	 * TCP/IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV4	UINT32_C(0x2)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv4 addresses and source/destination ports of
	 * UDP/IPv4 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV4	UINT32_C(0x4)
	/*
	 * When this bit is '1', the RSS hash shall be computed over source and
	 * destination IPv4 addresses of IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_IPV6		UINT32_C(0x8)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv6 addresses and source/destination ports of
	 * TCP/IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_TCP_IPV6	UINT32_C(0x10)
	/*
	 * When this bit is '1', the RSS hash shall be computed over
	 * source/destination IPv6 addresses and source/destination ports of
	 * UDP/IPv6 packets.
	 */
	#define HWRM_VNIC_RSS_CFG_INPUT_HASH_TYPE_UDP_IPV6	UINT32_C(0x20)
	uint32_t hash_type;

	uint32_t unused_0;

	/* This is the address for rss ring group table */
	uint64_t ring_grp_tbl_addr;

	/* This is the address for rss hash key table */
	uint64_t hash_key_tbl_addr;

	/* Index to the rss indirection table. */
	uint16_t rss_ctx_idx;

	uint16_t unused_1[3];
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_vnic_rss_cfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* Output (32 bytes) */
struct hwrm_queue_qportcfg_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	/* The maximum number of queues that can be configured. */
	uint8_t max_configurable_queues;

	/* The maximum number of lossless queues that can be configured. */
	uint8_t max_configurable_lossless_queues;

	/*
	 * 0 - Not allowed. Non-zero - Allowed. If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure queues using
	 * hwrm_queue_cfg.
	 */
	uint8_t queue_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure queue buffers
	 * using hwrm_queue_buffers_cfg.
	 */
	uint8_t queue_buffers_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure PFC using
	 * hwrm_queue_pfcenable_cfg.
	 */
	uint8_t queue_pfcenable_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure Priority to CoS
	 * mapping using hwrm_queue_pri2cos_cfg.
	 */
	uint8_t queue_pri2cos_cfg_allowed;

	/*
	 * 0 - Not allowed. Non-zero - Allowed If this value is non-zero, then
	 * the HWRM shall allow the host SW driver to configure CoS Bandwidth
	 * configuration using hwrm_queue_cos2bw_cfg.
	 */
	uint8_t queue_cos2bw_cfg_allowed;

	/* ID of CoS Queue 0. FF - Invalid id */
	uint8_t queue_id0;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID0_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id0_service_profile;

	/* ID of CoS Queue 1. FF - Invalid id */
	uint8_t queue_id1;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID1_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id1_service_profile;

	/* ID of CoS Queue 2. FF - Invalid id */
	uint8_t queue_id2;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID2_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id2_service_profile;

	/* ID of CoS Queue 3. FF - Invalid id */
	uint8_t queue_id3;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID3_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id3_service_profile;

	/* ID of CoS Queue 4. FF - Invalid id */
	uint8_t queue_id4;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID4_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id4_service_profile;

	/* ID of CoS Queue 5. FF - Invalid id */
	uint8_t queue_id5;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID5_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id5_service_profile;

	/* ID of CoS Queue 6. FF - Invalid id */
	uint8_t queue_id6_service_profile;
	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID6_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id6;

	/* ID of CoS Queue 7. FF - Invalid id */
	uint8_t queue_id7;

	/* This value is applicable to CoS queues only. */
		/* Lossy (best-effort) */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_LOSSY \
							(UINT32_C(0x0) << 0)
		/* Lossless */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_LOSSLESS \
							(UINT32_C(0x1) << 0)
		/*
		 * Set to 0xFF... (All Fs) if there is no service profile
		 * specified
		 */
	#define HWRM_QUEUE_QPORTCFG_OUTPUT_QUEUE_ID7_SERVICE_PROFILE_UNKNOWN \
							(UINT32_C(0xff) << 0)
	uint8_t queue_id7_service_profile;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_func_drv_rgtr */
/*
 * Description: This command is used by the function driver to register its
 * information with the HWRM. A function driver shall implement this command. A
 * function driver shall use this command during the driver initialization right
 * after the HWRM version discovery and default ring resources allocation.
 */

/* Input (80 bytes) */
struct hwrm_func_drv_rgtr_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', the function driver is requesting all requests
	 * from its children VF drivers to be forwarded to itself. This flag can
	 * only be set by the PF driver. If a VF driver sets this flag, it
	 * should be ignored by the HWRM.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_FWD_ALL_MODE        UINT32_C(0x1)
	/*
	 * When this bit is '1', the function is requesting none of the requests
	 * from its children VF drivers to be forwarded to itself. This flag can
	 * only be set by the PF driver. If a VF driver sets this flag, it
	 * should be ignored by the HWRM.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_FLAGS_FWD_NONE_MODE       UINT32_C(0x2)
	uint32_t flags;

	/* This bit must be '1' for the os_type field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_OS_TYPE           UINT32_C(0x1)
	/* This bit must be '1' for the ver field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VER               UINT32_C(0x2)
	/* This bit must be '1' for the timestamp field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_TIMESTAMP         UINT32_C(0x4)
	/* This bit must be '1' for the vf_req_fwd field to be configured. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_VF_REQ_FWD        UINT32_C(0x8)
	/*
	 * This bit must be '1' for the async_event_fwd field to be configured.
	 */
	#define HWRM_FUNC_DRV_RGTR_INPUT_ENABLES_ASYNC_EVENT_FWD \
								UINT32_C(0x10)
	uint32_t enables;

	/* This value indicates the type of OS. */
		/* Unknown */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_UNKNOWN \
							(UINT32_C(0x0) << 0)
		/* Other OS not listed below. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_OTHER \
							(UINT32_C(0x1) << 0)
		/* MSDOS OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_MSDOS \
							(UINT32_C(0xe) << 0)
		/* Windows OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WINDOWS \
							(UINT32_C(0x12) << 0)
		/* Solaris OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_SOLARIS \
							(UINT32_C(0x1d) << 0)
		/* Linux OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_LINUX \
							(UINT32_C(0x24) << 0)
		/* FreeBSD OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_FREEBSD \
							(UINT32_C(0x2a) << 0)
		/* VMware ESXi OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_ESXI \
							(UINT32_C(0x68) << 0)
		/* Microsoft Windows 8 64-bit OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WIN864 \
							(UINT32_C(0x73) << 0)
		/* Microsoft Windows Server 2012 R2 OS. */
	#define HWRM_FUNC_DRV_RGTR_INPUT_OS_TYPE_WIN2012R2 \
							(UINT32_C(0x74) << 0)
	uint16_t os_type;

	/* This is the major version of the driver. */
	uint8_t ver_maj;

	/* This is the minor version of the driver. */
	uint8_t ver_min;

	/* This is the update version of the driver. */
	uint8_t ver_upd;

	uint8_t unused_0;
	uint16_t unused_1;

	/*
	 * This is a 32-bit timestamp provided by the driver for keep alive. The
	 * timestamp is in multiples of 1ms.
	 */
	uint32_t timestamp;

	uint32_t unused_2;

	/*
	 * This is a 256-bit bit mask provided by the PF driver for letting the
	 * HWRM know what commands issued by the VF driver to the HWRM should be
	 * forwarded to the PF driver. Nth bit refers to the Nth req_type.
	 * Setting Nth bit to 1 indicates that requests from the VF driver with
	 * req_type equal to N shall be forwarded to the parent PF driver. This
	 * field is not valid for the VF driver.
	 */
	uint32_t vf_req_fwd[8];

	/*
	 * This is a 256-bit bit mask provided by the function driver (PF or VF
	 * driver) to indicate the list of asynchronous event completions to be
	 * forwarded. Nth bit refers to the Nth event_id. Setting Nth bit to 1
	 * by the function driver shall result in the HWRM forwarding
	 * asynchronous event completion with event_id equal to N. If all bits
	 * are set to 0 (value of 0), then the HWRM shall not forward any
	 * asynchronous event completion to this function driver.
	 */
	uint32_t async_event_fwd[8];
} __attribute__((packed));

/* Output (16 bytes) */

struct hwrm_func_drv_rgtr_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

/* hwrm_func_drv_unrgtr */
/*
 * Description: This command is used by the function driver to un register with
 * the HWRM. A function driver shall implement this command. A function driver
 * shall use this command during the driver unloading.
 */
/* Input (24 bytes) */

struct hwrm_func_drv_unrgtr_input {
	/*
	 * This value indicates what type of request this is. The format for the
	 * rest of the command is determined by this field.
	 */
	uint16_t req_type;

	/*
	 * This value indicates the what completion ring the request will be
	 * optionally completed on. If the value is -1, then no CR completion
	 * will be generated. Any other value must be a valid CR ring_id value
	 * for this function.
	 */
	uint16_t cmpl_ring;

	/* This value indicates the command sequence number. */
	uint16_t seq_id;

	/*
	 * Target ID of this command. 0x0 - 0xFFF8 - Used for function ids
	 * 0xFFF8 - 0xFFFE - Reserved for internal processors 0xFFFF - HWRM
	 */
	uint16_t target_id;

	/*
	 * This is the host address where the response will be written when the
	 * request is complete. This area must be 16B aligned and must be
	 * cleared to zero before the request is made.
	 */
	uint64_t resp_addr;

	/*
	 * When this bit is '1', the function driver is notifying the HWRM to
	 * prepare for the shutdown.
	 */
	#define HWRM_FUNC_DRV_UNRGTR_INPUT_FLAGS_PREPARE_FOR_SHUTDOWN \
							UINT32_C(0x1)
	uint32_t flags;

	uint32_t unused_0;
} __attribute__((packed));

/* Output (16 bytes) */
struct hwrm_func_drv_unrgtr_output {
	/*
	 * Pass/Fail or error type Note: receiver to verify the in parameters,
	 * and fail the call with an error when appropriate
	 */
	uint16_t error_code;

	/* This field returns the type of original request. */
	uint16_t req_type;

	/* This field provides original sequence number of the command. */
	uint16_t seq_id;

	/*
	 * This field is the length of the response in bytes. The last byte of
	 * the response is a valid flag that will read as '1' when the command
	 * has been completely written to memory.
	 */
	uint16_t resp_len;

	uint32_t unused_0;
	uint8_t unused_1;
	uint8_t unused_2;
	uint8_t unused_3;

	/*
	 * This field is used in Output records to indicate that the output is
	 * completely written to RAM. This field should be read as '1' to
	 * indicate that the output has been completely written. When writing a
	 * command completion or response to an internal processor, the order of
	 * writes has to be such that this field is written last.
	 */
	uint8_t valid;
} __attribute__((packed));

#endif
