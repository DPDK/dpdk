/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018 Microsoft Corp.
 * Copyright (c) 2010 Jonathan Armani <armani@openbsd.org>
 * Copyright (c) 2010 Fabien Romano <fabien@openbsd.org>
 * Copyright (c) 2010 Michael Knudsen <mk@openbsd.org>
 * All rights reserved.
 */

#ifndef	_NET_RNDIS_H_
#define	_NET_RNDIS_H_

/* Canonical major/minor version as of 22th Aug. 2016. */
#define	RNDIS_VERSION_MAJOR		0x00000001
#define	RNDIS_VERSION_MINOR		0x00000000

#define	RNDIS_STATUS_SUCCESS		0x00000000
#define	RNDIS_STATUS_PENDING		0x00000103

#define RNDIS_STATUS_ONLINE		0x40010003
#define RNDIS_STATUS_RESET_START	0x40010004
#define RNDIS_STATUS_RESET_END		0x40010005
#define RNDIS_STATUS_RING_STATUS	0x40010006
#define RNDIS_STATUS_CLOSED		0x40010007
#define RNDIS_STATUS_WAN_LINE_UP	0x40010008
#define RNDIS_STATUS_WAN_LINE_DOWN	0x40010009
#define RNDIS_STATUS_WAN_FRAGMENT	0x4001000A
#define	RNDIS_STATUS_MEDIA_CONNECT	0x4001000B
#define	RNDIS_STATUS_MEDIA_DISCONNECT	0x4001000C
#define RNDIS_STATUS_HARDWARE_LINE_UP	0x4001000D
#define RNDIS_STATUS_HARDWARE_LINE_DOWN	0x4001000E
#define RNDIS_STATUS_INTERFACE_UP	0x4001000F
#define RNDIS_STATUS_INTERFACE_DOWN	0x40010010
#define RNDIS_STATUS_MEDIA_BUSY		0x40010011
#define	RNDIS_STATUS_MEDIA_SPECIFIC_INDICATION	0x40010012
#define RNDIS_STATUS_WW_INDICATION	RDIA_SPECIFIC_INDICATION
#define RNDIS_STATUS_LINK_SPEED_CHANGE	0x40010013
#define RNDIS_STATUS_NETWORK_CHANGE	0x40010018
#define	RNDIS_STATUS_TASK_OFFLOAD_CURRENT_CONFIG 0x40020006

#define	RNDIS_STATUS_FAILURE		0xC0000001
#define RNDIS_STATUS_RESOURCES		0xC000009A
#define	RNDIS_STATUS_NOT_SUPPORTED	0xC00000BB
#define RNDIS_STATUS_CLOSING		0xC0010002
#define RNDIS_STATUS_BAD_VERSION	0xC0010004
#define RNDIS_STATUS_BAD_CHARACTERISTICS 0xC0010005
#define RNDIS_STATUS_ADAPTER_NOT_FOUND	0xC0010006
#define RNDIS_STATUS_OPEN_FAILED	0xC0010007
#define RNDIS_STATUS_DEVICE_FAILED	0xC0010008
#define RNDIS_STATUS_MULTICAST_FULL	0xC0010009
#define RNDIS_STATUS_MULTICAST_EXISTS	0xC001000A
#define RNDIS_STATUS_MULTICAST_NOT_FOUND 0xC001000B
#define RNDIS_STATUS_REQUEST_ABORTED	0xC001000C
#define RNDIS_STATUS_RESET_IN_PROGRESS	0xC001000D
#define RNDIS_STATUS_CLOSING_INDICATING	0xC001000E
#define RNDIS_STATUS_INVALID_PACKET	0xC001000F
#define RNDIS_STATUS_OPEN_LIST_FULL	0xC0010010
#define RNDIS_STATUS_ADAPTER_NOT_READY	0xC0010011
#define RNDIS_STATUS_ADAPTER_NOT_OPEN	0xC0010012
#define RNDIS_STATUS_NOT_INDICATING	0xC0010013
#define RNDIS_STATUS_INVALID_LENGTH	0xC0010014
#define	RNDIS_STATUS_INVALID_DATA	0xC0010015
#define RNDIS_STATUS_BUFFER_TOO_SHORT	0xC0010016
#define RNDIS_STATUS_INVALID_OID	0xC0010017
#define RNDIS_STATUS_ADAPTER_REMOVED	0xC0010018
#define RNDIS_STATUS_UNSUPPORTED_MEDIA	0xC0010019
#define RNDIS_STATUS_GROUP_ADDRESS_IN_US 0xC001001A
#define RNDIS_STATUS_FILE_NOT_FOUND	0xC001001B
#define RNDIS_STATUS_ERROR_READING_FILE	0xC001001C
#define RNDIS_STATUS_ALREADY_MAPPED	0xC001001D
#define RNDIS_STATUS_RESOURCE_CONFLICT	0xC001001E
#define RNDIS_STATUS_NO_CABLE		0xC001001F

