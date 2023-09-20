/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _VIRTCHNL2_H_
#define _VIRTCHNL2_H_

/* All opcodes associated with virtchnl 2 are prefixed with virtchnl2 or
 * VIRTCHNL2. Any future opcodes, offloads/capabilities, structures,
 * and defines must be prefixed with virtchnl2 or VIRTCHNL2 to avoid confusion.
 */

#include "virtchnl2_lan_desc.h"

/* VIRTCHNL2_ERROR_CODES */
/* success */
#define	VIRTCHNL2_STATUS_SUCCESS	0
/* Operation not permitted, used in case of command not permitted for sender */
#define	VIRTCHNL2_STATUS_ERR_EPERM	1
/* Bad opcode - virtchnl interface problem */
#define	VIRTCHNL2_STATUS_ERR_ESRCH	3
/* I/O error - HW access error */
#define	VIRTCHNL2_STATUS_ERR_EIO	5
/* No such resource - Referenced resource is not allacated */
#define	VIRTCHNL2_STATUS_ERR_ENXIO	6
/* Permission denied - Resource is not permitted to caller */
#define	VIRTCHNL2_STATUS_ERR_EACCES	13
/* Device or resource busy - In case shared resource is in use by others */
#define	VIRTCHNL2_STATUS_ERR_EBUSY	16
/* Object already exists and not free */
#define	VIRTCHNL2_STATUS_ERR_EEXIST	17
/* Invalid input argument in command */
#define	VIRTCHNL2_STATUS_ERR_EINVAL	22
/* No space left or allocation failure */
#define	VIRTCHNL2_STATUS_ERR_ENOSPC	28
/* Parameter out of range */
#define	VIRTCHNL2_STATUS_ERR_ERANGE	34

/* Op not allowed in current dev mode */
#define	VIRTCHNL2_STATUS_ERR_EMODE	200
/* State Machine error - Command sequence problem */
#define	VIRTCHNL2_STATUS_ERR_ESM	201

/* These macros are used to generate compilation errors if a structure/union
 * is not exactly the correct length. It gives a divide by zero error if the
 * structure/union is not of the correct size, otherwise it creates an enum
 * that is never used.
 */
