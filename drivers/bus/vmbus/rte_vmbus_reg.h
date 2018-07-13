/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018, Microsoft Corporation.
 * All Rights Reserved.
 */

#ifndef _VMBUS_REG_H_
#define _VMBUS_REG_H_

/*
 * Hyper-V SynIC message format.
 */
#define VMBUS_MSG_DSIZE_MAX		240
#define VMBUS_MSG_SIZE			256

struct vmbus_message {
	uint32_t	type;	/* HYPERV_MSGTYPE_ */
	uint8_t		dsize;	/* data size */
	uint8_t		flags;	/* VMBUS_MSGFLAG_ */
	uint16_t	rsvd;
	uint64_t	id;
	uint8_t		data[VMBUS_MSG_DSIZE_MAX];
} __rte_packed;

#define VMBUS_MSGFLAG_PENDING		0x01

/*
 * Hyper-V Monitor Notification Facility
 */

struct vmbus_mon_trig {
	uint32_t	pending;
	uint32_t	armed;
} __rte_packed;

#define VMBUS_MONTRIGS_MAX	4
#define VMBUS_MONTRIG_LEN	32

/*
 * Hyper-V Monitor Notification Facility
 */
struct hyperv_mon_param {
	uint32_t	connid;
	uint16_t	evtflag_ofs;
	uint16_t	rsvd;
} __rte_packed;

struct vmbus_mon_page {
	uint32_t	state;
	uint32_t	rsvd1;

	struct vmbus_mon_trig trigs[VMBUS_MONTRIGS_MAX];
	uint8_t		rsvd2[536];

	uint16_t	lat[VMBUS_MONTRIGS_MAX][VMBUS_MONTRIG_LEN];
	uint8_t		rsvd3[256];

	struct hyperv_mon_param
			param[VMBUS_MONTRIGS_MAX][VMBUS_MONTRIG_LEN];
	uint8_t		rsvd4[1984];
} __rte_packed;

/*
 * Buffer ring
 */

struct vmbus_bufring {
	volatile uint32_t windex;
	volatile uint32_t rindex;

	/*
	 * Interrupt mask {0,1}
	 *
	 * For TX bufring, host set this to 1, when it is processing
	 * the TX bufring, so that we can safely skip the TX event
	 * notification to host.
	 *
	 * For RX bufring, once this is set to 1 by us, host will not
	 * further dispatch interrupts to us, even if there are data
	 * pending on the RX bufring.  This effectively disables the
	 * interrupt of the channel to which this RX bufring is attached.
	 */
	volatile uint32_t imask;

	/*
	 * Win8 uses some of the reserved bits to implement
	 * interrupt driven flow management. On the send side
	 * we can request that the receiver interrupt the sender
	 * when the ring transitions from being full to being able
	 * to handle a message of size "pending_send_sz".
	 *
	 * Add necessary state for this enhancement.
	 */
	volatile uint32_t pending_send;
	uint32_t reserved1[12];

	union {
		struct {
			uint32_t feat_pending_send_sz:1;
		};
		uint32_t value;
	} feature_bits;

	/* Pad it to PAGE_SIZE so that data starts on page boundary */
	uint8_t	reserved2[4028];

	/*
	 * Ring data starts here + RingDataStartOffset
	 * !!! DO NOT place any fields below this !!!
	 */
	uint8_t data[0];
} __rte_packed;

/*
 * Channel packets
 */

/* Channel packet flags */
#define VMBUS_CHANPKT_TYPE_INBAND      0x0006
#define VMBUS_CHANPKT_TYPE_RXBUF       0x0007
#define VMBUS_CHANPKT_TYPE_GPA         0x0009
#define VMBUS_CHANPKT_TYPE_COMP        0x000b

#define VMBUS_CHANPKT_FLAG_NONE        0
#define VMBUS_CHANPKT_FLAG_RC          0x0001  /* report completion */

#define VMBUS_CHANPKT_SIZE_SHIFT	3
#define VMBUS_CHANPKT_SIZE_ALIGN	(1 << VMBUS_CHANPKT_SIZE_SHIFT)
#define VMBUS_CHANPKT_HLEN_MIN		\
	(sizeof(struct vmbus_chanpkt_hdr) >> VMBUS_CHANPKT_SIZE_SHIFT)

static inline uint32_t
vmbus_chanpkt_getlen(uint16_t pktlen)
{
	return (uint32_t)pktlen << VMBUS_CHANPKT_SIZE_SHIFT;
}

/*
 * GPA stuffs.
 */
struct vmbus_gpa_range {
	uint32_t       len;
	uint32_t       ofs;
	uint64_t       page[0];
} __rte_packed;

/* This is actually vmbus_gpa_range.gpa_page[1] */
struct vmbus_gpa {
	uint32_t	len;
	uint32_t	ofs;
	uint64_t	page;
} __rte_packed;

struct vmbus_chanpkt_hdr {
	uint16_t	type;	/* VMBUS_CHANPKT_TYPE_ */
	uint16_t	hlen;	/* header len, in 8 bytes */
	uint16_t	tlen;	/* total len, in 8 bytes */
	uint16_t	flags;	/* VMBUS_CHANPKT_FLAG_ */
	uint64_t	xactid;
} __rte_packed;

static inline uint32_t
vmbus_chanpkt_datalen(const struct vmbus_chanpkt_hdr *pkt)
{
	return vmbus_chanpkt_getlen(pkt->tlen)
		- vmbus_chanpkt_getlen(pkt->hlen);
}

struct vmbus_chanpkt {
	struct vmbus_chanpkt_hdr hdr;
} __rte_packed;

struct vmbus_rxbuf_desc {
	uint32_t	len;
	uint32_t	ofs;
} __rte_packed;

