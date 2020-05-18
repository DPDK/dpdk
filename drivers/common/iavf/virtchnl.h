/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2020 Intel Corporation
 */

#ifndef _VIRTCHNL_H_
#define _VIRTCHNL_H_

/* Description:
 * This header file describes the VF-PF communication protocol used
 * by the drivers for all devices starting from our 40G product line
 *
 * Admin queue buffer usage:
 * desc->opcode is always aqc_opc_send_msg_to_pf
 * flags, retval, datalen, and data addr are all used normally.
 * The Firmware copies the cookie fields when sending messages between the
 * PF and VF, but uses all other fields internally. Due to this limitation,
 * we must send all messages as "indirect", i.e. using an external buffer.
 *
 * All the VSI indexes are relative to the VF. Each VF can have maximum of
 * three VSIs. All the queue indexes are relative to the VSI.  Each VF can
 * have a maximum of sixteen queues for all of its VSIs.
 *
 * The PF is required to return a status code in v_retval for all messages
 * except RESET_VF, which does not require any response. The return value
 * is of status_code type, defined in the shared type.h.
 *
 * In general, VF driver initialization should roughly follow the order of
 * these opcodes. The VF driver must first validate the API version of the
 * PF driver, then request a reset, then get resources, then configure
 * queues and interrupts. After these operations are complete, the VF
 * driver may start its queues, optionally add MAC and VLAN filters, and
 * process traffic.
 */

/* START GENERIC DEFINES
 * Need to ensure the following enums and defines hold the same meaning and
 * value in current and future projects
 */

/* Error Codes */
enum virtchnl_status_code {
	VIRTCHNL_STATUS_SUCCESS				= 0,
	VIRTCHNL_STATUS_ERR_PARAM			= -5,
	VIRTCHNL_STATUS_ERR_NO_MEMORY			= -18,
	VIRTCHNL_STATUS_ERR_OPCODE_MISMATCH		= -38,
	VIRTCHNL_STATUS_ERR_CQP_COMPL_ERROR		= -39,
	VIRTCHNL_STATUS_ERR_INVALID_VF_ID		= -40,
	VIRTCHNL_STATUS_ERR_ADMIN_QUEUE_ERROR		= -53,
	VIRTCHNL_STATUS_ERR_NOT_SUPPORTED		= -64,
};

/* Backward compatibility */
#define VIRTCHNL_ERR_PARAM VIRTCHNL_STATUS_ERR_PARAM
#define VIRTCHNL_STATUS_NOT_SUPPORTED VIRTCHNL_STATUS_ERR_NOT_SUPPORTED

#define VIRTCHNL_LINK_SPEED_2_5GB_SHIFT		0x0
#define VIRTCHNL_LINK_SPEED_100MB_SHIFT		0x1
#define VIRTCHNL_LINK_SPEED_1000MB_SHIFT	0x2
#define VIRTCHNL_LINK_SPEED_10GB_SHIFT		0x3
#define VIRTCHNL_LINK_SPEED_40GB_SHIFT		0x4
#define VIRTCHNL_LINK_SPEED_20GB_SHIFT		0x5
#define VIRTCHNL_LINK_SPEED_25GB_SHIFT		0x6
#define VIRTCHNL_LINK_SPEED_5GB_SHIFT		0x7

enum virtchnl_link_speed {
	VIRTCHNL_LINK_SPEED_UNKNOWN	= 0,
	VIRTCHNL_LINK_SPEED_100MB	= BIT(VIRTCHNL_LINK_SPEED_100MB_SHIFT),
	VIRTCHNL_LINK_SPEED_1GB		= BIT(VIRTCHNL_LINK_SPEED_1000MB_SHIFT),
	VIRTCHNL_LINK_SPEED_10GB	= BIT(VIRTCHNL_LINK_SPEED_10GB_SHIFT),
	VIRTCHNL_LINK_SPEED_40GB	= BIT(VIRTCHNL_LINK_SPEED_40GB_SHIFT),
	VIRTCHNL_LINK_SPEED_20GB	= BIT(VIRTCHNL_LINK_SPEED_20GB_SHIFT),
	VIRTCHNL_LINK_SPEED_25GB	= BIT(VIRTCHNL_LINK_SPEED_25GB_SHIFT),
	VIRTCHNL_LINK_SPEED_2_5GB	= BIT(VIRTCHNL_LINK_SPEED_2_5GB_SHIFT),
	VIRTCHNL_LINK_SPEED_5GB		= BIT(VIRTCHNL_LINK_SPEED_5GB_SHIFT),
};

/* for hsplit_0 field of Rx HMC context */
/* deprecated with IAVF 1.0 */
enum virtchnl_rx_hsplit {
	VIRTCHNL_RX_HSPLIT_NO_SPLIT      = 0,
	VIRTCHNL_RX_HSPLIT_SPLIT_L2      = 1,
	VIRTCHNL_RX_HSPLIT_SPLIT_IP      = 2,
	VIRTCHNL_RX_HSPLIT_SPLIT_TCP_UDP = 4,
	VIRTCHNL_RX_HSPLIT_SPLIT_SCTP    = 8,
};

#define VIRTCHNL_ETH_LENGTH_OF_ADDRESS	6
/* END GENERIC DEFINES */

/* Opcodes for VF-PF communication. These are placed in the v_opcode field
 * of the virtchnl_msg structure.
 */
enum virtchnl_ops {
/* The PF sends status change events to VFs using
 * the VIRTCHNL_OP_EVENT opcode.
 * VFs send requests to the PF using the other ops.
 * Use of "advanced opcode" features must be negotiated as part of capabilities
 * exchange and are not considered part of base mode feature set.
 */
	VIRTCHNL_OP_UNKNOWN = 0,
	VIRTCHNL_OP_VERSION = 1, /* must ALWAYS be 1 */
	VIRTCHNL_OP_RESET_VF = 2,
	VIRTCHNL_OP_GET_VF_RESOURCES = 3,
	VIRTCHNL_OP_CONFIG_TX_QUEUE = 4,
	VIRTCHNL_OP_CONFIG_RX_QUEUE = 5,
	VIRTCHNL_OP_CONFIG_VSI_QUEUES = 6,
	VIRTCHNL_OP_CONFIG_IRQ_MAP = 7,
	VIRTCHNL_OP_ENABLE_QUEUES = 8,
	VIRTCHNL_OP_DISABLE_QUEUES = 9,
	VIRTCHNL_OP_ADD_ETH_ADDR = 10,
	VIRTCHNL_OP_DEL_ETH_ADDR = 11,
	VIRTCHNL_OP_ADD_VLAN = 12,
	VIRTCHNL_OP_DEL_VLAN = 13,
	VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE = 14,
	VIRTCHNL_OP_GET_STATS = 15,
	VIRTCHNL_OP_RSVD = 16,
	VIRTCHNL_OP_EVENT = 17, /* must ALWAYS be 17 */
	/* opcode 19 is reserved */
	/* opcodes 20, 21, and 22 are reserved */
	VIRTCHNL_OP_CONFIG_RSS_KEY = 23,
	VIRTCHNL_OP_CONFIG_RSS_LUT = 24,
	VIRTCHNL_OP_GET_RSS_HENA_CAPS = 25,
	VIRTCHNL_OP_SET_RSS_HENA = 26,
	VIRTCHNL_OP_ENABLE_VLAN_STRIPPING = 27,
	VIRTCHNL_OP_DISABLE_VLAN_STRIPPING = 28,
	VIRTCHNL_OP_REQUEST_QUEUES = 29,
	VIRTCHNL_OP_ENABLE_CHANNELS = 30,
	VIRTCHNL_OP_DISABLE_CHANNELS = 31,
	VIRTCHNL_OP_ADD_CLOUD_FILTER = 32,
	VIRTCHNL_OP_DEL_CLOUD_FILTER = 33,
	/* opcodes 34, 35, 36, 37 and 38 are reserved */
	VIRTCHNL_OP_DCF_CMD_DESC = 39,
	VIRTCHNL_OP_DCF_CMD_BUFF = 40,
	VIRTCHNL_OP_DCF_DISABLE = 41,
	VIRTCHNL_OP_DCF_GET_VSI_MAP = 42,
	VIRTCHNL_OP_DCF_GET_PKG_INFO = 43,
	VIRTCHNL_OP_GET_SUPPORTED_RXDIDS = 44,
	VIRTCHNL_OP_ADD_RSS_CFG = 45,
	VIRTCHNL_OP_DEL_RSS_CFG = 46,
	VIRTCHNL_OP_ADD_FDIR_FILTER = 47,
	VIRTCHNL_OP_DEL_FDIR_FILTER = 48,
	VIRTCHNL_OP_QUERY_FDIR_FILTER = 49,
};