#define VIRTCHNL2_CHECK_STRUCT_LEN(n, X) enum virtchnl2_static_assert_enum_##X \
	{ virtchnl2_static_assert_##X = (n)/((sizeof(struct X) == (n)) ? 1 : 0) }
#define VIRTCHNL2_CHECK_UNION_LEN(n, X) enum virtchnl2_static_asset_enum_##X \
	{ virtchnl2_static_assert_##X = (n)/((sizeof(union X) == (n)) ? 1 : 0) }

/* New major set of opcodes introduced and so leaving room for
 * old misc opcodes to be added in future. Also these opcodes may only
 * be used if both the PF and VF have successfully negotiated the
 * VIRTCHNL version as 2.0 during VIRTCHNL22_OP_VERSION exchange.
 */
#define		VIRTCHNL2_OP_UNKNOWN			0
#define		VIRTCHNL2_OP_VERSION			1
#define		VIRTCHNL2_OP_GET_CAPS			500
#define		VIRTCHNL2_OP_CREATE_VPORT		501
#define		VIRTCHNL2_OP_DESTROY_VPORT		502
#define		VIRTCHNL2_OP_ENABLE_VPORT		503
#define		VIRTCHNL2_OP_DISABLE_VPORT		504
#define		VIRTCHNL2_OP_CONFIG_TX_QUEUES		505
#define		VIRTCHNL2_OP_CONFIG_RX_QUEUES		506
#define		VIRTCHNL2_OP_ENABLE_QUEUES		507
#define		VIRTCHNL2_OP_DISABLE_QUEUES		508
#define		VIRTCHNL2_OP_ADD_QUEUES			509
#define		VIRTCHNL2_OP_DEL_QUEUES			510
#define		VIRTCHNL2_OP_MAP_QUEUE_VECTOR		511
#define		VIRTCHNL2_OP_UNMAP_QUEUE_VECTOR		512
#define		VIRTCHNL2_OP_GET_RSS_KEY		513
#define		VIRTCHNL2_OP_SET_RSS_KEY		514
#define		VIRTCHNL2_OP_GET_RSS_LUT		515
#define		VIRTCHNL2_OP_SET_RSS_LUT		516
#define		VIRTCHNL2_OP_GET_RSS_HASH		517
#define		VIRTCHNL2_OP_SET_RSS_HASH		518
#define		VIRTCHNL2_OP_SET_SRIOV_VFS		519
#define		VIRTCHNL2_OP_ALLOC_VECTORS		520
#define		VIRTCHNL2_OP_DEALLOC_VECTORS		521
#define		VIRTCHNL2_OP_EVENT			522
#define		VIRTCHNL2_OP_GET_STATS			523
#define		VIRTCHNL2_OP_RESET_VF			524
	/* opcode 525 is reserved */
#define		VIRTCHNL2_OP_GET_PTYPE_INFO		526
	/* opcode 527 and 528 are reserved for VIRTCHNL2_OP_GET_PTYPE_ID and
	 * VIRTCHNL2_OP_GET_PTYPE_INFO_RAW
	 */
	/* opcodes 529, 530, and 531 are reserved */
#define		VIRTCHNL2_OP_NON_FLEX_CREATE_ADI	532
#define		VIRTCHNL2_OP_NON_FLEX_DESTROY_ADI	533
#define		VIRTCHNL2_OP_LOOPBACK			534
#define		VIRTCHNL2_OP_ADD_MAC_ADDR		535
#define		VIRTCHNL2_OP_DEL_MAC_ADDR		536
#define		VIRTCHNL2_OP_CONFIG_PROMISCUOUS_MODE	537
#define		VIRTCHNL2_OP_ADD_QUEUE_GROUPS		538
#define		VIRTCHNL2_OP_DEL_QUEUE_GROUPS		539
#define		VIRTCHNL2_OP_GET_PORT_STATS		540
	/* TimeSync opcodes */
#define		VIRTCHNL2_OP_GET_PTP_CAPS		541
#define		VIRTCHNL2_OP_GET_PTP_TX_TSTAMP_LATCHES	542

#define VIRTCHNL2_RDMA_INVALID_QUEUE_IDX	0xFFFF

/* VIRTCHNL2_VPORT_TYPE
 * Type of virtual port
 */
#define VIRTCHNL2_VPORT_TYPE_DEFAULT		0
#define VIRTCHNL2_VPORT_TYPE_SRIOV		1
#define VIRTCHNL2_VPORT_TYPE_SIOV		2
#define VIRTCHNL2_VPORT_TYPE_SUBDEV		3
#define VIRTCHNL2_VPORT_TYPE_MNG		4

/* VIRTCHNL2_QUEUE_MODEL
 * Type of queue model
 *
 * In the single queue model, the same transmit descriptor queue is used by
 * software to post descriptors to hardware and by hardware to post completed
 * descriptors to software.
 * Likewise, the same receive descriptor queue is used by hardware to post
 * completions to software and by software to post buffers to hardware.
 */
#define VIRTCHNL2_QUEUE_MODEL_SINGLE		0
/* In the split queue model, hardware uses transmit completion queues to post
 * descriptor/buffer completions to software, while software uses transmit
 * descriptor queues to post descriptors to hardware.
 * Likewise, hardware posts descriptor completions to the receive descriptor
 * queue, while software uses receive buffer queues to post buffers to hardware.
 */
#define VIRTCHNL2_QUEUE_MODEL_SPLIT		1

/* VIRTCHNL2_CHECKSUM_OFFLOAD_CAPS
 * Checksum offload capability flags
 */
#define VIRTCHNL2_CAP_TX_CSUM_L3_IPV4		BIT(0)
#define VIRTCHNL2_CAP_TX_CSUM_L4_IPV4_TCP	BIT(1)
#define VIRTCHNL2_CAP_TX_CSUM_L4_IPV4_UDP	BIT(2)
#define VIRTCHNL2_CAP_TX_CSUM_L4_IPV4_SCTP	BIT(3)
#define VIRTCHNL2_CAP_TX_CSUM_L4_IPV6_TCP	BIT(4)
#define VIRTCHNL2_CAP_TX_CSUM_L4_IPV6_UDP	BIT(5)
#define VIRTCHNL2_CAP_TX_CSUM_L4_IPV6_SCTP	BIT(6)
#define VIRTCHNL2_CAP_TX_CSUM_GENERIC		BIT(7)
#define VIRTCHNL2_CAP_RX_CSUM_L3_IPV4		BIT(8)
#define VIRTCHNL2_CAP_RX_CSUM_L4_IPV4_TCP	BIT(9)
#define VIRTCHNL2_CAP_RX_CSUM_L4_IPV4_UDP	BIT(10)
#define VIRTCHNL2_CAP_RX_CSUM_L4_IPV4_SCTP	BIT(11)
#define VIRTCHNL2_CAP_RX_CSUM_L4_IPV6_TCP	BIT(12)
#define VIRTCHNL2_CAP_RX_CSUM_L4_IPV6_UDP	BIT(13)
#define VIRTCHNL2_CAP_RX_CSUM_L4_IPV6_SCTP	BIT(14)
#define VIRTCHNL2_CAP_RX_CSUM_GENERIC		BIT(15)
#define VIRTCHNL2_CAP_TX_CSUM_L3_SINGLE_TUNNEL	BIT(16)
#define VIRTCHNL2_CAP_TX_CSUM_L3_DOUBLE_TUNNEL	BIT(17)
#define VIRTCHNL2_CAP_RX_CSUM_L3_SINGLE_TUNNEL	BIT(18)
#define VIRTCHNL2_CAP_RX_CSUM_L3_DOUBLE_TUNNEL	BIT(19)
#define VIRTCHNL2_CAP_TX_CSUM_L4_SINGLE_TUNNEL	BIT(20)
#define VIRTCHNL2_CAP_TX_CSUM_L4_DOUBLE_TUNNEL	BIT(21)
#define VIRTCHNL2_CAP_RX_CSUM_L4_SINGLE_TUNNEL	BIT(22)
#define VIRTCHNL2_CAP_RX_CSUM_L4_DOUBLE_TUNNEL	BIT(23)

/* VIRTCHNL2_SEGMENTATION_OFFLOAD_CAPS
 * Segmentation offload capability flags
 */
#define VIRTCHNL2_CAP_SEG_IPV4_TCP		BIT(0)
#define VIRTCHNL2_CAP_SEG_IPV4_UDP		BIT(1)
#define VIRTCHNL2_CAP_SEG_IPV4_SCTP		BIT(2)
#define VIRTCHNL2_CAP_SEG_IPV6_TCP		BIT(3)
#define VIRTCHNL2_CAP_SEG_IPV6_UDP		BIT(4)
#define VIRTCHNL2_CAP_SEG_IPV6_SCTP		BIT(5)
#define VIRTCHNL2_CAP_SEG_GENERIC		BIT(6)
#define VIRTCHNL2_CAP_SEG_TX_SINGLE_TUNNEL	BIT(7)
#define VIRTCHNL2_CAP_SEG_TX_DOUBLE_TUNNEL	BIT(8)

/* VIRTCHNL2_RSS_FLOW_TYPE_CAPS
 * Receive Side Scaling Flow type capability flags
 */
#define VIRTCHNL2_CAP_RSS_IPV4_TCP		BIT(0)
#define VIRTCHNL2_CAP_RSS_IPV4_UDP		BIT(1)
#define VIRTCHNL2_CAP_RSS_IPV4_SCTP		BIT(2)
#define VIRTCHNL2_CAP_RSS_IPV4_OTHER		BIT(3)
#define VIRTCHNL2_CAP_RSS_IPV6_TCP		BIT(4)
#define VIRTCHNL2_CAP_RSS_IPV6_UDP		BIT(5)
#define VIRTCHNL2_CAP_RSS_IPV6_SCTP		BIT(6)
#define VIRTCHNL2_CAP_RSS_IPV6_OTHER		BIT(7)
#define VIRTCHNL2_CAP_RSS_IPV4_AH		BIT(8)
#define VIRTCHNL2_CAP_RSS_IPV4_ESP		BIT(9)
#define VIRTCHNL2_CAP_RSS_IPV4_AH_ESP		BIT(10)
#define VIRTCHNL2_CAP_RSS_IPV6_AH		BIT(11)
#define VIRTCHNL2_CAP_RSS_IPV6_ESP		BIT(12)
#define VIRTCHNL2_CAP_RSS_IPV6_AH_ESP		BIT(13)

/* VIRTCHNL2_HEADER_SPLIT_CAPS
 * Header split capability flags
 */
/* for prepended metadata  */
#define VIRTCHNL2_CAP_RX_HSPLIT_AT_L2		BIT(0)
/* all VLANs go into header buffer */
#define VIRTCHNL2_CAP_RX_HSPLIT_AT_L3		BIT(1)
#define VIRTCHNL2_CAP_RX_HSPLIT_AT_L4V4		BIT(2)
#define VIRTCHNL2_CAP_RX_HSPLIT_AT_L4V6		BIT(3)

/* VIRTCHNL2_RSC_OFFLOAD_CAPS
 * Receive Side Coalescing offload capability flags
 */
#define VIRTCHNL2_CAP_RSC_IPV4_TCP		BIT(0)
#define VIRTCHNL2_CAP_RSC_IPV4_SCTP		BIT(1)
#define VIRTCHNL2_CAP_RSC_IPV6_TCP		BIT(2)
#define VIRTCHNL2_CAP_RSC_IPV6_SCTP		BIT(3)

/* VIRTCHNL2_OTHER_CAPS
 * Other capability flags
 * SPLITQ_QSCHED: Queue based scheduling using split queue model
 * TX_VLAN: VLAN tag insertion
 * RX_VLAN: VLAN tag stripping
 */
#define VIRTCHNL2_CAP_RDMA			BIT(0)
#define VIRTCHNL2_CAP_SRIOV			BIT(1)
#define VIRTCHNL2_CAP_MACFILTER			BIT(2)
#define VIRTCHNL2_CAP_FLOW_DIRECTOR		BIT(3)
#define VIRTCHNL2_CAP_SPLITQ_QSCHED		BIT(4)
#define VIRTCHNL2_CAP_CRC			BIT(5)
#define VIRTCHNL2_CAP_ADQ			BIT(6)
#define VIRTCHNL2_CAP_WB_ON_ITR			BIT(7)
#define VIRTCHNL2_CAP_PROMISC			BIT(8)
#define VIRTCHNL2_CAP_LINK_SPEED		BIT(9)
#define VIRTCHNL2_CAP_INLINE_IPSEC		BIT(10)
#define VIRTCHNL2_CAP_LARGE_NUM_QUEUES		BIT(11)
/* require additional info */
#define VIRTCHNL2_CAP_VLAN			BIT(12)
#define VIRTCHNL2_CAP_PTP			BIT(13)
#define VIRTCHNL2_CAP_ADV_RSS			BIT(15)
#define VIRTCHNL2_CAP_FDIR			BIT(16)
#define VIRTCHNL2_CAP_RX_FLEX_DESC		BIT(17)
#define VIRTCHNL2_CAP_PTYPE			BIT(18)
#define VIRTCHNL2_CAP_LOOPBACK			BIT(19)
/* Enable miss completion types plus ability to detect a miss completion if a
 * reserved bit is set in a standared completion's tag.
 */
#define VIRTCHNL2_CAP_MISS_COMPL_TAG		BIT(20)
/* this must be the last capability */
#define VIRTCHNL2_CAP_OEM			BIT(63)

/* VIRTCHNL2_TXQ_SCHED_MODE
 * Transmit Queue Scheduling Modes - Queue mode is the legacy mode i.e. inorder
 * completions where descriptors and buffers are completed at the same time.
 * Flow scheduling mode allows for out of order packet processing where
 * descriptors are cleaned in order, but buffers can be completed out of order.
 */
#define VIRTCHNL2_TXQ_SCHED_MODE_QUEUE		0
#define VIRTCHNL2_TXQ_SCHED_MODE_FLOW		1

/* VIRTCHNL2_TXQ_FLAGS
 * Transmit Queue feature flags
 *
 * Enable rule miss completion type; packet completion for a packet
 * sent on exception path; only relevant in flow scheduling mode
 */
#define VIRTCHNL2_TXQ_ENABLE_MISS_COMPL		BIT(0)

/* VIRTCHNL2_PEER_TYPE
 * Transmit mailbox peer type
 */
#define VIRTCHNL2_RDMA_CPF			0
#define VIRTCHNL2_NVME_CPF			1
#define VIRTCHNL2_ATE_CPF			2
#define VIRTCHNL2_LCE_CPF			3

/* VIRTCHNL2_RXQ_FLAGS
 * Receive Queue Feature flags
 */
#define VIRTCHNL2_RXQ_RSC			BIT(0)
#define VIRTCHNL2_RXQ_HDR_SPLIT			BIT(1)
/* When set, packet descriptors are flushed by hardware immediately after
 * processing each packet.
 */
#define VIRTCHNL2_RXQ_IMMEDIATE_WRITE_BACK	BIT(2)
#define VIRTCHNL2_RX_DESC_SIZE_16BYTE		BIT(3)
#define VIRTCHNL2_RX_DESC_SIZE_32BYTE		BIT(4)

/* VIRTCHNL2_RSS_ALGORITHM
 * Type of RSS algorithm
 */
#define VIRTCHNL2_RSS_ALG_TOEPLITZ_ASYMMETRIC		0
#define VIRTCHNL2_RSS_ALG_R_ASYMMETRIC			1
#define VIRTCHNL2_RSS_ALG_TOEPLITZ_SYMMETRIC		2
#define VIRTCHNL2_RSS_ALG_XOR_SYMMETRIC			3

/* VIRTCHNL2_EVENT_CODES
 * Type of event
 */
#define VIRTCHNL2_EVENT_UNKNOWN			0
#define VIRTCHNL2_EVENT_LINK_CHANGE		1
/* These messages are only sent to PF from CP */
#define VIRTCHNL2_EVENT_START_RESET_ADI		2
#define VIRTCHNL2_EVENT_FINISH_RESET_ADI	3
#define VIRTCHNL2_EVENT_ADI_ACTIVE		4

/* VIRTCHNL2_QUEUE_TYPE
 * Transmit and Receive queue types are valid in legacy as well as split queue
 * models. With Split Queue model, 2 additional types are introduced -
 * TX_COMPLETION and RX_BUFFER. In split queue model, receive  corresponds to
 * the queue where hardware posts completions.
 */
#define VIRTCHNL2_QUEUE_TYPE_TX			0
#define VIRTCHNL2_QUEUE_TYPE_RX			1
#define VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION	2
#define VIRTCHNL2_QUEUE_TYPE_RX_BUFFER		3
#define VIRTCHNL2_QUEUE_TYPE_CONFIG_TX		4
#define VIRTCHNL2_QUEUE_TYPE_CONFIG_RX		5
#define VIRTCHNL2_QUEUE_TYPE_P2P_TX		6
#define VIRTCHNL2_QUEUE_TYPE_P2P_RX		7
#define VIRTCHNL2_QUEUE_TYPE_P2P_TX_COMPLETION	8
#define VIRTCHNL2_QUEUE_TYPE_P2P_RX_BUFFER	9
#define VIRTCHNL2_QUEUE_TYPE_MBX_TX		10
#define VIRTCHNL2_QUEUE_TYPE_MBX_RX		11

/* VIRTCHNL2_ITR_IDX
 * Virtchannel interrupt throttling rate index
 */
#define VIRTCHNL2_ITR_IDX_0			0
#define VIRTCHNL2_ITR_IDX_1			1

/* VIRTCHNL2_VECTOR_LIMITS
 * Since PF/VF messages are limited by __le16 size, precalculate the maximum
 * possible values of nested elements in virtchnl structures that virtual
 * channel can possibly handle in a single message.
 */

#define VIRTCHNL2_OP_DEL_ENABLE_DISABLE_QUEUES_MAX (\
		((__le16)(~0) - sizeof(struct virtchnl2_del_ena_dis_queues)) / \
		sizeof(struct virtchnl2_queue_chunk))

#define VIRTCHNL2_OP_MAP_UNMAP_QUEUE_VECTOR_MAX (\
		((__le16)(~0) - sizeof(struct virtchnl2_queue_vector_maps)) / \
		sizeof(struct virtchnl2_queue_vector))

/* VIRTCHNL2_MAC_TYPE
 * VIRTCHNL2_MAC_ADDR_PRIMARY
 * PF/VF driver should set @type to VIRTCHNL2_MAC_ADDR_PRIMARY for the
 * primary/device unicast MAC address filter for VIRTCHNL2_OP_ADD_MAC_ADDR and
 * VIRTCHNL2_OP_DEL_MAC_ADDR. This allows for the underlying control plane
 * function to accurately track the MAC address and for VM/function reset.
 *
 * VIRTCHNL2_MAC_ADDR_EXTRA
 * PF/VF driver should set @type to VIRTCHNL2_MAC_ADDR_EXTRA for any extra
 * unicast and/or multicast filters that are being added/deleted via
 * VIRTCHNL2_OP_ADD_MAC_ADDR/VIRTCHNL2_OP_DEL_MAC_ADDR respectively.
 */