#define	OID_GEN_SUPPORTED_LIST		0x00010101
#define	OID_GEN_HARDWARE_STATUS		0x00010102
#define	OID_GEN_MEDIA_SUPPORTED		0x00010103
#define	OID_GEN_MEDIA_IN_USE		0x00010104
#define	OID_GEN_MAXIMUM_LOOKAHEAD	0x00010105
#define	OID_GEN_MAXIMUM_FRAME_SIZE	0x00010106
#define	OID_GEN_LINK_SPEED		0x00010107
#define	OID_GEN_TRANSMIT_BUFFER_SPACE	0x00010108
#define	OID_GEN_RECEIVE_BUFFER_SPACE	0x00010109
#define	OID_GEN_TRANSMIT_BLOCK_SIZE	0x0001010A
#define	OID_GEN_RECEIVE_BLOCK_SIZE	0x0001010B
#define	OID_GEN_VENDOR_ID		0x0001010C
#define	OID_GEN_VENDOR_DESCRIPTION	0x0001010D
#define	OID_GEN_CURRENT_PACKET_FILTER	0x0001010E
#define	OID_GEN_CURRENT_LOOKAHEAD	0x0001010F
#define	OID_GEN_DRIVER_VERSION		0x00010110
#define	OID_GEN_MAXIMUM_TOTAL_SIZE	0x00010111
#define	OID_GEN_PROTOCOL_OPTIONS	0x00010112
#define	OID_GEN_MAC_OPTIONS		0x00010113
#define	OID_GEN_MEDIA_CONNECT_STATUS	0x00010114
#define	OID_GEN_MAXIMUM_SEND_PACKETS	0x00010115
#define	OID_GEN_VENDOR_DRIVER_VERSION	0x00010116
#define	OID_GEN_SUPPORTED_GUIDS		0x00010117
#define	OID_GEN_NETWORK_LAYER_ADDRESSES	0x00010118
#define	OID_GEN_TRANSPORT_HEADER_OFFSET	0x00010119
#define	OID_GEN_RECEIVE_SCALE_CAPABILITIES	0x00010203
#define	OID_GEN_RECEIVE_SCALE_PARAMETERS	0x00010204
#define	OID_GEN_MACHINE_NAME		0x0001021A
#define	OID_GEN_RNDIS_CONFIG_PARAMETER	0x0001021B
#define	OID_GEN_VLAN_ID			0x0001021C

#define	OID_802_3_PERMANENT_ADDRESS	0x01010101
#define	OID_802_3_CURRENT_ADDRESS	0x01010102
#define	OID_802_3_MULTICAST_LIST	0x01010103
#define	OID_802_3_MAXIMUM_LIST_SIZE	0x01010104
#define	OID_802_3_MAC_OPTIONS		0x01010105
#define	OID_802_3_RCV_ERROR_ALIGNMENT	0x01020101
#define	OID_802_3_XMIT_ONE_COLLISION	0x01020102
#define	OID_802_3_XMIT_MORE_COLLISIONS	0x01020103
#define	OID_802_3_XMIT_DEFERRED		0x01020201
#define	OID_802_3_XMIT_MAX_COLLISIONS	0x01020202
#define	OID_802_3_RCV_OVERRUN		0x01020203
#define	OID_802_3_XMIT_UNDERRUN		0x01020204
#define	OID_802_3_XMIT_HEARTBEAT_FAILURE	0x01020205
#define	OID_802_3_XMIT_TIMES_CRS_LOST	0x01020206
#define	OID_802_3_XMIT_LATE_COLLISIONS	0x01020207

#define	OID_TCP_OFFLOAD_PARAMETERS	0xFC01020C
#define	OID_TCP_OFFLOAD_HARDWARE_CAPABILITIES	0xFC01020D

#define	RNDIS_MEDIUM_802_3		0x00000000

/* Device flags */
#define	RNDIS_DF_CONNECTIONLESS		0x00000001
#define	RNDIS_DF_CONNECTION_ORIENTED	0x00000002

/*
 * Common RNDIS message header.
 */
