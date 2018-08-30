/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2018 Microsoft Corp.
 * All rights reserved.
 */

/*
 * The indirection table message is the largest message
 * received from host, and that is 112 bytes.
 */
#define NVS_RESPSIZE_MAX	256

/*
 * NDIS protocol version numbers
 */
#define NDIS_VERSION_6_1		0x00060001
#define NDIS_VERSION_6_20		0x00060014
#define NDIS_VERSION_6_30		0x0006001e
#define NDIS_VERSION_MAJOR(ver)	(((ver) & 0xffff0000) >> 16)
#define NDIS_VERSION_MINOR(ver)	((ver) & 0xffff)

/*
 * NVS versions.
 */
#define NVS_VERSION_1		0x00002
#define NVS_VERSION_2		0x30002
#define NVS_VERSION_4		0x40000
#define NVS_VERSION_5		0x50000
#define NVS_VERSION_6		0x60000
#define NVS_VERSION_61		0x60001

#define NVS_RXBUF_SIG		0xcafe
#define NVS_CHIM_SIG			0xface

#define NVS_CHIM_IDX_INVALID		0xffffffff

#define NVS_RNDIS_MTYPE_DATA		0
#define NVS_RNDIS_MTYPE_CTRL		1

/*
 * NVS message transacion status codes.
 */
#define NVS_STATUS_OK		1
#define NVS_STATUS_FAILED		2

/*
 * NVS request/response message types.
 */
#define NVS_TYPE_INIT		1
#define NVS_TYPE_INIT_RESP	2

#define NVS_TYPE_NDIS_INIT	100
#define NVS_TYPE_RXBUF_CONN	101
#define NVS_TYPE_RXBUF_CONNRESP	102
#define NVS_TYPE_RXBUF_DISCONN	103
#define NVS_TYPE_CHIM_CONN	104
#define NVS_TYPE_CHIM_CONNRESP	105
#define NVS_TYPE_CHIM_DISCONN	106
#define NVS_TYPE_RNDIS		107
#define NVS_TYPE_RNDIS_ACK	108

#define NVS_TYPE_NDIS_CONF	125
#define NVS_TYPE_VFASSOC_NOTE	128	/* notification */
#define NVS_TYPE_SET_DATAPATH	129
#define NVS_TYPE_SUBCH_REQ	133
#define NVS_TYPE_SUBCH_RESP	133	/* same as SUBCH_REQ */
#define NVS_TYPE_TXTBL_NOTE	134	/* notification */


/* NVS message common header */
struct hn_nvs_hdr {
	uint32_t	type;
} __rte_packed;

struct hn_nvs_init {
	uint32_t	type;	/* NVS_TYPE_INIT */
	uint32_t	ver_min;
	uint32_t	ver_max;
	uint8_t		rsvd[28];
} __rte_packed;

struct hn_nvs_init_resp {
	uint32_t	type;	/* NVS_TYPE_INIT_RESP */
	uint32_t	ver;	/* deprecated */
	uint32_t	rsvd;
	uint32_t	status;	/* NVS_STATUS_ */
} __rte_packed;

/* No response */
struct hn_nvs_ndis_conf {
	uint32_t	type;	/* NVS_TYPE_NDIS_CONF */
	uint32_t	mtu;
	uint32_t	rsvd;
	uint64_t	caps;	/* NVS_NDIS_CONF_ */
	uint8_t		rsvd1[20];
} __rte_packed;

#define NVS_NDIS_CONF_SRIOV		0x0004
#define NVS_NDIS_CONF_VLAN		0x0008

/* No response */
struct hn_nvs_ndis_init {
	uint32_t	type;	/* NVS_TYPE_NDIS_INIT */
	uint32_t	ndis_major;	/* NDIS_VERSION_MAJOR_ */
	uint32_t	ndis_minor;	/* NDIS_VERSION_MINOR_ */
	uint8_t		rsvd[28];
} __rte_packed;

struct hn_nvs_vf_association {
	uint32_t	type;	/* NVS_TYPE_VFASSOC_NOTE */
	uint32_t	allocated;
	uint32_t	serial;
} __rte_packed;

#define NVS_DATAPATH_SYNTHETIC	0
#define NVS_DATAPATH_VF		1

/* No response */
struct hn_nvs_datapath {
	uint32_t	type;	/* NVS_TYPE_SET_DATAPATH */
	uint32_t	active_path;/* NVS_DATAPATH_* */
	uint8_t		rsvd[32];
} __rte_packed;

struct hn_nvs_rxbuf_conn {
	uint32_t	type;	/* NVS_TYPE_RXBUF_CONN */
	uint32_t	gpadl;	/* RXBUF vmbus GPADL */
	uint16_t	sig;	/* NVS_RXBUF_SIG */
	uint8_t		rsvd[30];
} __rte_packed;