/* These macros are used to generate compilation errors if a structure/union
 * is not exactly the correct length. It gives a divide by zero error if the
 * structure/union is not of the correct size, otherwise it creates an enum
 * that is never used.
 */
#define VIRTCHNL_CHECK_STRUCT_LEN(n, X) enum virtchnl_static_assert_enum_##X \
	{ virtchnl_static_assert_##X = (n)/((sizeof(struct X) == (n)) ? 1 : 0) }
#define VIRTCHNL_CHECK_UNION_LEN(n, X) enum virtchnl_static_asset_enum_##X \
	{ virtchnl_static_assert_##X = (n)/((sizeof(union X) == (n)) ? 1 : 0) }

/* Virtual channel message descriptor. This overlays the admin queue
 * descriptor. All other data is passed in external buffers.
 */

struct virtchnl_msg {
	u8 pad[8];			 /* AQ flags/opcode/len/retval fields */
	enum virtchnl_ops v_opcode; /* avoid confusion with desc->opcode */
	enum virtchnl_status_code v_retval;  /* ditto for desc->retval */
	u32 vfid;			 /* used by PF when sending to VF */
};

VIRTCHNL_CHECK_STRUCT_LEN(20, virtchnl_msg);

/* Message descriptions and data structures. */

/* VIRTCHNL_OP_VERSION
 * VF posts its version number to the PF. PF responds with its version number
 * in the same format, along with a return code.
 * Reply from PF has its major/minor versions also in param0 and param1.
 * If there is a major version mismatch, then the VF cannot operate.
 * If there is a minor version mismatch, then the VF can operate but should
 * add a warning to the system log.
 *
 * This enum element MUST always be specified as == 1, regardless of other
 * changes in the API. The PF must always respond to this message without
 * error regardless of version mismatch.
 */
#define VIRTCHNL_VERSION_MAJOR		1
#define VIRTCHNL_VERSION_MINOR		1
#define VIRTCHNL_VERSION_MINOR_NO_VF_CAPS	0

struct virtchnl_version_info {
	u32 major;
	u32 minor;
};

VIRTCHNL_CHECK_STRUCT_LEN(8, virtchnl_version_info);

#define VF_IS_V10(_v) (((_v)->major == 1) && ((_v)->minor == 0))
#define VF_IS_V11(_ver) (((_ver)->major == 1) && ((_ver)->minor == 1))

/* VIRTCHNL_OP_RESET_VF
 * VF sends this request to PF with no parameters
 * PF does NOT respond! VF driver must delay then poll VFGEN_RSTAT register
 * until reset completion is indicated. The admin queue must be reinitialized
 * after this operation.
 *
 * When reset is complete, PF must ensure that all queues in all VSIs associated
 * with the VF are stopped, all queue configurations in the HMC are set to 0,
 * and all MAC and VLAN filters (except the default MAC address) on all VSIs
 * are cleared.
 */

/* VSI types that use VIRTCHNL interface for VF-PF communication. VSI_SRIOV
 * vsi_type should always be 6 for backward compatibility. Add other fields
 * as needed.
 */
enum virtchnl_vsi_type {
	VIRTCHNL_VSI_TYPE_INVALID = 0,
	VIRTCHNL_VSI_SRIOV = 6,
};

/* VIRTCHNL_OP_GET_VF_RESOURCES
 * Version 1.0 VF sends this request to PF with no parameters
 * Version 1.1 VF sends this request to PF with u32 bitmap of its capabilities
 * PF responds with an indirect message containing
 * virtchnl_vf_resource and one or more
 * virtchnl_vsi_resource structures.
 */

struct virtchnl_vsi_resource {
	u16 vsi_id;
	u16 num_queue_pairs;
	enum virtchnl_vsi_type vsi_type;
	u16 qset_handle;
	u8 default_mac_addr[VIRTCHNL_ETH_LENGTH_OF_ADDRESS];
};

VIRTCHNL_CHECK_STRUCT_LEN(16, virtchnl_vsi_resource);

/* VF capability flags
 * VIRTCHNL_VF_OFFLOAD_L2 flag is inclusive of base mode L2 offloads including
 * TX/RX Checksum offloading and TSO for non-tunnelled packets.
 */
#define VIRTCHNL_VF_OFFLOAD_L2			0x00000001
#define VIRTCHNL_VF_OFFLOAD_IWARP		0x00000002
#define VIRTCHNL_VF_OFFLOAD_RSVD		0x00000004
#define VIRTCHNL_VF_OFFLOAD_RSS_AQ		0x00000008
#define VIRTCHNL_VF_OFFLOAD_RSS_REG		0x00000010
#define VIRTCHNL_VF_OFFLOAD_WB_ON_ITR		0x00000020
#define VIRTCHNL_VF_OFFLOAD_REQ_QUEUES		0x00000040
#define VIRTCHNL_VF_OFFLOAD_CRC			0x00000080
#define VIRTCHNL_VF_OFFLOAD_VLAN		0x00010000
#define VIRTCHNL_VF_OFFLOAD_RX_POLLING		0x00020000
#define VIRTCHNL_VF_OFFLOAD_RSS_PCTYPE_V2	0x00040000
#define VIRTCHNL_VF_OFFLOAD_RSS_PF		0X00080000
#define VIRTCHNL_VF_OFFLOAD_ENCAP		0X00100000
#define VIRTCHNL_VF_OFFLOAD_ENCAP_CSUM		0X00200000
#define VIRTCHNL_VF_OFFLOAD_RX_ENCAP_CSUM	0X00400000
#define VIRTCHNL_VF_OFFLOAD_ADQ			0X00800000
#define VIRTCHNL_VF_OFFLOAD_ADQ_V2		0X01000000
#define VIRTCHNL_VF_OFFLOAD_USO			0X02000000
#define VIRTCHNL_VF_CAP_DCF			0X40000000
#define VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC	0X04000000
#define VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF		0X08000000
#define VIRTCHNL_VF_OFFLOAD_FDIR_PF		0X10000000
	/* 0X80000000 is reserved */

/* Define below the capability flags that are not offloads */
#define VIRTCHNL_VF_CAP_ADV_LINK_SPEED		0x00000080
#define VF_BASE_MODE_OFFLOADS (VIRTCHNL_VF_OFFLOAD_L2 | \
			       VIRTCHNL_VF_OFFLOAD_VLAN | \
			       VIRTCHNL_VF_OFFLOAD_RSS_PF)

struct virtchnl_vf_resource {
	u16 num_vsis;
	u16 num_queue_pairs;
	u16 max_vectors;
	u16 max_mtu;

	u32 vf_cap_flags;
	u32 rss_key_size;
	u32 rss_lut_size;

	struct virtchnl_vsi_resource vsi_res[1];
};

VIRTCHNL_CHECK_STRUCT_LEN(36, virtchnl_vf_resource);

/* VIRTCHNL_OP_CONFIG_TX_QUEUE
 * VF sends this message to set up parameters for one TX queue.
 * External data buffer contains one instance of virtchnl_txq_info.
 * PF configures requested queue and returns a status code.
 */

/* Tx queue config info */
struct virtchnl_txq_info {
	u16 vsi_id;
	u16 queue_id;
	u16 ring_len;		/* number of descriptors, multiple of 8 */
	u16 headwb_enabled; /* deprecated with AVF 1.0 */
	u64 dma_ring_addr;
	u64 dma_headwb_addr; /* deprecated with AVF 1.0 */
};

VIRTCHNL_CHECK_STRUCT_LEN(24, virtchnl_txq_info);

/* VIRTCHNL_OP_CONFIG_RX_QUEUE
 * VF sends this message to set up parameters for one RX queue.
 * External data buffer contains one instance of virtchnl_rxq_info.
 * PF configures requested queue and returns a status code. The
 * crc_disable flag disables CRC stripping on the VF. Setting
 * the crc_disable flag to 1 will disable CRC stripping for each
 * queue in the VF where the flag is set. The VIRTCHNL_VF_OFFLOAD_CRC
 * offload must have been set prior to sending this info or the PF
 * will ignore the request. This flag should be set the same for
 * all of the queues for a VF.
 */

/* Rx queue config info */
struct virtchnl_rxq_info {
	u16 vsi_id;
	u16 queue_id;
	u32 ring_len;		/* number of descriptors, multiple of 32 */
	u16 hdr_size;
	u16 splithdr_enabled; /* deprecated with AVF 1.0 */
	u32 databuffer_size;
	u32 max_pkt_size;
	u8 crc_disable;
	/* only used when VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC is supported */
	u8 rxdid;
	u8 pad1[2];
	u64 dma_ring_addr;
	enum virtchnl_rx_hsplit rx_split_pos; /* deprecated with AVF 1.0 */
	u32 pad2;
};

VIRTCHNL_CHECK_STRUCT_LEN(40, virtchnl_rxq_info);

/* VIRTCHNL_OP_CONFIG_VSI_QUEUES
 * VF sends this message to set parameters for active TX and RX queues
 * associated with the specified VSI.
 * PF configures queues and returns status.
 * If the number of queues specified is greater than the number of queues
 * associated with the VSI, an error is returned and no queues are configured.
 * NOTE: The VF is not required to configure all queues in a single request.
 * It may send multiple messages. PF drivers must correctly handle all VF
 * requests.
 */
struct virtchnl_queue_pair_info {
	/* NOTE: vsi_id and queue_id should be identical for both queues. */
	struct virtchnl_txq_info txq;
	struct virtchnl_rxq_info rxq;
};

VIRTCHNL_CHECK_STRUCT_LEN(64, virtchnl_queue_pair_info);

struct virtchnl_vsi_queue_config_info {
	u16 vsi_id;
	u16 num_queue_pairs;
	u32 pad;
	struct virtchnl_queue_pair_info qpair[1];
};

VIRTCHNL_CHECK_STRUCT_LEN(72, virtchnl_vsi_queue_config_info);

/* VIRTCHNL_OP_REQUEST_QUEUES
 * VF sends this message to request the PF to allocate additional queues to
 * this VF.  Each VF gets a guaranteed number of queues on init but asking for
 * additional queues must be negotiated.  This is a best effort request as it
 * is possible the PF does not have enough queues left to support the request.
 * If the PF cannot support the number requested it will respond with the
 * maximum number it is able to support.  If the request is successful, PF will
 * then reset the VF to institute required changes.
 */

/* VF resource request */
struct virtchnl_vf_res_request {
	u16 num_queue_pairs;
};

/* VIRTCHNL_OP_CONFIG_IRQ_MAP
 * VF uses this message to map vectors to queues.
 * The rxq_map and txq_map fields are bitmaps used to indicate which queues
 * are to be associated with the specified vector.
 * The "other" causes are always mapped to vector 0. The VF may not request
 * that vector 0 be used for traffic.
 * PF configures interrupt mapping and returns status.
 * NOTE: due to hardware requirements, all active queues (both TX and RX)
 * should be mapped to interrupts, even if the driver intends to operate
 * only in polling mode. In this case the interrupt may be disabled, but
 * the ITR timer will still run to trigger writebacks.
 */
struct virtchnl_vector_map {
	u16 vsi_id;
	u16 vector_id;
	u16 rxq_map;
	u16 txq_map;
	u16 rxitr_idx;
	u16 txitr_idx;
};

VIRTCHNL_CHECK_STRUCT_LEN(12, virtchnl_vector_map);

struct virtchnl_irq_map_info {
	u16 num_vectors;
	struct virtchnl_vector_map vecmap[1];
};

VIRTCHNL_CHECK_STRUCT_LEN(14, virtchnl_irq_map_info);

/* VIRTCHNL_OP_ENABLE_QUEUES
 * VIRTCHNL_OP_DISABLE_QUEUES
 * VF sends these message to enable or disable TX/RX queue pairs.
 * The queues fields are bitmaps indicating which queues to act upon.
 * (Currently, we only support 16 queues per VF, but we make the field
 * u32 to allow for expansion.)
 * PF performs requested action and returns status.
 * NOTE: The VF is not required to enable/disable all queues in a single
 * request. It may send multiple messages.
 * PF drivers must correctly handle all VF requests.
 */
struct virtchnl_queue_select {
	u16 vsi_id;
	u16 pad;
	u32 rx_queues;
	u32 tx_queues;
};

VIRTCHNL_CHECK_STRUCT_LEN(12, virtchnl_queue_select);

/* VIRTCHNL_OP_ADD_ETH_ADDR
 * VF sends this message in order to add one or more unicast or multicast
 * address filters for the specified VSI.
 * PF adds the filters and returns status.
 */

/* VIRTCHNL_OP_DEL_ETH_ADDR
 * VF sends this message in order to remove one or more unicast or multicast
 * filters for the specified VSI.
 * PF removes the filters and returns status.
 */

struct virtchnl_ether_addr {
	u8 addr[VIRTCHNL_ETH_LENGTH_OF_ADDRESS];
	u8 pad[2];
};

VIRTCHNL_CHECK_STRUCT_LEN(8, virtchnl_ether_addr);

struct virtchnl_ether_addr_list {
	u16 vsi_id;
	u16 num_elements;
	struct virtchnl_ether_addr list[1];
};

VIRTCHNL_CHECK_STRUCT_LEN(12, virtchnl_ether_addr_list);

/* VIRTCHNL_OP_ADD_VLAN
 * VF sends this message to add one or more VLAN tag filters for receives.
 * PF adds the filters and returns status.
 * If a port VLAN is configured by the PF, this operation will return an
 * error to the VF.
 */

/* VIRTCHNL_OP_DEL_VLAN
 * VF sends this message to remove one or more VLAN tag filters for receives.
 * PF removes the filters and returns status.
 * If a port VLAN is configured by the PF, this operation will return an
 * error to the VF.
 */

struct virtchnl_vlan_filter_list {
	u16 vsi_id;
	u16 num_elements;
	u16 vlan_id[1];
};

VIRTCHNL_CHECK_STRUCT_LEN(6, virtchnl_vlan_filter_list);

/* VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE
 * VF sends VSI id and flags.
 * PF returns status code in retval.
 * Note: we assume that broadcast accept mode is always enabled.
 */
struct virtchnl_promisc_info {
	u16 vsi_id;
	u16 flags;
};

VIRTCHNL_CHECK_STRUCT_LEN(4, virtchnl_promisc_info);

#define FLAG_VF_UNICAST_PROMISC	0x00000001
#define FLAG_VF_MULTICAST_PROMISC	0x00000002

/* VIRTCHNL_OP_GET_STATS
 * VF sends this message to request stats for the selected VSI. VF uses
 * the virtchnl_queue_select struct to specify the VSI. The queue_id
 * field is ignored by the PF.
 *
 * PF replies with struct virtchnl_eth_stats in an external buffer.
 */

struct virtchnl_eth_stats {
	u64 rx_bytes;			/* received bytes */
	u64 rx_unicast;			/* received unicast pkts */
	u64 rx_multicast;		/* received multicast pkts */
	u64 rx_broadcast;		/* received broadcast pkts */
	u64 rx_discards;
	u64 rx_unknown_protocol;
	u64 tx_bytes;			/* transmitted bytes */
	u64 tx_unicast;			/* transmitted unicast pkts */
	u64 tx_multicast;		/* transmitted multicast pkts */
	u64 tx_broadcast;		/* transmitted broadcast pkts */
	u64 tx_discards;
	u64 tx_errors;
};

/* VIRTCHNL_OP_CONFIG_RSS_KEY
 * VIRTCHNL_OP_CONFIG_RSS_LUT
 * VF sends these messages to configure RSS. Only supported if both PF
 * and VF drivers set the VIRTCHNL_VF_OFFLOAD_RSS_PF bit during
 * configuration negotiation. If this is the case, then the RSS fields in
 * the VF resource struct are valid.
 * Both the key and LUT are initialized to 0 by the PF, meaning that
 * RSS is effectively disabled until set up by the VF.
 */
struct virtchnl_rss_key {
	u16 vsi_id;
	u16 key_len;
	u8 key[1];         /* RSS hash key, packed bytes */
};

VIRTCHNL_CHECK_STRUCT_LEN(6, virtchnl_rss_key);

struct virtchnl_rss_lut {
	u16 vsi_id;
	u16 lut_entries;
	u8 lut[1];        /* RSS lookup table */
};

VIRTCHNL_CHECK_STRUCT_LEN(6, virtchnl_rss_lut);

/* VIRTCHNL_OP_GET_RSS_HENA_CAPS
 * VIRTCHNL_OP_SET_RSS_HENA
 * VF sends these messages to get and set the hash filter enable bits for RSS.
 * By default, the PF sets these to all possible traffic types that the
 * hardware supports. The VF can query this value if it wants to change the
 * traffic types that are hashed by the hardware.
 */
struct virtchnl_rss_hena {
	u64 hena;
};

VIRTCHNL_CHECK_STRUCT_LEN(8, virtchnl_rss_hena);

/* Type of RSS algorithm */
enum virtchnl_rss_algorithm {
	VIRTCHNL_RSS_ALG_TOEPLITZ_ASYMMETRIC	= 0,
	VIRTCHNL_RSS_ALG_XOR_ASYMMETRIC		= 1,
	VIRTCHNL_RSS_ALG_TOEPLITZ_SYMMETRIC	= 2,
	VIRTCHNL_RSS_ALG_XOR_SYMMETRIC		= 3,
};

/* This is used by PF driver to enforce how many channels can be supported.
 * When ADQ_V2 capability is negotiated, it will allow 16 channels otherwise
 * PF driver will allow only max 4 channels
 */
#define VIRTCHNL_MAX_ADQ_CHANNELS 4
#define VIRTCHNL_MAX_ADQ_V2_CHANNELS 16

/* VIRTCHNL_OP_ENABLE_CHANNELS
 * VIRTCHNL_OP_DISABLE_CHANNELS
 * VF sends these messages to enable or disable channels based on
 * the user specified queue count and queue offset for each traffic class.
 * This struct encompasses all the information that the PF needs from
 * VF to create a channel.
 */
struct virtchnl_channel_info {
	u16 count; /* number of queues in a channel */
	u16 offset; /* queues in a channel start from 'offset' */
	u32 pad;
	u64 max_tx_rate;
};

VIRTCHNL_CHECK_STRUCT_LEN(16, virtchnl_channel_info);

struct virtchnl_tc_info {
	u32	num_tc;
	u32	pad;
	struct	virtchnl_channel_info list[1];
};

VIRTCHNL_CHECK_STRUCT_LEN(24, virtchnl_tc_info);

/* VIRTCHNL_ADD_CLOUD_FILTER
 * VIRTCHNL_DEL_CLOUD_FILTER
 * VF sends these messages to add or delete a cloud filter based on the
 * user specified match and action filters. These structures encompass
 * all the information that the PF needs from the VF to add/delete a
 * cloud filter.
 */

struct virtchnl_l4_spec {
	u8	src_mac[ETH_ALEN];
	u8	dst_mac[ETH_ALEN];
	/* vlan_prio is part of this 16 bit field even from OS perspective
	 * vlan_id:12 is actual vlan_id, then vlanid:bit14..12 is vlan_prio
	 * in future, when decided to offload vlan_prio, pass that information
	 * as part of the "vlan_id" field, Bit14..12
	 */
	__be16	vlan_id;
	__be16	pad; /* reserved for future use */
	__be32	src_ip[4];
	__be32	dst_ip[4];
	__be16	src_port;
	__be16	dst_port;
};

VIRTCHNL_CHECK_STRUCT_LEN(52, virtchnl_l4_spec);

union virtchnl_flow_spec {
	struct	virtchnl_l4_spec tcp_spec;
	u8	buffer[128]; /* reserved for future use */
};

VIRTCHNL_CHECK_UNION_LEN(128, virtchnl_flow_spec);

enum virtchnl_action {
	/* action types */
	VIRTCHNL_ACTION_DROP = 0,
	VIRTCHNL_ACTION_TC_REDIRECT,
	VIRTCHNL_ACTION_PASSTHRU,
	VIRTCHNL_ACTION_QUEUE,
	VIRTCHNL_ACTION_Q_REGION,
	VIRTCHNL_ACTION_MARK,
	VIRTCHNL_ACTION_COUNT,
};

enum virtchnl_flow_type {
	/* flow types */
	VIRTCHNL_TCP_V4_FLOW = 0,
	VIRTCHNL_TCP_V6_FLOW,
	VIRTCHNL_UDP_V4_FLOW,
	VIRTCHNL_UDP_V6_FLOW,
};

struct virtchnl_filter {
	union	virtchnl_flow_spec data;
	union	virtchnl_flow_spec mask;
	enum	virtchnl_flow_type flow_type;
	enum	virtchnl_action action;
	u32	action_meta;
	u8	field_flags;
};

VIRTCHNL_CHECK_STRUCT_LEN(272, virtchnl_filter);

/* VIRTCHNL_OP_DCF_GET_VSI_MAP
 * VF sends this message to get VSI mapping table.
 * PF responds with an indirect message containing VF's
 * HW VSI IDs.
 * The index of vf_vsi array is the logical VF ID, the
 * value of vf_vsi array is the VF's HW VSI ID with its
 * valid configuration.
 */
struct virtchnl_dcf_vsi_map {
	u16 pf_vsi;	/* PF's HW VSI ID */
	u16 num_vfs;	/* The actual number of VFs allocated */
#define VIRTCHNL_DCF_VF_VSI_ID_S	0
#define VIRTCHNL_DCF_VF_VSI_ID_M	(0xFFF << VIRTCHNL_DCF_VF_VSI_ID_S)
#define VIRTCHNL_DCF_VF_VSI_VALID	(1 << 15)
	u16 vf_vsi[1];
};

VIRTCHNL_CHECK_STRUCT_LEN(6, virtchnl_dcf_vsi_map);

#define PKG_NAME_SIZE	32
#define DSN_SIZE	8

struct pkg_version {
	u8 major;
	u8 minor;
	u8 update;
	u8 draft;
};

VIRTCHNL_CHECK_STRUCT_LEN(4, pkg_version);

struct virtchnl_pkg_info {
	struct pkg_version pkg_ver;
	u32 track_id;
	char pkg_name[PKG_NAME_SIZE];
	u8 dsn[DSN_SIZE];
};

VIRTCHNL_CHECK_STRUCT_LEN(48, virtchnl_pkg_info);

struct virtchnl_supported_rxdids {
	u64 supported_rxdids;
};

VIRTCHNL_CHECK_STRUCT_LEN(8, virtchnl_supported_rxdids);

/* VIRTCHNL_OP_EVENT
 * PF sends this message to inform the VF driver of events that may affect it.
 * No direct response is expected from the VF, though it may generate other
 * messages in response to this one.
 */
enum virtchnl_event_codes {
	VIRTCHNL_EVENT_UNKNOWN = 0,
	VIRTCHNL_EVENT_LINK_CHANGE,
	VIRTCHNL_EVENT_RESET_IMPENDING,
	VIRTCHNL_EVENT_PF_DRIVER_CLOSE,
	VIRTCHNL_EVENT_DCF_VSI_MAP_UPDATE,
};

#define PF_EVENT_SEVERITY_INFO		0
#define PF_EVENT_SEVERITY_ATTENTION	1
#define PF_EVENT_SEVERITY_ACTION_REQUIRED	2
#define PF_EVENT_SEVERITY_CERTAIN_DOOM	255

struct virtchnl_pf_event {
	enum virtchnl_event_codes event;
	union {
		/* If the PF driver does not support the new speed reporting
		 * capabilities then use link_event else use link_event_adv to
		 * get the speed and link information. The ability to understand
		 * new speeds is indicated by setting the capability flag
		 * VIRTCHNL_VF_CAP_ADV_LINK_SPEED in vf_cap_flags parameter
		 * in virtchnl_vf_resource struct and can be used to determine
		 * which link event struct to use below.
		 */
		struct {
			enum virtchnl_link_speed link_speed;
			u8 link_status;
		} link_event;
		struct {
			/* link_speed provided in Mbps */
			u32 link_speed;
			u8 link_status;
		} link_event_adv;
		struct {
			u16 vf_id;
			u16 vsi_id;
		} vf_vsi_map;
	} event_data;

	int severity;
};

VIRTCHNL_CHECK_STRUCT_LEN(16, virtchnl_pf_event);


/* Since VF messages are limited by u16 size, precalculate the maximum possible
 * values of nested elements in virtchnl structures that virtual channel can
 * possibly handle in a single message.
 */
enum virtchnl_vector_limits {
	VIRTCHNL_OP_CONFIG_VSI_QUEUES_MAX	=
		((u16)(~0) - sizeof(struct virtchnl_vsi_queue_config_info)) /
		sizeof(struct virtchnl_queue_pair_info),

	VIRTCHNL_OP_CONFIG_IRQ_MAP_MAX		=
		((u16)(~0) - sizeof(struct virtchnl_irq_map_info)) /
		sizeof(struct virtchnl_vector_map),

	VIRTCHNL_OP_ADD_DEL_ETH_ADDR_MAX	=
		((u16)(~0) - sizeof(struct virtchnl_ether_addr_list)) /
		sizeof(struct virtchnl_ether_addr),

	VIRTCHNL_OP_ADD_DEL_VLAN_MAX		=
		((u16)(~0) - sizeof(struct virtchnl_vlan_filter_list)) /
		sizeof(u16),


	VIRTCHNL_OP_ENABLE_CHANNELS_MAX		=
		((u16)(~0) - sizeof(struct virtchnl_tc_info)) /
		sizeof(struct virtchnl_channel_info),
};

/* VF reset states - these are written into the RSTAT register:
 * VFGEN_RSTAT on the VF
 * When the PF initiates a reset, it writes 0
 * When the reset is complete, it writes 1
 * When the PF detects that the VF has recovered, it writes 2
 * VF checks this register periodically to determine if a reset has occurred,
 * then polls it to know when the reset is complete.
 * If either the PF or VF reads the register while the hardware
 * is in a reset state, it will return DEADBEEF, which, when masked
 * will result in 3.
 */
enum virtchnl_vfr_states {
	VIRTCHNL_VFR_INPROGRESS = 0,
	VIRTCHNL_VFR_COMPLETED,
	VIRTCHNL_VFR_VFACTIVE,
};

#define VIRTCHNL_MAX_NUM_PROTO_HDRS	32
#define PROTO_HDR_SHIFT			5
#define PROTO_HDR_FIELD_START(proto_hdr_type) \
					(proto_hdr_type << PROTO_HDR_SHIFT)
#define PROTO_HDR_FIELD_MASK ((1UL << PROTO_HDR_SHIFT) - 1)

/* VF use these macros to configure each protocol header.
 * Specify which protocol headers and protocol header fields base on
 * virtchnl_proto_hdr_type and virtchnl_proto_hdr_field.
 * @param hdr: a struct of virtchnl_proto_hdr
 * @param hdr_type: ETH/IPV4/TCP, etc
 * @param field: SRC/DST/TEID/SPI, etc
 */
#define VIRTCHNL_ADD_PROTO_HDR_FIELD(hdr, field) \
	((hdr)->field_selector |= BIT((field) & PROTO_HDR_FIELD_MASK))
#define VIRTCHNL_DEL_PROTO_HDR_FIELD(hdr, field) \
	((hdr)->field_selector &= ~BIT((field) & PROTO_HDR_FIELD_MASK))
#define VIRTCHNL_TEST_PROTO_HDR_FIELD(hdr, val) \
	((hdr)->field_selector & BIT((val) & PROTO_HDR_FIELD_MASK))
#define VIRTCHNL_GET_PROTO_HDR_FIELD(hdr)	((hdr)->field_selector)

#define VIRTCHNL_ADD_PROTO_HDR_FIELD_BIT(hdr, hdr_type, field) \
	(VIRTCHNL_ADD_PROTO_HDR_FIELD(hdr, \
		VIRTCHNL_PROTO_HDR_ ## hdr_type ## _ ## field))
#define VIRTCHNL_DEL_PROTO_HDR_FIELD_BIT(hdr, hdr_type, field) \
	(VIRTCHNL_DEL_PROTO_HDR_FIELD(hdr, \
		VIRTCHNL_PROTO_HDR_ ## hdr_type ## _ ## field))

#define VIRTCHNL_SET_PROTO_HDR_TYPE(hdr, hdr_type) \
	((hdr)->type = VIRTCHNL_PROTO_HDR_ ## hdr_type)
#define VIRTCHNL_GET_PROTO_HDR_TYPE(hdr) \
	(((hdr)->type) >> PROTO_HDR_SHIFT)
#define VIRTCHNL_TEST_PROTO_HDR_TYPE(hdr, val) \
	((hdr)->type == ((val) >> PROTO_HDR_SHIFT))
#define VIRTCHNL_TEST_PROTO_HDR(hdr, val) \
	(VIRTCHNL_TEST_PROTO_HDR_TYPE(hdr, val) && \
	 VIRTCHNL_TEST_PROTO_HDR_FIELD(hdr, val))

/* Protocol header type within a packet segment. A segment consists of one or
 * more protocol headers that make up a logical group of protocol headers. Each
 * logical group of protocol headers encapsulates or is encapsulated using/by
 * tunneling or encapsulation protocols for network virtualization.
 */
enum virtchnl_proto_hdr_type {
	VIRTCHNL_PROTO_HDR_NONE,
	VIRTCHNL_PROTO_HDR_ETH,
	VIRTCHNL_PROTO_HDR_S_VLAN,
	VIRTCHNL_PROTO_HDR_C_VLAN,
	VIRTCHNL_PROTO_HDR_IPV4,
	VIRTCHNL_PROTO_HDR_IPV6,
	VIRTCHNL_PROTO_HDR_TCP,
	VIRTCHNL_PROTO_HDR_UDP,
	VIRTCHNL_PROTO_HDR_SCTP,
	VIRTCHNL_PROTO_HDR_GTPU_IP,
	VIRTCHNL_PROTO_HDR_GTPU_EH,
	VIRTCHNL_PROTO_HDR_GTPU_EH_PDU_DWN,
	VIRTCHNL_PROTO_HDR_GTPU_EH_PDU_UP,
	VIRTCHNL_PROTO_HDR_PPPOE,
	VIRTCHNL_PROTO_HDR_L2TPV3,
	VIRTCHNL_PROTO_HDR_ESP,
	VIRTCHNL_PROTO_HDR_AH,
	VIRTCHNL_PROTO_HDR_PFCP,
};

/* Protocol header field within a protocol header. */
enum virtchnl_proto_hdr_field {
	/* ETHER */
	VIRTCHNL_PROTO_HDR_ETH_SRC =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_ETH),
	VIRTCHNL_PROTO_HDR_ETH_DST,
	VIRTCHNL_PROTO_HDR_ETH_ETHERTYPE,
	/* S-VLAN */
	VIRTCHNL_PROTO_HDR_S_VLAN_ID =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_S_VLAN),
	/* C-VLAN */
	VIRTCHNL_PROTO_HDR_C_VLAN_ID =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_C_VLAN),
	/* IPV4 */
	VIRTCHNL_PROTO_HDR_IPV4_SRC =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_IPV4),
	VIRTCHNL_PROTO_HDR_IPV4_DST,
	VIRTCHNL_PROTO_HDR_IPV4_DSCP,
	VIRTCHNL_PROTO_HDR_IPV4_TTL,
	VIRTCHNL_PROTO_HDR_IPV4_PROT,
	/* IPV6 */
	VIRTCHNL_PROTO_HDR_IPV6_SRC =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_IPV6),
	VIRTCHNL_PROTO_HDR_IPV6_DST,
	VIRTCHNL_PROTO_HDR_IPV6_TC,
	VIRTCHNL_PROTO_HDR_IPV6_HOP_LIMIT,
	VIRTCHNL_PROTO_HDR_IPV6_PROT,
	/* TCP */
	VIRTCHNL_PROTO_HDR_TCP_SRC_PORT =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_TCP),
	VIRTCHNL_PROTO_HDR_TCP_DST_PORT,
	/* UDP */
	VIRTCHNL_PROTO_HDR_UDP_SRC_PORT =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_UDP),
	VIRTCHNL_PROTO_HDR_UDP_DST_PORT,
	/* SCTP */
	VIRTCHNL_PROTO_HDR_SCTP_SRC_PORT =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_SCTP),
	VIRTCHNL_PROTO_HDR_SCTP_DST_PORT,
	/* GTPU_IP */
	VIRTCHNL_PROTO_HDR_GTPU_IP_TEID =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_GTPU_IP),
	/* GTPU_EH */
	VIRTCHNL_PROTO_HDR_GTPU_EH_PDU =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_GTPU_EH),
	VIRTCHNL_PROTO_HDR_GTPU_EH_QFI,
	/* PPPOE */
	VIRTCHNL_PROTO_HDR_PPPOE_SESS_ID =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_PPPOE),
	/* L2TPV3 */
	VIRTCHNL_PROTO_HDR_L2TPV3_SESS_ID =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_L2TPV3),
	/* ESP */
	VIRTCHNL_PROTO_HDR_ESP_SPI =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_ESP),
	/* AH */
	VIRTCHNL_PROTO_HDR_AH_SPI =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_AH),
	/* PFCP */
	VIRTCHNL_PROTO_HDR_PFCP_S_FIELD =
		PROTO_HDR_FIELD_START(VIRTCHNL_PROTO_HDR_PFCP),
	VIRTCHNL_PROTO_HDR_PFCP_SEID,
};