struct rndis_msghdr {
	uint32_t type;
	uint32_t len;
};

/*
 * RNDIS data message
 */
#define	RNDIS_PACKET_MSG		0x00000001

struct rndis_packet_msg {
	uint32_t type;
	uint32_t len;
	uint32_t dataoffset;
	uint32_t datalen;
	uint32_t oobdataoffset;
	uint32_t oobdatalen;
	uint32_t oobdataelements;
	uint32_t pktinfooffset;
	uint32_t pktinfolen;
	uint32_t vchandle;
	uint32_t reserved;
};

/*
 * Minimum value for dataoffset, oobdataoffset, and
 * pktinfooffset.
 */
#define	RNDIS_PACKET_MSG_OFFSET_MIN		\
	(sizeof(struct rndis_packet_msg) -	\
	 offsetof(struct rndis_packet_msg, dataoffset))

/* Offset from the beginning of rndis_packet_msg. */
#define	RNDIS_PACKET_MSG_OFFSET_ABS(ofs)	\
	((ofs) + offsetof(struct rndis_packet_msg, dataoffset))

#define	RNDIS_PACKET_MSG_OFFSET_ALIGN		4
#define	RNDIS_PACKET_MSG_OFFSET_ALIGNMASK	\
	(RNDIS_PACKET_MSG_OFFSET_ALIGN - 1)

/* Per-packet-info for RNDIS data message */
struct rndis_pktinfo {
	uint32_t size;
	uint32_t type;		/* NDIS_PKTINFO_TYPE_ */
	uint32_t offset;
	uint8_t data[];
};

#define	RNDIS_PKTINFO_OFFSET		\
	offsetof(struct rndis_pktinfo, data[0])
#define	RNDIS_PKTINFO_SIZE_ALIGN	4
#define	RNDIS_PKTINFO_SIZE_ALIGNMASK	(RNDIS_PKTINFO_SIZE_ALIGN - 1)

#define	NDIS_PKTINFO_TYPE_CSUM		0
#define	NDIS_PKTINFO_TYPE_IPSEC		1
#define	NDIS_PKTINFO_TYPE_LSO		2
#define	NDIS_PKTINFO_TYPE_CLASSIFY	3
/* reserved 4 */
#define	NDIS_PKTINFO_TYPE_SGLIST	5
#define	NDIS_PKTINFO_TYPE_VLAN		6
#define	NDIS_PKTINFO_TYPE_ORIG		7
#define	NDIS_PKTINFO_TYPE_PKT_CANCELID	8
#define	NDIS_PKTINFO_TYPE_ORIG_NBLIST	9
#define	NDIS_PKTINFO_TYPE_CACHE_NBLIST	10
#define	NDIS_PKTINFO_TYPE_PKT_PAD	11

/* RNDIS extension */

/* Per-packet hash info */
#define NDIS_HASH_INFO_SIZE		sizeof(uint32_t)
#define NDIS_PKTINFO_TYPE_HASHINF	NDIS_PKTINFO_TYPE_ORIG_NBLIST
/* NDIS_HASH_ */

/* Per-packet hash value */
#define NDIS_HASH_VALUE_SIZE		sizeof(uint32_t)
#define NDIS_PKTINFO_TYPE_HASHVAL	NDIS_PKTINFO_TYPE_PKT_CANCELID

/* Per-packet-info size */
#define RNDIS_PKTINFO_SIZE(dlen)	offsetof(struct rndis_pktinfo, data[dlen])

/*
 * RNDIS control messages
 */

/*
 * Common header for RNDIS completion messages.
 *
 * NOTE: It does not apply to RNDIS_RESET_CMPLT.
 */
struct rndis_comp_hdr {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t status;
};

/* Initialize the device. */
#define	RNDIS_INITIALIZE_MSG	0x00000002
#define	RNDIS_INITIALIZE_CMPLT	0x80000002

struct rndis_init_req {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t ver_major;
	uint32_t ver_minor;
	uint32_t max_xfersz;
};

struct rndis_init_comp {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t status;
	uint32_t ver_major;
	uint32_t ver_minor;
	uint32_t devflags;
	uint32_t medium;
	uint32_t pktmaxcnt;
	uint32_t pktmaxsz;
	uint32_t align;
	uint32_t aflistoffset;
	uint32_t aflistsz;
};

#define	RNDIS_INIT_COMP_SIZE_MIN	\
	offsetof(struct rndis_init_comp, aflistsz)

/* Halt the device.  No response sent. */
#define	RNDIS_HALT_MSG		0x00000003