struct hn_nvs_rxbuf_sect {
	uint32_t	start;
	uint32_t	slotsz;
	uint32_t	slotcnt;
	uint32_t	end;
} __rte_packed;

struct hn_nvs_rxbuf_connresp {
	uint32_t	type;	/* NVS_TYPE_RXBUF_CONNRESP */
	uint32_t	status;	/* NVS_STATUS_ */
	uint32_t	nsect;	/* # of elem in nvs_sect */
	struct hn_nvs_rxbuf_sect nvs_sect[1];
} __rte_packed;

/* No response */
struct hn_nvs_rxbuf_disconn {
	uint32_t	type;	/* NVS_TYPE_RXBUF_DISCONN */
	uint16_t	sig;	/* NVS_RXBUF_SIG */
	uint8_t		rsvd[34];
} __rte_packed;

struct hn_nvs_chim_conn {
	uint32_t	type;	/* NVS_TYPE_CHIM_CONN */
	uint32_t	gpadl;	/* chimney buf vmbus GPADL */
	uint16_t	sig;	/* NDIS_NVS_CHIM_SIG */
	uint8_t		rsvd[30];
} __rte_packed;

struct hn_nvs_chim_connresp {
	uint32_t	type;	/* NVS_TYPE_CHIM_CONNRESP */
	uint32_t	status;	/* NVS_STATUS_ */
	uint32_t	sectsz;	/* section size */
} __rte_packed;

/* No response */
struct hn_nvs_chim_disconn {
	uint32_t	type;	/* NVS_TYPE_CHIM_DISCONN */
	uint16_t	sig;	/* NVS_CHIM_SIG */
	uint8_t		rsvd[34];
} __rte_packed;

#define NVS_SUBCH_OP_ALLOC		1

struct hn_nvs_subch_req {
	uint32_t	type;	/* NVS_TYPE_SUBCH_REQ */
	uint32_t	op;	/* NVS_SUBCH_OP_ */
	uint32_t	nsubch;
	uint8_t		rsvd[28];
} __rte_packed;

struct hn_nvs_subch_resp {
	uint32_t	type;	/* NVS_TYPE_SUBCH_RESP */
	uint32_t	status;	/* NVS_STATUS_ */
	uint32_t	nsubch;
	uint8_t		rsvd[28];
} __rte_packed;

struct hn_nvs_rndis {
	uint32_t	type;	/* NVS_TYPE_RNDIS */
	uint32_t	rndis_mtype;/* NVS_RNDIS_MTYPE_ */
	/*
	 * Chimney sending buffer index and size.
	 *
	 * NOTE:
	 * If nvs_chim_idx is set to NVS_CHIM_IDX_INVALID
	 * and nvs_chim_sz is set to 0, then chimney sending
	 * buffer is _not_ used by this RNDIS message.
	 */
	uint32_t	chim_idx;
	uint32_t	chim_sz;
	uint8_t		rsvd[24];
} __rte_packed;

struct hn_nvs_rndis_ack {
	uint32_t	type;	/* NVS_TYPE_RNDIS_ACK */
	uint32_t	status;	/* NVS_STATUS_ */
	uint8_t		rsvd[32];
} __rte_packed;


int	hn_nvs_attach(struct hn_data *hv, unsigned int mtu);
void	hn_nvs_detach(struct hn_data *hv);
void	hn_nvs_ack_rxbuf(struct vmbus_channel *chan, uint64_t tid);
int	hn_nvs_alloc_subchans(struct hn_data *hv, uint32_t *nsubch);
void	hn_nvs_set_datapath(struct hn_data *hv, uint32_t path);
void	hn_nvs_handle_vfassoc(struct rte_eth_dev *dev,
			      const struct vmbus_chanpkt_hdr *hdr,
			      const void *data);

static inline int
hn_nvs_send(struct vmbus_channel *chan, uint16_t flags,
	    void *nvs_msg, int nvs_msglen, uintptr_t sndc,
	    bool *need_sig)
{
	return rte_vmbus_chan_send(chan, VMBUS_CHANPKT_TYPE_INBAND,
				   nvs_msg, nvs_msglen, (uint64_t)sndc,
				   flags, need_sig);
}

static inline int
hn_nvs_send_sglist(struct vmbus_channel *chan,
		   struct vmbus_gpa sg[], unsigned int sglen,
		   void *nvs_msg, int nvs_msglen,
		   uintptr_t sndc, bool *need_sig)
{
	return rte_vmbus_chan_send_sglist(chan, sg, sglen, nvs_msg, nvs_msglen,
					  (uint64_t)sndc, need_sig);
}