struct virtchnl_proto_hdr {
	enum virtchnl_proto_hdr_type type;
	u32 field_selector; /* a bit mask to select field for header type */
	u8 buffer[64];
	/**
	 * binary buffer in network order for specific header type.
	 * For example, if type = VIRTCHNL_PROTO_HDR_IPV4, a IPv4
	 * header is expected to be copied into the buffer.
	 */
};

VIRTCHNL_CHECK_STRUCT_LEN(72, virtchnl_proto_hdr);

struct virtchnl_proto_hdrs {
	u8 tunnel_level;
	/**
	 * specify where protocol header start from.
	 * 0 - from the outer layer
	 * 1 - from the first inner layer
	 * 2 - from the second inner layer
	 * ....
	 **/
	int count; /* the proto layers must < VIRTCHNL_MAX_NUM_PROTO_HDRS */
	struct virtchnl_proto_hdr proto_hdr[VIRTCHNL_MAX_NUM_PROTO_HDRS];
};

VIRTCHNL_CHECK_STRUCT_LEN(2312, virtchnl_proto_hdrs);

struct virtchnl_rss_cfg {
	struct virtchnl_proto_hdrs proto_hdrs;	   /* protocol headers */
	enum virtchnl_rss_algorithm rss_algorithm; /* rss algorithm type */
	u8 reserved[128];                          /* reserve for future */
};

