/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2001-2023 Intel Corporation
 */

#ifndef _IDPF_TYPE_H_
#define _IDPF_TYPE_H_

#include "idpf_controlq.h"

#define UNREFERENCED_XPARAMETER
#define UNREFERENCED_1PARAMETER(_p)
#define UNREFERENCED_2PARAMETER(_p, _q)
#define UNREFERENCED_3PARAMETER(_p, _q, _r)
#define UNREFERENCED_4PARAMETER(_p, _q, _r, _s)
#define UNREFERENCED_5PARAMETER(_p, _q, _r, _s, _t)

struct idpf_eth_stats {
	u64 rx_bytes;			/* gorc */
	u64 rx_unicast;			/* uprc */
	u64 rx_multicast;		/* mprc */
	u64 rx_broadcast;		/* bprc */
	u64 rx_discards;		/* rdpc */
	u64 rx_unknown_protocol;	/* rupp */
	u64 tx_bytes;			/* gotc */
	u64 tx_unicast;			/* uptc */
	u64 tx_multicast;		/* mptc */
	u64 tx_broadcast;		/* bptc */
	u64 tx_discards;		/* tdpc */
	u64 tx_errors;			/* tepc */
};

/* Statistics collected by the MAC */
struct idpf_hw_port_stats {
	/* eth stats collected by the port */
	struct idpf_eth_stats eth;

	/* additional port specific stats */
	u64 tx_dropped_link_down;	/* tdold */
	u64 crc_errors;			/* crcerrs */
	u64 illegal_bytes;		/* illerrc */
	u64 error_bytes;		/* errbc */
	u64 mac_local_faults;		/* mlfc */
	u64 mac_remote_faults;		/* mrfc */
	u64 rx_length_errors;		/* rlec */
	u64 link_xon_rx;		/* lxonrxc */
	u64 link_xoff_rx;		/* lxoffrxc */
	u64 priority_xon_rx[8];		/* pxonrxc[8] */
	u64 priority_xoff_rx[8];	/* pxoffrxc[8] */
	u64 link_xon_tx;		/* lxontxc */
	u64 link_xoff_tx;		/* lxofftxc */
	u64 priority_xon_tx[8];		/* pxontxc[8] */
	u64 priority_xoff_tx[8];	/* pxofftxc[8] */
	u64 priority_xon_2_xoff[8];	/* pxon2offc[8] */
	u64 rx_size_64;			/* prc64 */
	u64 rx_size_127;		/* prc127 */
	u64 rx_size_255;		/* prc255 */
	u64 rx_size_511;		/* prc511 */
	u64 rx_size_1023;		/* prc1023 */
	u64 rx_size_1522;		/* prc1522 */
	u64 rx_size_big;		/* prc9522 */
	u64 rx_undersize;		/* ruc */
	u64 rx_fragments;		/* rfc */
	u64 rx_oversize;		/* roc */
	u64 rx_jabber;			/* rjc */
	u64 tx_size_64;			/* ptc64 */
	u64 tx_size_127;		/* ptc127 */
	u64 tx_size_255;		/* ptc255 */
	u64 tx_size_511;		/* ptc511 */
	u64 tx_size_1023;		/* ptc1023 */
	u64 tx_size_1522;		/* ptc1522 */
	u64 tx_size_big;		/* ptc9522 */
	u64 mac_short_packet_dropped;	/* mspdc */
	u64 checksum_error;		/* xec */
};
/* Static buffer size to initialize control queue */
struct idpf_ctlq_size {
	u16 asq_buf_size;
	u16 asq_ring_size;
	u16 arq_buf_size;
	u16 arq_ring_size;
};

/* Temporary definition to compile - TBD if needed */
struct idpf_arq_event_info {
	struct idpf_ctlq_desc desc;
	u16 msg_len;
	u16 buf_len;
	u8 *msg_buf;
};

struct idpf_get_set_rss_key_data {
	u8 standard_rss_key[0x28];
	u8 extended_hash_key[0xc];
};

struct idpf_aq_get_phy_abilities_resp {
	__le32 phy_type;
};

struct idpf_filter_program_desc {
	__le32 qid;
};

#endif /* _IDPF_TYPE_H_ */