struct vmbus_chanpkt_rxbuf {
	struct vmbus_chanpkt_hdr hdr;
	uint16_t	rxbuf_id;
	uint16_t	rsvd;
	uint32_t	rxbuf_cnt;
	struct vmbus_rxbuf_desc rxbuf[];
} __rte_packed;

struct vmbus_chanpkt_sglist {
	struct vmbus_chanpkt_hdr hdr;
	uint32_t	rsvd;
	uint32_t	gpa_cnt;
	struct vmbus_gpa gpa[];
} __rte_packed;

/*
 * Channel messages
 * - Embedded in vmbus_message.msg_data, e.g. response and notification.
 * - Embedded in hypercall_postmsg_in.hc_data, e.g. request.
 */

#define VMBUS_CHANMSG_TYPE_CHOFFER		1	/* NOTE */
#define VMBUS_CHANMSG_TYPE_CHRESCIND		2	/* NOTE */
#define VMBUS_CHANMSG_TYPE_CHREQUEST		3	/* REQ */
#define VMBUS_CHANMSG_TYPE_CHOFFER_DONE		4	/* NOTE */
#define VMBUS_CHANMSG_TYPE_CHOPEN		5	/* REQ */
#define VMBUS_CHANMSG_TYPE_CHOPEN_RESP		6	/* RESP */
#define VMBUS_CHANMSG_TYPE_CHCLOSE		7	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_CONN		8	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_SUBCONN	9	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_CONNRESP	10	/* RESP */
#define VMBUS_CHANMSG_TYPE_GPADL_DISCONN	11	/* REQ */
#define VMBUS_CHANMSG_TYPE_GPADL_DISCONNRESP	12	/* RESP */
#define VMBUS_CHANMSG_TYPE_CHFREE		13	/* REQ */
#define VMBUS_CHANMSG_TYPE_CONNECT		14	/* REQ */
#define VMBUS_CHANMSG_TYPE_CONNECT_RESP		15	/* RESP */
#define VMBUS_CHANMSG_TYPE_DISCONNECT		16	/* REQ */
#define VMBUS_CHANMSG_TYPE_MAX			22

struct vmbus_chanmsg_hdr {
	uint32_t	type;	/* VMBUS_CHANMSG_TYPE_ */
	uint32_t	rsvd;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CONNECT */
struct vmbus_chanmsg_connect {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	ver;
	uint32_t	rsvd;
	uint64_t	evtflags;
	uint64_t	mnf1;
	uint64_t	mnf2;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CONNECT_RESP */
struct vmbus_chanmsg_connect_resp {
	struct vmbus_chanmsg_hdr hdr;
	uint8_t		done;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CHREQUEST */
struct vmbus_chanmsg_chrequest {
	struct vmbus_chanmsg_hdr hdr;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_DISCONNECT */
struct vmbus_chanmsg_disconnect {
	struct vmbus_chanmsg_hdr hdr;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CHOPEN */
struct vmbus_chanmsg_chopen {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
	uint32_t	openid;
	uint32_t	gpadl;
	uint32_t	vcpuid;
	uint32_t	txbr_pgcnt;
#define VMBUS_CHANMSG_CHOPEN_UDATA_SIZE	120
	uint8_t		udata[VMBUS_CHANMSG_CHOPEN_UDATA_SIZE];
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CHOPEN_RESP */
struct vmbus_chanmsg_chopen_resp {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
	uint32_t	openid;
	uint32_t	status;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_GPADL_CONN */
struct vmbus_chanmsg_gpadl_conn {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
	uint32_t	gpadl;
	uint16_t	range_len;
	uint16_t	range_cnt;
	struct vmbus_gpa_range range;
} __rte_packed;

#define VMBUS_CHANMSG_GPADL_CONN_PGMAX		26

/* VMBUS_CHANMSG_TYPE_GPADL_SUBCONN */
struct vmbus_chanmsg_gpadl_subconn {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	msgno;
	uint32_t	gpadl;
	uint64_t	gpa_page[];
} __rte_packed;

#define VMBUS_CHANMSG_GPADL_SUBCONN_PGMAX	28

/* VMBUS_CHANMSG_TYPE_GPADL_CONNRESP */
struct vmbus_chanmsg_gpadl_connresp {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
	uint32_t	gpadl;
	uint32_t	status;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CHCLOSE */
struct vmbus_chanmsg_chclose {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_GPADL_DISCONN */
struct vmbus_chanmsg_gpadl_disconn {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
	uint32_t	gpadl;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CHFREE */
struct vmbus_chanmsg_chfree {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CHRESCIND */
struct vmbus_chanmsg_chrescind {
	struct vmbus_chanmsg_hdr hdr;
	uint32_t	chanid;
} __rte_packed;

/* VMBUS_CHANMSG_TYPE_CHOFFER */
struct vmbus_chanmsg_choffer {
	struct vmbus_chanmsg_hdr hdr;
	rte_uuid_t	chtype;
	rte_uuid_t	chinst;
	uint64_t	chlat;	/* unit: 100ns */
	uint32_t	chrev;
	uint32_t	svrctx_sz;
	uint16_t	chflags;
	uint16_t	mmio_sz;	/* unit: MB */
	uint8_t		udata[120];
	uint16_t	subidx;
	uint16_t	rsvd;
	uint32_t	chanid;
	uint8_t		montrig;
	uint8_t		flags1;	/* VMBUS_CHOFFER_FLAG1_ */
	uint16_t	flags2;
	uint32_t	connid;
} __rte_packed;

#define VMBUS_CHOFFER_FLAG1_HASMNF	0x01

#endif	/* !_VMBUS_REG_H_ */