VIRTCHNL_CHECK_STRUCT_LEN(2444, virtchnl_rss_cfg);

/* action configuration for FDIR */
struct virtchnl_filter_action {
	enum virtchnl_action type;
	union {
		/* used for queue and qgroup action */
		struct {
			u16 index;
			u8 region;
		} queue;
		/* used for count action */
		struct {
			/* share counter ID with other flow rules */
			u8 shared;
			u32 id; /* counter ID */
		} count;
		/* used for mark action */
		u32 mark_id;
		u8 reserve[32];
	} act_conf;
};

VIRTCHNL_CHECK_STRUCT_LEN(36, virtchnl_filter_action);

#define VIRTCHNL_MAX_NUM_ACTIONS  8

struct virtchnl_filter_action_set {
	/* action number must be less then VIRTCHNL_MAX_NUM_ACTIONS */
	int count;
	struct virtchnl_filter_action actions[VIRTCHNL_MAX_NUM_ACTIONS];
};

VIRTCHNL_CHECK_STRUCT_LEN(292, virtchnl_filter_action_set);

/* pattern and action for FDIR rule */
struct virtchnl_fdir_rule {
	struct virtchnl_proto_hdrs proto_hdrs;
	struct virtchnl_filter_action_set action_set;
};

VIRTCHNL_CHECK_STRUCT_LEN(2604, virtchnl_fdir_rule);