#define VIRTCHNL2_MAC_ADDR_PRIMARY		1
#define VIRTCHNL2_MAC_ADDR_EXTRA		2

/* VIRTCHNL2_PROMISC_FLAGS
 * Flags used for promiscuous mode
 */
#define VIRTCHNL2_UNICAST_PROMISC		BIT(0)
#define VIRTCHNL2_MULTICAST_PROMISC		BIT(1)

/* VIRTCHNL2_QUEUE_GROUP_TYPE
 * Type of queue groups
 * 0 till 0xFF is for general use
 */
#define VIRTCHNL2_QUEUE_GROUP_DATA		1
#define VIRTCHNL2_QUEUE_GROUP_MBX		2
#define VIRTCHNL2_QUEUE_GROUP_CONFIG		3

/* VIRTCHNL2_PROTO_HDR_TYPE
 * Protocol header type within a packet segment. A segment consists of one or
 * more protocol headers that make up a logical group of protocol headers. Each
 * logical group of protocol headers encapsulates or is encapsulated using/by
 * tunneling or encapsulation protocols for network virtualization.
 */
/* VIRTCHNL2_PROTO_HDR_ANY is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_ANY			0
#define VIRTCHNL2_PROTO_HDR_PRE_MAC		1
/* VIRTCHNL2_PROTO_HDR_MAC is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_MAC			2
#define VIRTCHNL2_PROTO_HDR_POST_MAC		3
#define VIRTCHNL2_PROTO_HDR_ETHERTYPE		4
#define VIRTCHNL2_PROTO_HDR_VLAN		5
#define VIRTCHNL2_PROTO_HDR_SVLAN		6
#define VIRTCHNL2_PROTO_HDR_CVLAN		7
#define VIRTCHNL2_PROTO_HDR_MPLS		8
#define VIRTCHNL2_PROTO_HDR_UMPLS		9
#define VIRTCHNL2_PROTO_HDR_MMPLS		10
#define VIRTCHNL2_PROTO_HDR_PTP			11
#define VIRTCHNL2_PROTO_HDR_CTRL		12
#define VIRTCHNL2_PROTO_HDR_LLDP		13
#define VIRTCHNL2_PROTO_HDR_ARP			14
#define VIRTCHNL2_PROTO_HDR_ECP			15
#define VIRTCHNL2_PROTO_HDR_EAPOL		16
#define VIRTCHNL2_PROTO_HDR_PPPOD		17
#define VIRTCHNL2_PROTO_HDR_PPPOE		18
/* VIRTCHNL2_PROTO_HDR_IPV4 is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_IPV4		19
/* IPv4 and IPv6 Fragment header types are only associated to
 * VIRTCHNL2_PROTO_HDR_IPV4 and VIRTCHNL2_PROTO_HDR_IPV6 respectively,
 * cannot be used independently.
 */
/* VIRTCHNL2_PROTO_HDR_IPV4_FRAG is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_IPV4_FRAG		20
/* VIRTCHNL2_PROTO_HDR_IPV6 is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_IPV6		21
/* VIRTCHNL2_PROTO_HDR_IPV6_FRAG is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_IPV6_FRAG		22
#define VIRTCHNL2_PROTO_HDR_IPV6_EH		23
/* VIRTCHNL2_PROTO_HDR_UDP is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_UDP			24
/* VIRTCHNL2_PROTO_HDR_TCP is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_TCP			25
/* VIRTCHNL2_PROTO_HDR_SCTP is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_SCTP		26
/* VIRTCHNL2_PROTO_HDR_ICMP is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_ICMP		27
/* VIRTCHNL2_PROTO_HDR_ICMPV6 is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_ICMPV6		28
#define VIRTCHNL2_PROTO_HDR_IGMP		29
#define VIRTCHNL2_PROTO_HDR_AH			30
#define VIRTCHNL2_PROTO_HDR_ESP			31
#define VIRTCHNL2_PROTO_HDR_IKE			32
#define VIRTCHNL2_PROTO_HDR_NATT_KEEP		33
/* VIRTCHNL2_PROTO_HDR_PAY is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_PAY			34
#define VIRTCHNL2_PROTO_HDR_L2TPV2		35
#define VIRTCHNL2_PROTO_HDR_L2TPV2_CONTROL	36
#define VIRTCHNL2_PROTO_HDR_L2TPV3		37
#define VIRTCHNL2_PROTO_HDR_GTP			38
#define VIRTCHNL2_PROTO_HDR_GTP_EH		39
#define VIRTCHNL2_PROTO_HDR_GTPCV2		40
#define VIRTCHNL2_PROTO_HDR_GTPC_TEID		41
#define VIRTCHNL2_PROTO_HDR_GTPU		42
#define VIRTCHNL2_PROTO_HDR_GTPU_UL		43
#define VIRTCHNL2_PROTO_HDR_GTPU_DL		44
#define VIRTCHNL2_PROTO_HDR_ECPRI		45
#define VIRTCHNL2_PROTO_HDR_VRRP		46
#define VIRTCHNL2_PROTO_HDR_OSPF		47
/* VIRTCHNL2_PROTO_HDR_TUN is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_TUN			48
#define VIRTCHNL2_PROTO_HDR_GRE			49
#define VIRTCHNL2_PROTO_HDR_NVGRE		50
#define VIRTCHNL2_PROTO_HDR_VXLAN		51
#define VIRTCHNL2_PROTO_HDR_VXLAN_GPE		52
#define VIRTCHNL2_PROTO_HDR_GENEVE		53
#define VIRTCHNL2_PROTO_HDR_NSH			54
#define VIRTCHNL2_PROTO_HDR_QUIC		55
#define VIRTCHNL2_PROTO_HDR_PFCP		56
#define VIRTCHNL2_PROTO_HDR_PFCP_NODE		57
#define VIRTCHNL2_PROTO_HDR_PFCP_SESSION	58
#define VIRTCHNL2_PROTO_HDR_RTP			59
#define VIRTCHNL2_PROTO_HDR_ROCE		60
#define VIRTCHNL2_PROTO_HDR_ROCEV1		61
#define VIRTCHNL2_PROTO_HDR_ROCEV2		62
/* protocol ids up to 32767 are reserved for AVF use */
/* 32768 - 65534 are used for user defined protocol ids */
/* VIRTCHNL2_PROTO_HDR_NO_PROTO is a mandatory protocol id */
#define VIRTCHNL2_PROTO_HDR_NO_PROTO		65535

#define VIRTCHNL2_VERSION_MAJOR_2        2
#define VIRTCHNL2_VERSION_MINOR_0        0


/* VIRTCHNL2_OP_VERSION
 * PF/VF posts its version number to the CP. CP responds with its version number
 * in the same format, along with a return code.
 * If there is a major version mismatch, then the PF/VF cannot operate.
 * If there is a minor version mismatch, then the PF/VF can operate but should
 * add a warning to the system log.
 *
 * This version opcode MUST always be specified as == 1, regardless of other
 * changes in the API. The CP must always respond to this message without
 * error regardless of version mismatch.
 */
struct virtchnl2_version_info {
	u32 major;
	u32 minor;
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_version_info);

/* VIRTCHNL2_OP_GET_CAPS
 * Dataplane driver sends this message to CP to negotiate capabilities and
 * provides a virtchnl2_get_capabilities structure with its desired
 * capabilities, max_sriov_vfs and num_allocated_vectors.
 * CP responds with a virtchnl2_get_capabilities structure updated
 * with allowed capabilities and the other fields as below.
 * If PF sets max_sriov_vfs as 0, CP will respond with max number of VFs
 * that can be created by this PF. For any other value 'n', CP responds
 * with max_sriov_vfs set to min(n, x) where x is the max number of VFs
 * allowed by CP's policy. max_sriov_vfs is not applicable for VFs.
 * If dataplane driver sets num_allocated_vectors as 0, CP will respond with 1
 * which is default vector associated with the default mailbox. For any other
 * value 'n', CP responds with a value <= n based on the CP's policy of
 * max number of vectors for a PF.
 * CP will respond with the vector ID of mailbox allocated to the PF in
 * mailbox_vector_id and the number of itr index registers in itr_idx_map.
 * It also responds with default number of vports that the dataplane driver
 * should comeup with in default_num_vports and maximum number of vports that
 * can be supported in max_vports
 */
struct virtchnl2_get_capabilities {
	/* see VIRTCHNL2_CHECKSUM_OFFLOAD_CAPS definitions */
	__le32 csum_caps;

	/* see VIRTCHNL2_SEGMENTATION_OFFLOAD_CAPS definitions */
	__le32 seg_caps;

	/* see VIRTCHNL2_HEADER_SPLIT_CAPS definitions */
	__le32 hsplit_caps;

	/* see VIRTCHNL2_RSC_OFFLOAD_CAPS definitions */
	__le32 rsc_caps;

	/* see VIRTCHNL2_RSS_FLOW_TYPE_CAPS definitions  */
	__le64 rss_caps;


	/* see VIRTCHNL2_OTHER_CAPS definitions  */
	__le64 other_caps;

	/* DYN_CTL register offset and vector id for mailbox provided by CP */
	__le32 mailbox_dyn_ctl;
	__le16 mailbox_vector_id;
	/* Maximum number of allocated vectors for the device */
	__le16 num_allocated_vectors;

	/* Maximum number of queues that can be supported */
	__le16 max_rx_q;
	__le16 max_tx_q;
	__le16 max_rx_bufq;
	__le16 max_tx_complq;

	/* The PF sends the maximum VFs it is requesting. The CP responds with
	 * the maximum VFs granted.
	 */
	__le16 max_sriov_vfs;

	/* maximum number of vports that can be supported */
	__le16 max_vports;
	/* default number of vports driver should allocate on load */
	__le16 default_num_vports;

	/* Max header length hardware can parse/checksum, in bytes */
	__le16 max_tx_hdr_size;

	/* Max number of scatter gather buffers that can be sent per transmit
	 * packet without needing to be linearized
	 */
	u8 max_sg_bufs_per_tx_pkt;

	u8 reserved1;
	/* upper bound of number of ADIs supported */
	__le16 max_adis;