struct rndis_halt_req {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
};

/* Send a query object. */
#define	RNDIS_QUERY_MSG		0x00000004
#define	RNDIS_QUERY_CMPLT	0x80000004

struct rndis_query_req {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t oid;
	uint32_t infobuflen;
	uint32_t infobufoffset;
	uint32_t devicevchdl;
};

#define	RNDIS_QUERY_REQ_INFOBUFOFFSET		\
	(sizeof(struct rndis_query_req) -	\
	 offsetof(struct rndis_query_req, rid))

struct rndis_query_comp {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t status;
	uint32_t infobuflen;
	uint32_t infobufoffset;
};

/* infobuf offset from the beginning of rndis_query_comp. */
#define	RNDIS_QUERY_COMP_INFOBUFOFFSET_ABS(ofs)	\
	((ofs) + offsetof(struct rndis_query_comp, rid))

/* Send a set object request. */
#define	RNDIS_SET_MSG		0x00000005
#define	RNDIS_SET_CMPLT		0x80000005

struct rndis_set_req {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t oid;
	uint32_t infobuflen;
	uint32_t infobufoffset;
	uint32_t devicevchdl;
};

#define	RNDIS_SET_REQ_INFOBUFOFFSET		\
	(sizeof(struct rndis_set_req) -		\
	 offsetof(struct rndis_set_req, rid))

struct rndis_set_comp {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t status;
};

/*
 * Parameter used by OID_GEN_RNDIS_CONFIG_PARAMETER.
 */
#define	RNDIS_SET_PARAM_NUMERIC	0x00000000
#define	RNDIS_SET_PARAM_STRING	0x00000002

struct rndis_set_parameter {
	uint32_t nameoffset;
	uint32_t namelen;
	uint32_t type;
	uint32_t valueoffset;
	uint32_t valuelen;
};

/* Perform a soft reset on the device. */
#define	RNDIS_RESET_MSG		0x00000006
#define	RNDIS_RESET_CMPLT		0x80000006

struct rndis_reset_req {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
};

struct rndis_reset_comp {
	uint32_t type;
	uint32_t len;
	uint32_t status;
	uint32_t adrreset;
};

/* 802.3 link-state or undefined message error.  Sent by device. */
#define	RNDIS_INDICATE_STATUS_MSG	0x00000007

struct rndis_status_msg {
	uint32_t type;
	uint32_t len;
	uint32_t status;
	uint32_t stbuflen;
	uint32_t stbufoffset;
	/* rndis_diag_info */
};

/* stbuf offset from the beginning of rndis_status_msg. */
#define	RNDIS_STBUFOFFSET_ABS(ofs)	\
	((ofs) + offsetof(struct rndis_status_msg, status))

/*
 * Immediately after rndis_status_msg.stbufoffset, if a control
 * message is malformatted, or a packet message contains inappropriate
 * content.
 */
struct rndis_diag_info {
	uint32_t diagstatus;
	uint32_t erroffset;
};

/* Keepalive message.  May be sent by device. */
#define	RNDIS_KEEPALIVE_MSG	0x00000008
#define	RNDIS_KEEPALIVE_CMPLT	0x80000008

struct rndis_keepalive_req {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
};

struct rndis_keepalive_comp {
	uint32_t type;
	uint32_t len;
	uint32_t rid;
	uint32_t status;
};

/* Packet filter bits used by OID_GEN_CURRENT_PACKET_FILTER */
#define	NDIS_PACKET_TYPE_NONE			0x00000000
#define	NDIS_PACKET_TYPE_DIRECTED		0x00000001
#define	NDIS_PACKET_TYPE_MULTICAST		0x00000002
#define	NDIS_PACKET_TYPE_ALL_MULTICAST		0x00000004
#define	NDIS_PACKET_TYPE_BROADCAST		0x00000008
#define	NDIS_PACKET_TYPE_SOURCE_ROUTING		0x00000010
#define	NDIS_PACKET_TYPE_PROMISCUOUS		0x00000020
#define	NDIS_PACKET_TYPE_SMT			0x00000040
#define	NDIS_PACKET_TYPE_ALL_LOCAL		0x00000080
#define	NDIS_PACKET_TYPE_GROUP			0x00001000
#define	NDIS_PACKET_TYPE_ALL_FUNCTIONAL		0x00002000
#define	NDIS_PACKET_TYPE_FUNCTIONAL		0x00004000
#define	NDIS_PACKET_TYPE_MAC_FRAME		0x00008000

#endif	/* !_NET_RNDIS_H_ */