/* query information to retrieve fdir rule counters.
 * PF will fill out this structure to reset counter.
 */
struct virtchnl_fdir_query_info {
	u32 match_packets_valid:1;
	u32 match_bytes_valid:1;
	u32 reserved:30;  /* Reserved, must be zero. */
	u32 pad;
	u64 matched_packets; /* Number of packets for this rule. */
	u64 matched_bytes;   /* Number of bytes through this rule. */
};

VIRTCHNL_CHECK_STRUCT_LEN(24, virtchnl_fdir_query_info);

/* Status returned to VF after VF requests FDIR commands
 * VIRTCHNL_FDIR_SUCCESS
 * VF FDIR related request is successfully done by PF
 * The request can be OP_ADD/DEL/QUERY_FDIR_FILTER.
 *
 * VIRTCHNL_FDIR_FAILURE_RULE_NORESOURCE
 * OP_ADD_FDIR_FILTER request is failed due to no Hardware resource.
 *
 * VIRTCHNL_FDIR_FAILURE_RULE_EXIST
 * OP_ADD_FDIR_FILTER request is failed due to the rule is already existed.
 *
 * VIRTCHNL_FDIR_FAILURE_RULE_CONFLICT
 * OP_ADD_FDIR_FILTER request is failed due to conflict with existing rule.
 *
 * VIRTCHNL_FDIR_FAILURE_RULE_NONEXIST
 * OP_DEL_FDIR_FILTER request is failed due to this rule doesn't exist.
 *
 * VIRTCHNL_FDIR_FAILURE_RULE_INVALID
 * OP_ADD_FDIR_FILTER request is failed due to parameters validation
 * or HW doesn't support.
 *
 * VIRTCHNL_FDIR_FAILURE_RULE_TIMEOUT
 * OP_ADD/DEL_FDIR_FILTER request is failed due to timing out
 * for programming.
 *
 * VIRTCHNL_FDIR_FAILURE_QUERY_INVALID
 * OP_QUERY_FDIR_FILTER request is failed due to parameters validation,
 * for example, VF query counter of a rule who has no counter action.
 */