	/* version of Control Plane that is running */
	__le16 oem_cp_ver_major;
	__le16 oem_cp_ver_minor;
	/* see VIRTCHNL2_DEVICE_TYPE definitions */
	__le32 device_type;

	/* min packet length supported by device for single segment offload */
	u8 min_sso_packet_len;
	/* max number of header buffers that can be used for an LSO */
	u8 max_hdr_buf_per_lso;

	u8 reserved[10];
};

VIRTCHNL2_CHECK_STRUCT_LEN(80, virtchnl2_get_capabilities);

struct virtchnl2_queue_reg_chunk {
	/* see VIRTCHNL2_QUEUE_TYPE definitions */
	__le32 type;
	__le32 start_queue_id;
	__le32 num_queues;
	__le32 pad;

	/* Queue tail register offset and spacing provided by CP */
	__le64 qtail_reg_start;
	__le32 qtail_reg_spacing;

	u8 reserved[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(32, virtchnl2_queue_reg_chunk);

/* structure to specify several chunks of contiguous queues */
struct virtchnl2_queue_reg_chunks {
	__le16 num_chunks;
	u8 reserved[6];
	struct virtchnl2_queue_reg_chunk chunks[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(40, virtchnl2_queue_reg_chunks);

/* VIRTCHNL2_VPORT_FLAGS */
#define VIRTCHNL2_VPORT_UPLINK_PORT	BIT(0)

#define VIRTCHNL2_ETH_LENGTH_OF_ADDRESS  6

/* VIRTCHNL2_OP_CREATE_VPORT
 * PF sends this message to CP to create a vport by filling in required
 * fields of virtchnl2_create_vport structure.
 * CP responds with the updated virtchnl2_create_vport structure containing the
 * necessary fields followed by chunks which in turn will have an array of
 * num_chunks entries of virtchnl2_queue_chunk structures.
 */
struct virtchnl2_create_vport {
	/* PF/VF populates the following fields on request */
	/* see VIRTCHNL2_VPORT_TYPE definitions */
	__le16 vport_type;

	/* see VIRTCHNL2_QUEUE_MODEL definitions */
	__le16 txq_model;

	/* see VIRTCHNL2_QUEUE_MODEL definitions */
	__le16 rxq_model;
	__le16 num_tx_q;
	/* valid only if txq_model is split queue */
	__le16 num_tx_complq;
	__le16 num_rx_q;
	/* valid only if rxq_model is split queue */
	__le16 num_rx_bufq;
	/* relative receive queue index to be used as default */
	__le16 default_rx_q;
	/* used to align PF and CP in case of default multiple vports, it is
	 * filled by the PF and CP returns the same value, to enable the driver
	 * to support multiple asynchronous parallel CREATE_VPORT requests and
	 * associate a response to a specific request
	 */
	__le16 vport_index;

	/* CP populates the following fields on response */
	__le16 max_mtu;
	__le32 vport_id;
	u8 default_mac_addr[VIRTCHNL2_ETH_LENGTH_OF_ADDRESS];
	/* see VIRTCHNL2_VPORT_FLAGS definitions */
	__le16 vport_flags;
	/* see VIRTCHNL2_RX_DESC_IDS definitions */
	__le64 rx_desc_ids;
	/* see VIRTCHNL2_TX_DESC_IDS definitions */
	__le64 tx_desc_ids;

	u8 reserved1[72];

	/* see VIRTCHNL2_RSS_ALGORITHM definitions */
	__le32 rss_algorithm;
	__le16 rss_key_size;
	__le16 rss_lut_size;

	/* see VIRTCHNL2_HEADER_SPLIT_CAPS definitions */
	__le32 rx_split_pos;

	u8 reserved[20];
	struct virtchnl2_queue_reg_chunks chunks;
};

VIRTCHNL2_CHECK_STRUCT_LEN(192, virtchnl2_create_vport);

/* VIRTCHNL2_OP_DESTROY_VPORT
 * VIRTCHNL2_OP_ENABLE_VPORT
 * VIRTCHNL2_OP_DISABLE_VPORT
 * PF sends this message to CP to destroy, enable or disable a vport by filling
 * in the vport_id in virtchnl2_vport structure.
 * CP responds with the status of the requested operation.
 */
struct virtchnl2_vport {
	__le32 vport_id;
	u8 reserved[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_vport);

/* Transmit queue config info */
struct virtchnl2_txq_info {
	__le64 dma_ring_addr;

	/* see VIRTCHNL2_QUEUE_TYPE definitions */
	__le32 type;

	__le32 queue_id;
	/* valid only if queue model is split and type is transmit queue. Used
	 * in many to one mapping of transmit queues to completion queue
	 */
	__le16 relative_queue_id;

	/* see VIRTCHNL2_QUEUE_MODEL definitions */
	__le16 model;

	/* see VIRTCHNL2_TXQ_SCHED_MODE definitions */
	__le16 sched_mode;

	/* see VIRTCHNL2_TXQ_FLAGS definitions */
	__le16 qflags;
	__le16 ring_len;

	/* valid only if queue model is split and type is transmit queue */
	__le16 tx_compl_queue_id;
	/* valid only if queue type is VIRTCHNL2_QUEUE_TYPE_MAILBOX_TX */
	/* see VIRTCHNL2_PEER_TYPE definitions */
	__le16 peer_type;
	/* valid only if queue type is CONFIG_TX and used to deliver messages
	 * for the respective CONFIG_TX queue
	 */
	__le16 peer_rx_queue_id;

	u8 pad[4];

	/* Egress pasid is used for SIOV use case */
	__le32 egress_pasid;
	__le32 egress_hdr_pasid;
	__le32 egress_buf_pasid;

	u8 reserved[8];
};

VIRTCHNL2_CHECK_STRUCT_LEN(56, virtchnl2_txq_info);

/* VIRTCHNL2_OP_CONFIG_TX_QUEUES
 * PF sends this message to set up parameters for one or more transmit queues.
 * This message contains an array of num_qinfo instances of virtchnl2_txq_info
 * structures. CP configures requested queues and returns a status code. If
 * num_qinfo specified is greater than the number of queues associated with the
 * vport, an error is returned and no queues are configured.
 */
struct virtchnl2_config_tx_queues {
	__le32 vport_id;
	__le16 num_qinfo;

	u8 reserved[10];
	struct virtchnl2_txq_info qinfo[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(72, virtchnl2_config_tx_queues);

/* Receive queue config info */
struct virtchnl2_rxq_info {
	/* see VIRTCHNL2_RX_DESC_IDS definitions */
	__le64 desc_ids;
	__le64 dma_ring_addr;

	/* see VIRTCHNL2_QUEUE_TYPE definitions */
	__le32 type;
	__le32 queue_id;

	/* see QUEUE_MODEL definitions */
	__le16 model;

	__le16 hdr_buffer_size;
	__le32 data_buffer_size;
	__le32 max_pkt_size;

	__le16 ring_len;
	u8 buffer_notif_stride;
	u8 pad[1];

	/* Applicable only for receive buffer queues */
	__le64 dma_head_wb_addr;

	/* Applicable only for receive completion queues */
	/* see VIRTCHNL2_RXQ_FLAGS definitions */
	__le16 qflags;

	__le16 rx_buffer_low_watermark;

	/* valid only in split queue model */
	__le16 rx_bufq1_id;
	/* valid only in split queue model */
	__le16 rx_bufq2_id;
	/* it indicates if there is a second buffer, rx_bufq2_id is valid only
	 * if this field is set
	 */
	u8 bufq2_ena;
	u8 pad2[3];

	/* Ingress pasid is used for SIOV use case */
	__le32 ingress_pasid;
	__le32 ingress_hdr_pasid;
	__le32 ingress_buf_pasid;

	u8 reserved[16];
};

VIRTCHNL2_CHECK_STRUCT_LEN(88, virtchnl2_rxq_info);

/* VIRTCHNL2_OP_CONFIG_RX_QUEUES
 * PF sends this message to set up parameters for one or more receive queues.
 * This message contains an array of num_qinfo instances of virtchnl2_rxq_info
 * structures. CP configures requested queues and returns a status code.
 * If the number of queues specified is greater than the number of queues
 * associated with the vport, an error is returned and no queues are configured.
 */
struct virtchnl2_config_rx_queues {
	__le32 vport_id;
	__le16 num_qinfo;

	u8 reserved[18];
	struct virtchnl2_rxq_info qinfo[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(112, virtchnl2_config_rx_queues);

/* VIRTCHNL2_OP_ADD_QUEUES
 * PF sends this message to request additional transmit/receive queues beyond
 * the ones that were assigned via CREATE_VPORT request. virtchnl2_add_queues
 * structure is used to specify the number of each type of queues.
 * CP responds with the same structure with the actual number of queues assigned
 * followed by num_chunks of virtchnl2_queue_chunk structures.
 */
struct virtchnl2_add_queues {
	__le32 vport_id;
	__le16 num_tx_q;
	__le16 num_tx_complq;
	__le16 num_rx_q;
	__le16 num_rx_bufq;
	u8 reserved[4];
	struct virtchnl2_queue_reg_chunks chunks;
};

VIRTCHNL2_CHECK_STRUCT_LEN(56, virtchnl2_add_queues);

/* Queue Groups Extension */

struct virtchnl2_rx_queue_group_info {
	/* IN/OUT, user can ask to update rss_lut size originally allocated
	 * by CreateVport command. New size will be returned if allocation
	 * succeeded, otherwise original rss_size from CreateVport will
	 * be returned.
	 */
	__le16 rss_lut_size;
	/* Future extension purpose */
	u8 pad[6];
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_rx_queue_group_info);

struct virtchnl2_tx_queue_group_info { /* IN */
	/* TX TC queue group will be connected to */
	u8 tx_tc;
	/* Each group can have its own priority, value 0-7, while each group
	 * with unique priority is strict priority.
	 * It can be single set of queue groups which configured with
	 * same priority, then they are assumed part of WFQ arbitration
	 * group and are expected to be assigned with weight.
	 */
	u8 priority;
	/* Determines if queue group is expected to be Strict Priority
	 * according to its priority
	 */
	u8 is_sp;
	u8 pad;

	/* Peak Info Rate Weight in case Queue Group is part of WFQ
	 * arbitration set.
	 * The weights of the groups are independent of each other.
	 * Possible values: 1-200
	 */
	__le16 pir_weight;
	/* Future extension purpose for CIR only */
	u8 cir_pad[2];
	/* Future extension purpose*/
	u8 pad2[8];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_tx_queue_group_info);

struct virtchnl2_queue_group_id {
	/* Queue group ID - depended on it's type
	 * Data: is an ID which is relative to Vport
	 * Config & Mailbox: is an ID which is relative to func.
	 * This ID is use in future calls, i.e. delete.
	 * Requested by host and assigned by Control plane.
	 */
	__le16 queue_group_id;
	/* Functional type: see VIRTCHNL2_QUEUE_GROUP_TYPE definitions */
	__le16 queue_group_type;
	u8 pad[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_queue_group_id);

struct virtchnl2_queue_group_info {
	/* IN */
	struct virtchnl2_queue_group_id qg_id;
	/* IN, Number of queue of different types in the group. */
	__le16 num_tx_q;
	__le16 num_tx_complq;
	__le16 num_rx_q;
	__le16 num_rx_bufq;

	struct virtchnl2_tx_queue_group_info tx_q_grp_info;
	struct virtchnl2_rx_queue_group_info rx_q_grp_info;
	/* Future extension purpose */
	u8 pad[40];
	struct virtchnl2_queue_reg_chunks chunks; /* OUT */
};

VIRTCHNL2_CHECK_STRUCT_LEN(120, virtchnl2_queue_group_info);

struct virtchnl2_queue_groups {
	__le16 num_queue_groups;
	u8 pad[6];
	struct virtchnl2_queue_group_info groups[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(128, virtchnl2_queue_groups);

/* VIRTCHNL2_OP_ADD_QUEUE_GROUPS
 * PF sends this message to request additional transmit/receive queue groups
 * beyond the ones that were assigned via CREATE_VPORT request.
 * virtchnl2_add_queue_groups structure is used to specify the number of each
 * type of queues. CP responds with the same structure with the actual number of
 * groups and queues assigned followed by num_queue_groups and num_chunks of
 * virtchnl2_queue_groups and virtchnl2_queue_chunk structures.
 */
struct virtchnl2_add_queue_groups {
	/* IN, vport_id to add queue group to, same as allocated by CreateVport.
	 * NA for mailbox and other types not assigned to vport
	 */
	__le32 vport_id;
	u8 pad[4];
	/* IN/OUT */
	struct virtchnl2_queue_groups qg_info;
};

VIRTCHNL2_CHECK_STRUCT_LEN(136, virtchnl2_add_queue_groups);

/* VIRTCHNL2_OP_DEL_QUEUE_GROUPS
 * PF sends this message to delete queue groups.
 * PF sends virtchnl2_delete_queue_groups struct to specify the queue groups
 * to be deleted. CP performs requested action and returns status and update
 * num_queue_groups with number of successfully deleted queue groups.
 */
struct virtchnl2_delete_queue_groups {
	/* IN, vport_id to delete queue group from, same as
	 * allocated by CreateVport.
	 */
	__le32 vport_id;
	/* IN/OUT, Defines number of groups provided below */
	__le16 num_queue_groups;
	u8 pad[2];

	/* IN, IDs & types of Queue Groups to delete */
	struct virtchnl2_queue_group_id qg_ids[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_delete_queue_groups);

/* Structure to specify a chunk of contiguous interrupt vectors */
struct virtchnl2_vector_chunk {
	__le16 start_vector_id;
	__le16 start_evv_id;
	__le16 num_vectors;
	__le16 pad1;

	/* Register offsets and spacing provided by CP.
	 * dynamic control registers are used for enabling/disabling/re-enabling
	 * interrupts and updating interrupt rates in the hotpath. Any changes
	 * to interrupt rates in the dynamic control registers will be reflected
	 * in the interrupt throttling rate registers.
	 * itrn registers are used to update interrupt rates for specific
	 * interrupt indices without modifying the state of the interrupt.
	 */
	__le32 dynctl_reg_start;
	/* register spacing between dynctl registers of 2 consecutive vectors */
	__le32 dynctl_reg_spacing;

	__le32 itrn_reg_start;
	/* register spacing between itrn registers of 2 consecutive vectors */
	__le32 itrn_reg_spacing;
	/* register spacing between itrn registers of the same vector
	 * where n=0..2
	 */
	__le32 itrn_index_spacing;
	u8 reserved[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(32, virtchnl2_vector_chunk);

/* Structure to specify several chunks of contiguous interrupt vectors */
struct virtchnl2_vector_chunks {
	__le16 num_vchunks;
	u8 reserved[14];
	struct virtchnl2_vector_chunk vchunks[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(48, virtchnl2_vector_chunks);

/* VIRTCHNL2_OP_ALLOC_VECTORS
 * PF sends this message to request additional interrupt vectors beyond the
 * ones that were assigned via GET_CAPS request. virtchnl2_alloc_vectors
 * structure is used to specify the number of vectors requested. CP responds
 * with the same structure with the actual number of vectors assigned followed
 * by virtchnl2_vector_chunks structure identifying the vector ids.
 */
struct virtchnl2_alloc_vectors {
	__le16 num_vectors;
	u8 reserved[14];
	struct virtchnl2_vector_chunks vchunks;
};

VIRTCHNL2_CHECK_STRUCT_LEN(64, virtchnl2_alloc_vectors);

/* VIRTCHNL2_OP_DEALLOC_VECTORS
 * PF sends this message to release the vectors.
 * PF sends virtchnl2_vector_chunks struct to specify the vectors it is giving
 * away. CP performs requested action and returns status.
 */

/* VIRTCHNL2_OP_GET_RSS_LUT
 * VIRTCHNL2_OP_SET_RSS_LUT
 * PF sends this message to get or set RSS lookup table. Only supported if
 * both PF and CP drivers set the VIRTCHNL2_CAP_RSS bit during configuration
 * negotiation. Uses the virtchnl2_rss_lut structure
 */
struct virtchnl2_rss_lut {
	__le32 vport_id;
	__le16 lut_entries_start;
	__le16 lut_entries;
	u8 reserved[4];
	__le32 lut[1]; /* RSS lookup table */
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_rss_lut);

/* VIRTCHNL2_OP_GET_RSS_KEY
 * PF sends this message to get RSS key. Only supported if both PF and CP
 * drivers set the VIRTCHNL2_CAP_RSS bit during configuration negotiation. Uses
 * the virtchnl2_rss_key structure
 */

/* VIRTCHNL2_OP_GET_RSS_HASH
 * VIRTCHNL2_OP_SET_RSS_HASH
 * PF sends these messages to get and set the hash filter enable bits for RSS.
 * By default, the CP sets these to all possible traffic types that the
 * hardware supports. The PF can query this value if it wants to change the
 * traffic types that are hashed by the hardware.
 * Only supported if both PF and CP drivers set the VIRTCHNL2_CAP_RSS bit
 * during configuration negotiation.
 */
struct virtchnl2_rss_hash {
	/* Packet Type Groups bitmap */
	__le64 ptype_groups;
	__le32 vport_id;
	u8 reserved[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_rss_hash);

/* VIRTCHNL2_OP_SET_SRIOV_VFS
 * This message is used to set number of SRIOV VFs to be created. The actual
 * allocation of resources for the VFs in terms of vport, queues and interrupts
 * is done by CP. When this call completes, the APF driver calls
 * pci_enable_sriov to let the OS instantiate the SRIOV PCIE devices.
 * The number of VFs set to 0 will destroy all the VFs of this function.
 */

struct virtchnl2_sriov_vfs_info {
	__le16 num_vfs;
	__le16 pad;
};

VIRTCHNL2_CHECK_STRUCT_LEN(4, virtchnl2_sriov_vfs_info);

/* structure to specify single chunk of queue */
/* 'chunks' is fixed size(not flexible) and will be deprecated at some point */
struct virtchnl2_non_flex_queue_reg_chunks {
	__le16 num_chunks;
	u8 reserved[6];
	struct virtchnl2_queue_reg_chunk chunks[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(40, virtchnl2_non_flex_queue_reg_chunks);

/* structure to specify single chunk of interrupt vector */
/* 'vchunks' is fixed size(not flexible) and will be deprecated at some point */
struct virtchnl2_non_flex_vector_chunks {
	__le16 num_vchunks;
	u8 reserved[14];
	struct virtchnl2_vector_chunk vchunks[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(48, virtchnl2_non_flex_vector_chunks);

/* VIRTCHNL2_OP_NON_FLEX_CREATE_ADI
 * PF sends this message to CP to create ADI by filling in required
 * fields of virtchnl2_non_flex_create_adi structure.
 * CP responds with the updated virtchnl2_non_flex_create_adi structure containing
 * the necessary fields followed by chunks which in turn will have an array of
 * num_chunks entries of virtchnl2_queue_chunk structures.
 */
struct virtchnl2_non_flex_create_adi {
	/* PF sends PASID to CP */
	__le32 pasid;
	/*
	 * mbx_id is set to 1 by PF when requesting CP to provide HW mailbox
	 * id else it is set to 0 by PF
	 */
	__le16 mbx_id;
	/* PF sends mailbox vector id to CP */
	__le16 mbx_vec_id;
	/* PF populates this ADI index */
	__le16 adi_index;
	/* CP populates ADI id */
	__le16 adi_id;
	u8 reserved[64];
	u8 pad[4];
	/* CP populates queue chunks */
	struct virtchnl2_non_flex_queue_reg_chunks chunks;
	/* PF sends vector chunks to CP */
	struct virtchnl2_non_flex_vector_chunks vchunks;
};

VIRTCHNL2_CHECK_STRUCT_LEN(168, virtchnl2_non_flex_create_adi);

/* VIRTCHNL2_OP_DESTROY_ADI
 * PF sends this message to CP to destroy ADI by filling
 * in the adi_id in virtchnl2_destropy_adi structure.
 * CP responds with the status of the requested operation.
 */
struct virtchnl2_non_flex_destroy_adi {
	__le16 adi_id;
	u8 reserved[2];
};

VIRTCHNL2_CHECK_STRUCT_LEN(4, virtchnl2_non_flex_destroy_adi);

/* Based on the descriptor type the PF supports, CP fills ptype_id_10 or
 * ptype_id_8 for flex and base descriptor respectively. If ptype_id_10 value
 * is set to 0xFFFF, PF should consider this ptype as dummy one and it is the
 * last ptype.
 */
struct virtchnl2_ptype {
	__le16 ptype_id_10;
	u8 ptype_id_8;
	/* number of protocol ids the packet supports, maximum of 32
	 * protocol ids are supported
	 */
	u8 proto_id_count;
	__le16 pad;
	/* proto_id_count decides the allocation of protocol id array */
	/* see VIRTCHNL2_PROTO_HDR_TYPE */
	__le16 proto_id[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_ptype);

/* VIRTCHNL2_OP_GET_PTYPE_INFO
 * PF sends this message to CP to get all supported packet types. It does by
 * filling in start_ptype_id and num_ptypes. Depending on descriptor type the
 * PF supports, it sets num_ptypes to 1024 (10-bit ptype) for flex descriptor
 * and 256 (8-bit ptype) for base descriptor support. CP responds back to PF by
 * populating start_ptype_id, num_ptypes and array of ptypes. If all ptypes
 * doesn't fit into one mailbox buffer, CP splits ptype info into multiple
 * messages, where each message will have the start ptype id, number of ptypes
 * sent in that message and the ptype array itself. When CP is done updating
 * all ptype information it extracted from the package (number of ptypes
 * extracted might be less than what PF expects), it will append a dummy ptype
 * (which has 'ptype_id_10' of 'struct virtchnl2_ptype' as 0xFFFF) to the ptype
 * array. PF is expected to receive multiple VIRTCHNL2_OP_GET_PTYPE_INFO
 * messages.
 */
struct virtchnl2_get_ptype_info {
	__le16 start_ptype_id;
	__le16 num_ptypes;
	__le32 pad;
	struct virtchnl2_ptype ptype[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_get_ptype_info);

/* VIRTCHNL2_OP_GET_STATS
 * PF/VF sends this message to CP to get the update stats by specifying the
 * vport_id. CP responds with stats in struct virtchnl2_vport_stats.
 */
struct virtchnl2_vport_stats {
	__le32 vport_id;
	u8 pad[4];

	__le64 rx_bytes;		/* received bytes */
	__le64 rx_unicast;		/* received unicast pkts */
	__le64 rx_multicast;		/* received multicast pkts */
	__le64 rx_broadcast;		/* received broadcast pkts */
	__le64 rx_discards;
	__le64 rx_errors;
	__le64 rx_unknown_protocol;
	__le64 tx_bytes;		/* transmitted bytes */
	__le64 tx_unicast;		/* transmitted unicast pkts */
	__le64 tx_multicast;		/* transmitted multicast pkts */
	__le64 tx_broadcast;		/* transmitted broadcast pkts */
	__le64 tx_discards;
	__le64 tx_errors;
	__le64 rx_invalid_frame_length;
	__le64 rx_overflow_drop;
};

VIRTCHNL2_CHECK_STRUCT_LEN(128, virtchnl2_vport_stats);

/* physical port statistics */
struct virtchnl2_phy_port_stats {
	__le64 rx_bytes;
	__le64 rx_unicast_pkts;
	__le64 rx_multicast_pkts;
	__le64 rx_broadcast_pkts;
	__le64 rx_size_64_pkts;
	__le64 rx_size_127_pkts;
	__le64 rx_size_255_pkts;
	__le64 rx_size_511_pkts;
	__le64 rx_size_1023_pkts;
	__le64 rx_size_1518_pkts;
	__le64 rx_size_jumbo_pkts;
	__le64 rx_xon_events;
	__le64 rx_xoff_events;
	__le64 rx_undersized_pkts;
	__le64 rx_fragmented_pkts;
	__le64 rx_oversized_pkts;
	__le64 rx_jabber_pkts;
	__le64 rx_csum_errors;
	__le64 rx_length_errors;
	__le64 rx_dropped_pkts;
	__le64 rx_crc_errors;
	/* Frames with length < 64 and a bad CRC */
	__le64 rx_runt_errors;
	__le64 rx_illegal_bytes;
	__le64 rx_total_pkts;
	u8 rx_reserved[128];

	__le64 tx_bytes;
	__le64 tx_unicast_pkts;
	__le64 tx_multicast_pkts;
	__le64 tx_broadcast_pkts;
	__le64 tx_errors;
	__le64 tx_timeout_events;
	__le64 tx_size_64_pkts;
	__le64 tx_size_127_pkts;
	__le64 tx_size_255_pkts;
	__le64 tx_size_511_pkts;
	__le64 tx_size_1023_pkts;
	__le64 tx_size_1518_pkts;
	__le64 tx_size_jumbo_pkts;
	__le64 tx_xon_events;
	__le64 tx_xoff_events;
	__le64 tx_dropped_link_down_pkts;
	__le64 tx_total_pkts;
	u8 tx_reserved[128];
	__le64 mac_local_faults;
	__le64 mac_remote_faults;
};

VIRTCHNL2_CHECK_STRUCT_LEN(600, virtchnl2_phy_port_stats);

/* VIRTCHNL2_OP_GET_PORT_STATS
 * PF/VF sends this message to CP to get the updated stats by specifying the
 * vport_id. CP responds with stats in struct virtchnl2_port_stats that
 * includes both physical port as well as vport statistics.
 */
struct virtchnl2_port_stats {
	__le32 vport_id;
	u8 pad[4];

	struct virtchnl2_phy_port_stats phy_port_stats;
	struct virtchnl2_vport_stats virt_port_stats;
};

VIRTCHNL2_CHECK_STRUCT_LEN(736, virtchnl2_port_stats);

/* VIRTCHNL2_OP_EVENT
 * CP sends this message to inform the PF/VF driver of events that may affect
 * it. No direct response is expected from the driver, though it may generate
 * other messages in response to this one.
 */
struct virtchnl2_event {
	/* see VIRTCHNL2_EVENT_CODES definitions */
	__le32 event;
	/* link_speed provided in Mbps */
	__le32 link_speed;
	__le32 vport_id;
	u8 link_status;
	u8 pad[1];
	/* CP sends reset notification to PF with corresponding ADI ID */
	__le16 adi_id;
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_event);

/* VIRTCHNL2_OP_GET_RSS_KEY
 * VIRTCHNL2_OP_SET_RSS_KEY
 * PF/VF sends this message to get or set RSS key. Only supported if both
 * PF/VF and CP drivers set the VIRTCHNL2_CAP_RSS bit during configuration
 * negotiation. Uses the virtchnl2_rss_key structure
 */
struct virtchnl2_rss_key {
	__le32 vport_id;
	__le16 key_len;
	u8 pad;
	u8 key[1];         /* RSS hash key, packed bytes */
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_rss_key);

/* structure to specify a chunk of contiguous queues */
struct virtchnl2_queue_chunk {
	/* see VIRTCHNL2_QUEUE_TYPE definitions */
	__le32 type;
	__le32 start_queue_id;
	__le32 num_queues;
	u8 reserved[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_queue_chunk);

/* structure to specify several chunks of contiguous queues */
struct virtchnl2_queue_chunks {
	__le16 num_chunks;
	u8 reserved[6];
	struct virtchnl2_queue_chunk chunks[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(24, virtchnl2_queue_chunks);

/* VIRTCHNL2_OP_ENABLE_QUEUES
 * VIRTCHNL2_OP_DISABLE_QUEUES
 * VIRTCHNL2_OP_DEL_QUEUES
 *
 * PF sends these messages to enable, disable or delete queues specified in
 * chunks. PF sends virtchnl2_del_ena_dis_queues struct to specify the queues
 * to be enabled/disabled/deleted. Also applicable to single queue receive or
 * transmit. CP performs requested action and returns status.
 */
struct virtchnl2_del_ena_dis_queues {
	__le32 vport_id;
	u8 reserved[4];
	struct virtchnl2_queue_chunks chunks;
};

VIRTCHNL2_CHECK_STRUCT_LEN(32, virtchnl2_del_ena_dis_queues);

/* Queue to vector mapping */
struct virtchnl2_queue_vector {
	__le32 queue_id;
	__le16 vector_id;
	u8 pad[2];

	/* see VIRTCHNL2_ITR_IDX definitions */
	__le32 itr_idx;

	/* see VIRTCHNL2_QUEUE_TYPE definitions */
	__le32 queue_type;
	u8 reserved[8];
};

VIRTCHNL2_CHECK_STRUCT_LEN(24, virtchnl2_queue_vector);

/* VIRTCHNL2_OP_MAP_QUEUE_VECTOR
 * VIRTCHNL2_OP_UNMAP_QUEUE_VECTOR
 *
 * PF sends this message to map or unmap queues to vectors and interrupt
 * throttling rate index registers. External data buffer contains
 * virtchnl2_queue_vector_maps structure that contains num_qv_maps of
 * virtchnl2_queue_vector structures. CP maps the requested queue vector maps
 * after validating the queue and vector ids and returns a status code.
 */
struct virtchnl2_queue_vector_maps {
	__le32 vport_id;
	__le16 num_qv_maps;
	u8 pad[10];
	struct virtchnl2_queue_vector qv_maps[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(40, virtchnl2_queue_vector_maps);

/* VIRTCHNL2_OP_LOOPBACK
 *
 * PF/VF sends this message to transition to/from the loopback state. Setting
 * the 'enable' to 1 enables the loopback state and setting 'enable' to 0
 * disables it. CP configures the state to loopback and returns status.
 */
struct virtchnl2_loopback {
	__le32 vport_id;
	u8 enable;
	u8 pad[3];
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_loopback);

/* structure to specify each MAC address */
struct virtchnl2_mac_addr {
	u8 addr[VIRTCHNL2_ETH_LENGTH_OF_ADDRESS];
	/* see VIRTCHNL2_MAC_TYPE definitions */
	u8 type;
	u8 pad;
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_mac_addr);

/* VIRTCHNL2_OP_ADD_MAC_ADDR
 * VIRTCHNL2_OP_DEL_MAC_ADDR
 *
 * PF/VF driver uses this structure to send list of MAC addresses to be
 * added/deleted to the CP where as CP performs the action and returns the
 * status.
 */
struct virtchnl2_mac_addr_list {
	__le32 vport_id;
	__le16 num_mac_addr;
	u8 pad[2];
	struct virtchnl2_mac_addr mac_addr_list[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_mac_addr_list);

/* VIRTCHNL2_OP_CONFIG_PROMISCUOUS_MODE
 *
 * PF/VF sends vport id and flags to the CP where as CP performs the action
 * and returns the status.
 */
struct virtchnl2_promisc_info {
	__le32 vport_id;
	/* see VIRTCHNL2_PROMISC_FLAGS definitions */
	__le16 flags;
	u8 pad[2];
};

VIRTCHNL2_CHECK_STRUCT_LEN(8, virtchnl2_promisc_info);

/* VIRTCHNL2_PTP_CAPS
 * PTP capabilities
 */
#define VIRTCHNL2_PTP_CAP_LEGACY_CROSS_TIME	BIT(0)
#define VIRTCHNL2_PTP_CAP_PTM			BIT(1)
#define VIRTCHNL2_PTP_CAP_DEVICE_CLOCK_CONTROL	BIT(2)
#define VIRTCHNL2_PTP_CAP_TX_TSTAMPS_DIRECT	BIT(3)
#define	VIRTCHNL2_PTP_CAP_TX_TSTAMPS_VIRTCHNL	BIT(4)

/* Legacy cross time registers offsets */
struct virtchnl2_ptp_legacy_cross_time_reg {
	__le32 shadow_time_0;
	__le32 shadow_time_l;
	__le32 shadow_time_h;
	__le32 cmd_sync;
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_ptp_legacy_cross_time_reg);

/* PTM cross time registers offsets */
struct virtchnl2_ptp_ptm_cross_time_reg {
	__le32 art_l;
	__le32 art_h;
	__le32 cmd_sync;
	u8 pad[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_ptp_ptm_cross_time_reg);

/* Registers needed to control the main clock */
struct virtchnl2_ptp_device_clock_control {
	__le32 cmd;
	__le32 incval_l;
	__le32 incval_h;
	__le32 shadj_l;
	__le32 shadj_h;
	u8 pad[4];
};

VIRTCHNL2_CHECK_STRUCT_LEN(24, virtchnl2_ptp_device_clock_control);

/* Structure that defines tx tstamp entry - index and register offset */
struct virtchnl2_ptp_tx_tstamp_entry {
	__le32 tx_latch_register_base;
	__le32 tx_latch_register_offset;
	u8 index;
	u8 pad[7];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_ptp_tx_tstamp_entry);

/* Structure that defines tx tstamp entries - total number of latches
 * and the array of entries.
 */
struct virtchnl2_ptp_tx_tstamp {
	__le16 num_latches;
	/* latch size expressed in bits */
	__le16 latch_size;
	u8 pad[4];
	struct virtchnl2_ptp_tx_tstamp_entry ptp_tx_tstamp_entries[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(24, virtchnl2_ptp_tx_tstamp);

/* VIRTCHNL2_OP_GET_PTP_CAPS
 * PV/VF sends this message to negotiate PTP capabilities. CP updates bitmap
 * with supported features and fulfills appropriate structures.
 */
struct virtchnl2_get_ptp_caps {
	/* PTP capability bitmap */
	/* see VIRTCHNL2_PTP_CAPS definitions */
	__le32 ptp_caps;
	u8 pad[4];

	struct virtchnl2_ptp_legacy_cross_time_reg legacy_cross_time_reg;
	struct virtchnl2_ptp_ptm_cross_time_reg ptm_cross_time_reg;
	struct virtchnl2_ptp_device_clock_control device_clock_control;
	struct virtchnl2_ptp_tx_tstamp tx_tstamp;
};

VIRTCHNL2_CHECK_STRUCT_LEN(88, virtchnl2_get_ptp_caps);

/* Structure that describes tx tstamp values, index and validity */
struct virtchnl2_ptp_tx_tstamp_latch {
	__le32 tstamp_h;
	__le32 tstamp_l;
	u8 index;
	u8 valid;
	u8 pad[6];
};

VIRTCHNL2_CHECK_STRUCT_LEN(16, virtchnl2_ptp_tx_tstamp_latch);

/* VIRTCHNL2_OP_GET_PTP_TX_TSTAMP_LATCHES
 * PF/VF sends this message to receive a specified number of timestamps
 * entries.
 */
struct virtchnl2_ptp_tx_tstamp_latches {
	__le16 num_latches;
	/* latch size expressed in bits */
	__le16 latch_size;
	u8 pad[4];
	struct virtchnl2_ptp_tx_tstamp_latch tstamp_latches[1];
};

VIRTCHNL2_CHECK_STRUCT_LEN(24, virtchnl2_ptp_tx_tstamp_latches);

static inline const char *virtchnl2_op_str(__le32 v_opcode)
{
	switch (v_opcode) {
	case VIRTCHNL2_OP_VERSION:
		return "VIRTCHNL2_OP_VERSION";
	case VIRTCHNL2_OP_GET_CAPS:
		return "VIRTCHNL2_OP_GET_CAPS";
	case VIRTCHNL2_OP_CREATE_VPORT:
		return "VIRTCHNL2_OP_CREATE_VPORT";
	case VIRTCHNL2_OP_DESTROY_VPORT:
		return "VIRTCHNL2_OP_DESTROY_VPORT";
	case VIRTCHNL2_OP_ENABLE_VPORT:
		return "VIRTCHNL2_OP_ENABLE_VPORT";
	case VIRTCHNL2_OP_DISABLE_VPORT:
		return "VIRTCHNL2_OP_DISABLE_VPORT";
	case VIRTCHNL2_OP_CONFIG_TX_QUEUES:
		return "VIRTCHNL2_OP_CONFIG_TX_QUEUES";
	case VIRTCHNL2_OP_CONFIG_RX_QUEUES:
		return "VIRTCHNL2_OP_CONFIG_RX_QUEUES";
	case VIRTCHNL2_OP_ENABLE_QUEUES:
		return "VIRTCHNL2_OP_ENABLE_QUEUES";
	case VIRTCHNL2_OP_DISABLE_QUEUES:
		return "VIRTCHNL2_OP_DISABLE_QUEUES";
	case VIRTCHNL2_OP_ADD_QUEUES:
		return "VIRTCHNL2_OP_ADD_QUEUES";
	case VIRTCHNL2_OP_DEL_QUEUES:
		return "VIRTCHNL2_OP_DEL_QUEUES";
	case VIRTCHNL2_OP_MAP_QUEUE_VECTOR:
		return "VIRTCHNL2_OP_MAP_QUEUE_VECTOR";
	case VIRTCHNL2_OP_UNMAP_QUEUE_VECTOR:
		return "VIRTCHNL2_OP_UNMAP_QUEUE_VECTOR";
	case VIRTCHNL2_OP_GET_RSS_KEY:
		return "VIRTCHNL2_OP_GET_RSS_KEY";
	case VIRTCHNL2_OP_SET_RSS_KEY:
		return "VIRTCHNL2_OP_SET_RSS_KEY";
	case VIRTCHNL2_OP_GET_RSS_LUT:
		return "VIRTCHNL2_OP_GET_RSS_LUT";
	case VIRTCHNL2_OP_SET_RSS_LUT:
		return "VIRTCHNL2_OP_SET_RSS_LUT";
	case VIRTCHNL2_OP_GET_RSS_HASH:
		return "VIRTCHNL2_OP_GET_RSS_HASH";
	case VIRTCHNL2_OP_SET_RSS_HASH:
		return "VIRTCHNL2_OP_SET_RSS_HASH";
	case VIRTCHNL2_OP_SET_SRIOV_VFS:
		return "VIRTCHNL2_OP_SET_SRIOV_VFS";
	case VIRTCHNL2_OP_ALLOC_VECTORS:
		return "VIRTCHNL2_OP_ALLOC_VECTORS";
	case VIRTCHNL2_OP_DEALLOC_VECTORS:
		return "VIRTCHNL2_OP_DEALLOC_VECTORS";
	case VIRTCHNL2_OP_GET_PTYPE_INFO:
		return "VIRTCHNL2_OP_GET_PTYPE_INFO";
	case VIRTCHNL2_OP_GET_STATS:
		return "VIRTCHNL2_OP_GET_STATS";
	case VIRTCHNL2_OP_EVENT:
		return "VIRTCHNL2_OP_EVENT";
	case VIRTCHNL2_OP_RESET_VF:
		return "VIRTCHNL2_OP_RESET_VF";
	case VIRTCHNL2_OP_NON_FLEX_CREATE_ADI:
		return "VIRTCHNL2_OP_NON_FLEX_CREATE_ADI";
	case VIRTCHNL2_OP_NON_FLEX_DESTROY_ADI:
		return "VIRTCHNL2_OP_NON_FLEX_DESTROY_ADI";
	case VIRTCHNL2_OP_ADD_QUEUE_GROUPS:
		return "VIRTCHNL2_OP_ADD_QUEUE_GROUPS";
	case VIRTCHNL2_OP_DEL_QUEUE_GROUPS:
		return "VIRTCHNL2_OP_DEL_QUEUE_GROUPS";
	case VIRTCHNL2_OP_GET_PORT_STATS:
		return "VIRTCHNL2_OP_GET_PORT_STATS";
	case VIRTCHNL2_OP_GET_PTP_CAPS:
		return "VIRTCHNL2_OP_GET_PTP_CAPS";
	case VIRTCHNL2_OP_GET_PTP_TX_TSTAMP_LATCHES:
		return "VIRTCHNL2_OP_GET_PTP_TX_TSTAMP_LATCHES";
	default:
		return "Unsupported (update virtchnl2.h)";
	}
}

/**
 * virtchnl2_vc_validate_vf_msg
 * @ver: Virtchnl2 version info
 * @v_opcode: Opcode for the message
 * @msg: pointer to the msg buffer
 * @msglen: msg length
 *
 * validate msg format against struct for each opcode
 */
static inline int
virtchnl2_vc_validate_vf_msg(__rte_unused struct virtchnl2_version_info *ver, u32 v_opcode,
			     u8 *msg, __le16 msglen)
{
	bool err_msg_format = false;
	__le32 valid_len = 0;

	/* Validate message length. */
	switch (v_opcode) {
	case VIRTCHNL2_OP_VERSION:
		valid_len = sizeof(struct virtchnl2_version_info);
		break;
	case VIRTCHNL2_OP_GET_CAPS:
		valid_len = sizeof(struct virtchnl2_get_capabilities);
		break;
	case VIRTCHNL2_OP_CREATE_VPORT:
		valid_len = sizeof(struct virtchnl2_create_vport);
		if (msglen >= valid_len) {
			struct virtchnl2_create_vport *cvport =
				(struct virtchnl2_create_vport *)msg;

			if (cvport->chunks.num_chunks == 0) {
				/* zero chunks is allowed as input */
				break;
			}

			valid_len += (cvport->chunks.num_chunks - 1) *
				      sizeof(struct virtchnl2_queue_reg_chunk);
		}
		break;
	case VIRTCHNL2_OP_NON_FLEX_CREATE_ADI:
		valid_len = sizeof(struct virtchnl2_non_flex_create_adi);
		if (msglen >= valid_len) {
			struct virtchnl2_non_flex_create_adi *cadi =
				(struct virtchnl2_non_flex_create_adi *)msg;

			if (cadi->chunks.num_chunks == 0) {
				/* zero chunks is allowed as input */
				break;
			}

			if (cadi->vchunks.num_vchunks == 0) {
				err_msg_format = true;
				break;
			}
			valid_len += (cadi->chunks.num_chunks - 1) *
				      sizeof(struct virtchnl2_queue_reg_chunk);
			valid_len += (cadi->vchunks.num_vchunks - 1) *
				      sizeof(struct virtchnl2_vector_chunk);
		}
		break;
	case VIRTCHNL2_OP_NON_FLEX_DESTROY_ADI:
		valid_len = sizeof(struct virtchnl2_non_flex_destroy_adi);
		break;
	case VIRTCHNL2_OP_DESTROY_VPORT:
	case VIRTCHNL2_OP_ENABLE_VPORT:
	case VIRTCHNL2_OP_DISABLE_VPORT:
		valid_len = sizeof(struct virtchnl2_vport);
		break;
	case VIRTCHNL2_OP_CONFIG_TX_QUEUES:
		valid_len = sizeof(struct virtchnl2_config_tx_queues);
		if (msglen >= valid_len) {
			struct virtchnl2_config_tx_queues *ctq =
				(struct virtchnl2_config_tx_queues *)msg;
			if (ctq->num_qinfo == 0) {
				err_msg_format = true;
				break;
			}
			valid_len += (ctq->num_qinfo - 1) *
				     sizeof(struct virtchnl2_txq_info);
		}
		break;
	case VIRTCHNL2_OP_CONFIG_RX_QUEUES:
		valid_len = sizeof(struct virtchnl2_config_rx_queues);
		if (msglen >= valid_len) {
			struct virtchnl2_config_rx_queues *crq =
				(struct virtchnl2_config_rx_queues *)msg;
			if (crq->num_qinfo == 0) {
				err_msg_format = true;
				break;
			}
			valid_len += (crq->num_qinfo - 1) *
				     sizeof(struct virtchnl2_rxq_info);
		}
		break;
	case VIRTCHNL2_OP_ADD_QUEUES:
		valid_len = sizeof(struct virtchnl2_add_queues);
		if (msglen >= valid_len) {
			struct virtchnl2_add_queues *add_q =
				(struct virtchnl2_add_queues *)msg;

			if (add_q->chunks.num_chunks == 0) {
				/* zero chunks is allowed as input */
				break;
			}

			valid_len += (add_q->chunks.num_chunks - 1) *
				      sizeof(struct virtchnl2_queue_reg_chunk);
		}
		break;
	case VIRTCHNL2_OP_ENABLE_QUEUES:
	case VIRTCHNL2_OP_DISABLE_QUEUES:
	case VIRTCHNL2_OP_DEL_QUEUES:
		valid_len = sizeof(struct virtchnl2_del_ena_dis_queues);
		if (msglen >= valid_len) {
			struct virtchnl2_del_ena_dis_queues *qs =
				(struct virtchnl2_del_ena_dis_queues *)msg;
			if (qs->chunks.num_chunks == 0 ||
			    qs->chunks.num_chunks > VIRTCHNL2_OP_DEL_ENABLE_DISABLE_QUEUES_MAX) {
				err_msg_format = true;
				break;
			}
			valid_len += (qs->chunks.num_chunks - 1) *
				      sizeof(struct virtchnl2_queue_chunk);
		}
		break;
	case VIRTCHNL2_OP_ADD_QUEUE_GROUPS:
		valid_len = sizeof(struct virtchnl2_add_queue_groups);
		if (msglen != valid_len) {
			__le32 i = 0, offset = 0;
			struct virtchnl2_add_queue_groups *add_queue_grp =
				(struct virtchnl2_add_queue_groups *)msg;
			struct virtchnl2_queue_groups *groups = &(add_queue_grp->qg_info);
			struct virtchnl2_queue_group_info *grp_info;
			__le32 chunk_size = sizeof(struct virtchnl2_queue_reg_chunk);
			__le32 group_size = sizeof(struct virtchnl2_queue_group_info);
			__le32 total_chunks_size;

			if (groups->num_queue_groups == 0) {
				err_msg_format = true;
				break;
			}
			valid_len += (groups->num_queue_groups - 1) *
				      sizeof(struct virtchnl2_queue_group_info);
			offset = (u8 *)(&groups->groups[0]) - (u8 *)groups;

			for (i = 0; i < groups->num_queue_groups; i++) {
				grp_info = (struct virtchnl2_queue_group_info *)
						   ((u8 *)groups + offset);
				if (grp_info->chunks.num_chunks == 0) {
					offset += group_size;
					continue;
				}
				total_chunks_size = (grp_info->chunks.num_chunks - 1) * chunk_size;
				offset += group_size + total_chunks_size;
				valid_len += total_chunks_size;
			}
		}
		break;
	case VIRTCHNL2_OP_DEL_QUEUE_GROUPS:
		valid_len = sizeof(struct virtchnl2_delete_queue_groups);
		if (msglen != valid_len) {
			struct virtchnl2_delete_queue_groups *del_queue_grp =
				(struct virtchnl2_delete_queue_groups *)msg;

			if (del_queue_grp->num_queue_groups == 0) {
				err_msg_format = true;
				break;
			}

			valid_len += (del_queue_grp->num_queue_groups - 1) *
				      sizeof(struct virtchnl2_queue_group_id);
		}
		break;
	case VIRTCHNL2_OP_MAP_QUEUE_VECTOR:
	case VIRTCHNL2_OP_UNMAP_QUEUE_VECTOR:
		valid_len = sizeof(struct virtchnl2_queue_vector_maps);
		if (msglen >= valid_len) {
			struct virtchnl2_queue_vector_maps *v_qp =
				(struct virtchnl2_queue_vector_maps *)msg;
			if (v_qp->num_qv_maps == 0 ||
			    v_qp->num_qv_maps > VIRTCHNL2_OP_MAP_UNMAP_QUEUE_VECTOR_MAX) {
				err_msg_format = true;
				break;
			}
			valid_len += (v_qp->num_qv_maps - 1) *
				      sizeof(struct virtchnl2_queue_vector);
		}
		break;
	case VIRTCHNL2_OP_ALLOC_VECTORS:
		valid_len = sizeof(struct virtchnl2_alloc_vectors);
		if (msglen >= valid_len) {
			struct virtchnl2_alloc_vectors *v_av =
				(struct virtchnl2_alloc_vectors *)msg;

			if (v_av->vchunks.num_vchunks == 0) {
				/* zero chunks is allowed as input */
				break;
			}

			valid_len += (v_av->vchunks.num_vchunks - 1) *
				      sizeof(struct virtchnl2_vector_chunk);
		}
		break;
	case VIRTCHNL2_OP_DEALLOC_VECTORS:
		valid_len = sizeof(struct virtchnl2_vector_chunks);
		if (msglen >= valid_len) {
			struct virtchnl2_vector_chunks *v_chunks =
				(struct virtchnl2_vector_chunks *)msg;
			if (v_chunks->num_vchunks == 0) {
				err_msg_format = true;
				break;
			}
			valid_len += (v_chunks->num_vchunks - 1) *
				      sizeof(struct virtchnl2_vector_chunk);
		}
		break;
	case VIRTCHNL2_OP_GET_RSS_KEY:
	case VIRTCHNL2_OP_SET_RSS_KEY:
		valid_len = sizeof(struct virtchnl2_rss_key);
		if (msglen >= valid_len) {
			struct virtchnl2_rss_key *vrk =
				(struct virtchnl2_rss_key *)msg;

			if (vrk->key_len == 0) {
				/* zero length is allowed as input */
				break;
			}

			valid_len += vrk->key_len - 1;
		}
		break;
	case VIRTCHNL2_OP_GET_RSS_LUT:
	case VIRTCHNL2_OP_SET_RSS_LUT:
		valid_len = sizeof(struct virtchnl2_rss_lut);
		if (msglen >= valid_len) {
			struct virtchnl2_rss_lut *vrl =
				(struct virtchnl2_rss_lut *)msg;

			if (vrl->lut_entries == 0) {
				/* zero entries is allowed as input */
				break;
			}

			valid_len += (vrl->lut_entries - 1) * sizeof(vrl->lut);
		}
		break;
	case VIRTCHNL2_OP_GET_RSS_HASH:
	case VIRTCHNL2_OP_SET_RSS_HASH:
		valid_len = sizeof(struct virtchnl2_rss_hash);
		break;
	case VIRTCHNL2_OP_SET_SRIOV_VFS:
		valid_len = sizeof(struct virtchnl2_sriov_vfs_info);
		break;
	case VIRTCHNL2_OP_GET_PTYPE_INFO:
		valid_len = sizeof(struct virtchnl2_get_ptype_info);
		break;
	case VIRTCHNL2_OP_GET_STATS:
		valid_len = sizeof(struct virtchnl2_vport_stats);
		break;
	case VIRTCHNL2_OP_GET_PORT_STATS:
		valid_len = sizeof(struct virtchnl2_port_stats);
		break;
	case VIRTCHNL2_OP_RESET_VF:
		break;
	case VIRTCHNL2_OP_GET_PTP_CAPS:
		valid_len = sizeof(struct virtchnl2_get_ptp_caps);

		if (msglen >= valid_len) {
			struct virtchnl2_get_ptp_caps *ptp_caps =
			(struct virtchnl2_get_ptp_caps *)msg;

			if (ptp_caps->tx_tstamp.num_latches == 0) {
				err_msg_format = true;
				break;
			}

			valid_len += ((ptp_caps->tx_tstamp.num_latches - 1) *
				      sizeof(struct virtchnl2_ptp_tx_tstamp_entry));
		}
		break;
	case VIRTCHNL2_OP_GET_PTP_TX_TSTAMP_LATCHES:
		valid_len = sizeof(struct virtchnl2_ptp_tx_tstamp_latches);

		if (msglen >= valid_len) {
			struct virtchnl2_ptp_tx_tstamp_latches *tx_tstamp_latches =
			(struct virtchnl2_ptp_tx_tstamp_latches *)msg;

			if (tx_tstamp_latches->num_latches == 0) {
				err_msg_format = true;
				break;
			}

			valid_len += ((tx_tstamp_latches->num_latches - 1) *
				      sizeof(struct virtchnl2_ptp_tx_tstamp_latch));
		}
		break;
	/* These are always errors coming from the VF. */
	case VIRTCHNL2_OP_EVENT:
	case VIRTCHNL2_OP_UNKNOWN:
	default:
		return VIRTCHNL2_STATUS_ERR_ESRCH;
	}
	/* few more checks */
	if (err_msg_format || valid_len != msglen)
		return VIRTCHNL2_STATUS_ERR_EINVAL;

	return 0;
}

#endif /* _VIRTCHNL_2_H_ */