enum virtchnl_fdir_prgm_status {
	VIRTCHNL_FDIR_SUCCESS = 0,
	VIRTCHNL_FDIR_FAILURE_RULE_NORESOURCE,
	VIRTCHNL_FDIR_FAILURE_RULE_EXIST,
	VIRTCHNL_FDIR_FAILURE_RULE_CONFLICT,
	VIRTCHNL_FDIR_FAILURE_RULE_NONEXIST,
	VIRTCHNL_FDIR_FAILURE_RULE_INVALID,
	VIRTCHNL_FDIR_FAILURE_RULE_TIMEOUT,
	VIRTCHNL_FDIR_FAILURE_QUERY_INVALID,
};

/* VIRTCHNL_OP_ADD_FDIR_FILTER
 * VF sends this request to PF by filling out vsi_id,
 * validate_only and rule_cfg. PF will return flow_id
 * if the request is successfully done and return add_status to VF.
 */
struct virtchnl_fdir_add {
	u16 vsi_id;  /* INPUT */
	/*
	 * 1 for validating a fdir rule, 0 for creating a fdir rule.
	 * Validate and create share one ops: VIRTCHNL_OP_ADD_FDIR_FILTER.
	 */
	u16 validate_only; /* INPUT */
	u32 flow_id;       /* OUTPUT */
	struct virtchnl_fdir_rule rule_cfg; /* INPUT */
	enum virtchnl_fdir_prgm_status status; /* OUTPUT */
};

VIRTCHNL_CHECK_STRUCT_LEN(2616, virtchnl_fdir_add);

/* VIRTCHNL_OP_DEL_FDIR_FILTER
 * VF sends this request to PF by filling out vsi_id
 * and flow_id. PF will return del_status to VF.
 */
struct virtchnl_fdir_del {
	u16 vsi_id;  /* INPUT */
	u16 pad;
	u32 flow_id; /* INPUT */
	enum virtchnl_fdir_prgm_status status; /* OUTPUT */
};

VIRTCHNL_CHECK_STRUCT_LEN(12, virtchnl_fdir_del);

/* VIRTCHNL_OP_QUERY_FDIR_FILTER
 * VF sends this request to PF by filling out vsi_id,
 * flow_id and reset_counter. PF will return query_info
 * and query_status to VF.
 */
struct virtchnl_fdir_query {
	u16 vsi_id;   /* INPUT */
	u16 pad1[3];
	u32 flow_id;  /* INPUT */
	u32 reset_counter:1; /* INPUT */
	struct virtchnl_fdir_query_info query_info; /* OUTPUT */
	enum virtchnl_fdir_prgm_status status;  /* OUTPUT */
	u32 pad2;
};

VIRTCHNL_CHECK_STRUCT_LEN(48, virtchnl_fdir_query);
/**
 * virtchnl_vc_validate_vf_msg
 * @ver: Virtchnl version info
 * @v_opcode: Opcode for the message
 * @msg: pointer to the msg buffer
 * @msglen: msg length
 *
 * validate msg format against struct for each opcode
 */
static inline int
virtchnl_vc_validate_vf_msg(struct virtchnl_version_info *ver, u32 v_opcode,
			    u8 *msg, u16 msglen)
{
	bool err_msg_format = false;
	u32 valid_len = 0;

	/* Validate message length. */
	switch (v_opcode) {
	case VIRTCHNL_OP_VERSION:
		valid_len = sizeof(struct virtchnl_version_info);
		break;
	case VIRTCHNL_OP_RESET_VF:
		break;
	case VIRTCHNL_OP_GET_VF_RESOURCES:
		if (VF_IS_V11(ver))
			valid_len = sizeof(u32);
		break;
	case VIRTCHNL_OP_CONFIG_TX_QUEUE:
		valid_len = sizeof(struct virtchnl_txq_info);
		break;
	case VIRTCHNL_OP_CONFIG_RX_QUEUE:
		valid_len = sizeof(struct virtchnl_rxq_info);
		break;
	case VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		valid_len = sizeof(struct virtchnl_vsi_queue_config_info);
		if (msglen >= valid_len) {
			struct virtchnl_vsi_queue_config_info *vqc =
			    (struct virtchnl_vsi_queue_config_info *)msg;

			if (vqc->num_queue_pairs == 0 || vqc->num_queue_pairs >
			    VIRTCHNL_OP_CONFIG_VSI_QUEUES_MAX) {
				err_msg_format = true;
				break;
			}

			valid_len += (vqc->num_queue_pairs *
				      sizeof(struct
					     virtchnl_queue_pair_info));
		}
		break;
	case VIRTCHNL_OP_CONFIG_IRQ_MAP:
		valid_len = sizeof(struct virtchnl_irq_map_info);
		if (msglen >= valid_len) {
			struct virtchnl_irq_map_info *vimi =
			    (struct virtchnl_irq_map_info *)msg;

			if (vimi->num_vectors == 0 || vimi->num_vectors >
			    VIRTCHNL_OP_CONFIG_IRQ_MAP_MAX) {
				err_msg_format = true;
				break;
			}

			valid_len += (vimi->num_vectors *
				      sizeof(struct virtchnl_vector_map));
		}
		break;
	case VIRTCHNL_OP_ENABLE_QUEUES:
	case VIRTCHNL_OP_DISABLE_QUEUES:
		valid_len = sizeof(struct virtchnl_queue_select);
		break;
	case VIRTCHNL_OP_ADD_ETH_ADDR:
	case VIRTCHNL_OP_DEL_ETH_ADDR:
		valid_len = sizeof(struct virtchnl_ether_addr_list);
		if (msglen >= valid_len) {
			struct virtchnl_ether_addr_list *veal =
			    (struct virtchnl_ether_addr_list *)msg;

			if (veal->num_elements == 0 || veal->num_elements >
			    VIRTCHNL_OP_ADD_DEL_ETH_ADDR_MAX) {
				err_msg_format = true;
				break;
			}

			valid_len += veal->num_elements *
			    sizeof(struct virtchnl_ether_addr);
		}
		break;
	case VIRTCHNL_OP_ADD_VLAN:
	case VIRTCHNL_OP_DEL_VLAN:
		valid_len = sizeof(struct virtchnl_vlan_filter_list);
		if (msglen >= valid_len) {
			struct virtchnl_vlan_filter_list *vfl =
			    (struct virtchnl_vlan_filter_list *)msg;

			if (vfl->num_elements == 0 || vfl->num_elements >
			    VIRTCHNL_OP_ADD_DEL_VLAN_MAX) {
				err_msg_format = true;
				break;
			}

			valid_len += vfl->num_elements * sizeof(u16);
		}
		break;
	case VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		valid_len = sizeof(struct virtchnl_promisc_info);
		break;
	case VIRTCHNL_OP_GET_STATS:
		valid_len = sizeof(struct virtchnl_queue_select);
		break;
	case VIRTCHNL_OP_CONFIG_RSS_KEY:
		valid_len = sizeof(struct virtchnl_rss_key);
		if (msglen >= valid_len) {
			struct virtchnl_rss_key *vrk =
				(struct virtchnl_rss_key *)msg;

			if (vrk->key_len == 0) {
				/* zero length is allowed as input */
				break;
			}

			valid_len += vrk->key_len - 1;
		}
		break;
	case VIRTCHNL_OP_CONFIG_RSS_LUT:
		valid_len = sizeof(struct virtchnl_rss_lut);
		if (msglen >= valid_len) {
			struct virtchnl_rss_lut *vrl =
				(struct virtchnl_rss_lut *)msg;

			if (vrl->lut_entries == 0) {
				/* zero entries is allowed as input */
				break;
			}

			valid_len += vrl->lut_entries - 1;
		}
		break;
	case VIRTCHNL_OP_GET_RSS_HENA_CAPS:
		break;
	case VIRTCHNL_OP_SET_RSS_HENA:
		valid_len = sizeof(struct virtchnl_rss_hena);
		break;
	case VIRTCHNL_OP_ENABLE_VLAN_STRIPPING:
	case VIRTCHNL_OP_DISABLE_VLAN_STRIPPING:
		break;
	case VIRTCHNL_OP_REQUEST_QUEUES:
		valid_len = sizeof(struct virtchnl_vf_res_request);
		break;
	case VIRTCHNL_OP_ENABLE_CHANNELS:
		valid_len = sizeof(struct virtchnl_tc_info);
		if (msglen >= valid_len) {
			struct virtchnl_tc_info *vti =
				(struct virtchnl_tc_info *)msg;

			if (vti->num_tc == 0 || vti->num_tc >
			    VIRTCHNL_OP_ENABLE_CHANNELS_MAX) {
				err_msg_format = true;
				break;
			}

			valid_len += (vti->num_tc - 1) *
				     sizeof(struct virtchnl_channel_info);
		}
		break;
	case VIRTCHNL_OP_DISABLE_CHANNELS:
		break;
	case VIRTCHNL_OP_ADD_CLOUD_FILTER:
	case VIRTCHNL_OP_DEL_CLOUD_FILTER:
		valid_len = sizeof(struct virtchnl_filter);
		break;
	case VIRTCHNL_OP_DCF_CMD_DESC:
	case VIRTCHNL_OP_DCF_CMD_BUFF:
		/* These two opcodes are specific to handle the AdminQ command,
		 * so the validation needs to be done in PF's context.
		 */
		return 0;
	case VIRTCHNL_OP_DCF_DISABLE:
	case VIRTCHNL_OP_DCF_GET_VSI_MAP:
		/* The two opcodes are required by DCF without message buffer,
		 * so the valid length keeps the default value 0.
		 */
		break;
	case VIRTCHNL_OP_DCF_GET_PKG_INFO:
		break;
	case VIRTCHNL_OP_GET_SUPPORTED_RXDIDS:
		break;
	case VIRTCHNL_OP_ADD_RSS_CFG:
	case VIRTCHNL_OP_DEL_RSS_CFG:
		valid_len = sizeof(struct virtchnl_rss_cfg);
		break;
	case VIRTCHNL_OP_ADD_FDIR_FILTER:
		valid_len = sizeof(struct virtchnl_fdir_add);
		break;
	case VIRTCHNL_OP_DEL_FDIR_FILTER:
		valid_len = sizeof(struct virtchnl_fdir_del);
		break;
	case VIRTCHNL_OP_QUERY_FDIR_FILTER:
		valid_len = sizeof(struct virtchnl_fdir_query);
		break;
	/* These are always errors coming from the VF. */
	case VIRTCHNL_OP_EVENT:
	case VIRTCHNL_OP_UNKNOWN:
	default:
		return VIRTCHNL_STATUS_ERR_PARAM;
	}
	/* few more checks */
	if (err_msg_format || valid_len != msglen)
		return VIRTCHNL_STATUS_ERR_OPCODE_MISMATCH;

	return 0;
}
#endif /* _VIRTCHNL_H_ */
