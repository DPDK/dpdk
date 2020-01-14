/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _CQ_ENET_DESC_H_
#define _CQ_ENET_DESC_H_

#include <rte_byteorder.h>
#include "cq_desc.h"

/* Ethernet completion queue descriptor: 16B */
struct cq_enet_wq_desc {
	uint16_t completed_index;
	uint16_t q_number;
	uint8_t reserved[11];
	uint8_t type_color;
};

static inline void cq_enet_wq_desc_enc(struct cq_enet_wq_desc *desc,
	uint8_t type, uint8_t color, uint16_t q_number,
	uint16_t completed_index)
{
	cq_desc_enc((struct cq_desc *)desc, type,
		color, q_number, completed_index);
}

static inline void cq_enet_wq_desc_dec(struct cq_enet_wq_desc *desc,
	uint8_t *type, uint8_t *color, uint16_t *q_number,
	uint16_t *completed_index)
{
	cq_desc_dec((struct cq_desc *)desc, type,
		color, q_number, completed_index);
}

/* Completion queue descriptor: Ethernet receive queue, 16B */
struct cq_enet_rq_desc {
	uint16_t completed_index_flags;
	uint16_t q_number_rss_type_flags;
	uint32_t rss_hash;
	uint16_t bytes_written_flags;
	uint16_t vlan;
	uint16_t checksum_fcoe;
	uint8_t flags;
	uint8_t type_color;
};

/* Completion queue descriptor: Ethernet receive queue, 16B */
struct cq_enet_rq_clsf_desc {
	uint16_t completed_index_flags;
	uint16_t q_number_rss_type_flags;
	uint16_t filter_id;
	uint16_t lif;
	uint16_t bytes_written_flags;
	uint16_t vlan;
	uint16_t checksum_fcoe;
	uint8_t flags;
	uint8_t type_color;
};

#define CQ_ENET_RQ_DESC_FLAGS_INGRESS_PORT          (0x1 << 12)
#define CQ_ENET_RQ_DESC_FLAGS_FCOE                  (0x1 << 13)
#define CQ_ENET_RQ_DESC_FLAGS_EOP                   (0x1 << 14)
#define CQ_ENET_RQ_DESC_FLAGS_SOP                   (0x1 << 15)

#define CQ_ENET_RQ_DESC_RSS_TYPE_BITS               4
#define CQ_ENET_RQ_DESC_RSS_TYPE_MASK \
	((1 << CQ_ENET_RQ_DESC_RSS_TYPE_BITS) - 1)
#define CQ_ENET_RQ_DESC_RSS_TYPE_NONE               0
#define CQ_ENET_RQ_DESC_RSS_TYPE_IPv4               1
#define CQ_ENET_RQ_DESC_RSS_TYPE_TCP_IPv4           2
#define CQ_ENET_RQ_DESC_RSS_TYPE_IPv6               3
#define CQ_ENET_RQ_DESC_RSS_TYPE_TCP_IPv6           4
#define CQ_ENET_RQ_DESC_RSS_TYPE_IPv6_EX            5
#define CQ_ENET_RQ_DESC_RSS_TYPE_TCP_IPv6_EX        6

#define CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC         (0x1 << 14)

#define CQ_ENET_RQ_DESC_BYTES_WRITTEN_BITS          14
#define CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK \
	((1 << CQ_ENET_RQ_DESC_BYTES_WRITTEN_BITS) - 1)
#define CQ_ENET_RQ_DESC_FLAGS_TRUNCATED             (0x1 << 14)
#define CQ_ENET_RQ_DESC_FLAGS_VLAN_STRIPPED         (0x1 << 15)

#define CQ_ENET_RQ_DESC_VLAN_TCI_VLAN_BITS          12
#define CQ_ENET_RQ_DESC_VLAN_TCI_VLAN_MASK \
	((1 << CQ_ENET_RQ_DESC_VLAN_TCI_VLAN_BITS) - 1)
#define CQ_ENET_RQ_DESC_VLAN_TCI_CFI_MASK           (0x1 << 12)
#define CQ_ENET_RQ_DESC_VLAN_TCI_USER_PRIO_BITS     3
#define CQ_ENET_RQ_DESC_VLAN_TCI_USER_PRIO_MASK \
	((1 << CQ_ENET_RQ_DESC_VLAN_TCI_USER_PRIO_BITS) - 1)
#define CQ_ENET_RQ_DESC_VLAN_TCI_USER_PRIO_SHIFT    13

#define CQ_ENET_RQ_DESC_FCOE_SOF_BITS               8
#define CQ_ENET_RQ_DESC_FCOE_SOF_MASK \
	((1 << CQ_ENET_RQ_DESC_FCOE_SOF_BITS) - 1)
#define CQ_ENET_RQ_DESC_FCOE_EOF_BITS               8
#define CQ_ENET_RQ_DESC_FCOE_EOF_MASK \
	((1 << CQ_ENET_RQ_DESC_FCOE_EOF_BITS) - 1)
#define CQ_ENET_RQ_DESC_FCOE_EOF_SHIFT              8

#define CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK       (0x1 << 0)
#define CQ_ENET_RQ_DESC_FCOE_FC_CRC_OK              (0x1 << 0)
#define CQ_ENET_RQ_DESC_FLAGS_UDP                   (0x1 << 1)
#define CQ_ENET_RQ_DESC_FCOE_ENC_ERROR              (0x1 << 1)
#define CQ_ENET_RQ_DESC_FLAGS_TCP                   (0x1 << 2)
#define CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK          (0x1 << 3)
#define CQ_ENET_RQ_DESC_FLAGS_IPV6                  (0x1 << 4)
#define CQ_ENET_RQ_DESC_FLAGS_IPV4                  (0x1 << 5)
#define CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT         (0x1 << 6)
#define CQ_ENET_RQ_DESC_FLAGS_FCS_OK                (0x1 << 7)

static inline void cq_enet_rq_desc_enc(struct cq_enet_rq_desc *desc,
	uint8_t type, uint8_t color, uint16_t q_number,
	uint16_t completed_index, uint8_t ingress_port, uint8_t fcoe,
	uint8_t eop, uint8_t sop, uint8_t rss_type, uint8_t csum_not_calc,
	uint32_t rss_hash, uint16_t bytes_written, uint8_t packet_error,
	uint8_t vlan_stripped, uint16_t vlan, uint16_t checksum,
	uint8_t fcoe_sof, uint8_t fcoe_fc_crc_ok, uint8_t fcoe_enc_error,
	uint8_t fcoe_eof, uint8_t tcp_udp_csum_ok, uint8_t udp, uint8_t tcp,
	uint8_t ipv4_csum_ok, uint8_t ipv6, uint8_t ipv4, uint8_t ipv4_fragment,
	uint8_t fcs_ok)
{
	cq_desc_enc((struct cq_desc *)desc, type,
		color, q_number, completed_index);

	desc->completed_index_flags |= rte_cpu_to_le_16
		((ingress_port ? CQ_ENET_RQ_DESC_FLAGS_INGRESS_PORT : 0) |
		(fcoe ? CQ_ENET_RQ_DESC_FLAGS_FCOE : 0) |
		(eop ? CQ_ENET_RQ_DESC_FLAGS_EOP : 0) |
		(sop ? CQ_ENET_RQ_DESC_FLAGS_SOP : 0));

	desc->q_number_rss_type_flags |= rte_cpu_to_le_16
		(((rss_type & CQ_ENET_RQ_DESC_RSS_TYPE_MASK) <<
		CQ_DESC_Q_NUM_BITS) |
		(csum_not_calc ? CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC : 0));

	desc->rss_hash = rte_cpu_to_le_32(rss_hash);

	desc->bytes_written_flags = rte_cpu_to_le_16
		((bytes_written & CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK) |
		(packet_error ? CQ_ENET_RQ_DESC_FLAGS_TRUNCATED : 0) |
		(vlan_stripped ? CQ_ENET_RQ_DESC_FLAGS_VLAN_STRIPPED : 0));

	desc->vlan = rte_cpu_to_le_16(vlan);

	if (fcoe) {
		desc->checksum_fcoe = rte_cpu_to_le_16
			((fcoe_sof & CQ_ENET_RQ_DESC_FCOE_SOF_MASK) |
			((fcoe_eof & CQ_ENET_RQ_DESC_FCOE_EOF_MASK) <<
				CQ_ENET_RQ_DESC_FCOE_EOF_SHIFT));
	} else {
		desc->checksum_fcoe = rte_cpu_to_le_16(checksum);
	}

	desc->flags =
		(tcp_udp_csum_ok ? CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK : 0) |
		(udp ? CQ_ENET_RQ_DESC_FLAGS_UDP : 0) |
		(tcp ? CQ_ENET_RQ_DESC_FLAGS_TCP : 0) |
		(ipv4_csum_ok ? CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK : 0) |
		(ipv6 ? CQ_ENET_RQ_DESC_FLAGS_IPV6 : 0) |
		(ipv4 ? CQ_ENET_RQ_DESC_FLAGS_IPV4 : 0) |
		(ipv4_fragment ? CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT : 0) |
		(fcs_ok ? CQ_ENET_RQ_DESC_FLAGS_FCS_OK : 0) |
		(fcoe_fc_crc_ok ? CQ_ENET_RQ_DESC_FCOE_FC_CRC_OK : 0) |
		(fcoe_enc_error ? CQ_ENET_RQ_DESC_FCOE_ENC_ERROR : 0);
}

static inline void cq_enet_rq_desc_dec(struct cq_enet_rq_desc *desc,
	uint8_t *type, uint8_t *color, uint16_t *q_number,
	uint16_t *completed_index, uint8_t *ingress_port, uint8_t *fcoe,
	uint8_t *eop, uint8_t *sop, uint8_t *rss_type, uint8_t *csum_not_calc,
	uint32_t *rss_hash, uint16_t *bytes_written, uint8_t *packet_error,
	uint8_t *vlan_stripped, uint16_t *vlan_tci, uint16_t *checksum,
	uint8_t *fcoe_sof, uint8_t *fcoe_fc_crc_ok, uint8_t *fcoe_enc_error,
	uint8_t *fcoe_eof, uint8_t *tcp_udp_csum_ok, uint8_t *udp, uint8_t *tcp,
	uint8_t *ipv4_csum_ok, uint8_t *ipv6, uint8_t *ipv4,
	uint8_t *ipv4_fragment, uint8_t *fcs_ok)
{
	uint16_t completed_index_flags;
	uint16_t q_number_rss_type_flags;
	uint16_t bytes_written_flags;

	cq_desc_dec((struct cq_desc *)desc, type,
		color, q_number, completed_index);

	completed_index_flags = rte_le_to_cpu_16(desc->completed_index_flags);
	q_number_rss_type_flags =
		rte_le_to_cpu_16(desc->q_number_rss_type_flags);
	bytes_written_flags = rte_le_to_cpu_16(desc->bytes_written_flags);

	*ingress_port = (completed_index_flags &
		CQ_ENET_RQ_DESC_FLAGS_INGRESS_PORT) ? 1 : 0;
	*fcoe = (completed_index_flags & CQ_ENET_RQ_DESC_FLAGS_FCOE) ?
		1 : 0;
	*eop = (completed_index_flags & CQ_ENET_RQ_DESC_FLAGS_EOP) ?
		1 : 0;
	*sop = (completed_index_flags & CQ_ENET_RQ_DESC_FLAGS_SOP) ?
		1 : 0;

	*rss_type = (uint8_t)((q_number_rss_type_flags >> CQ_DESC_Q_NUM_BITS) &
		CQ_ENET_RQ_DESC_RSS_TYPE_MASK);
	*csum_not_calc = (q_number_rss_type_flags &
		CQ_ENET_RQ_DESC_FLAGS_CSUM_NOT_CALC) ? 1 : 0;

	*rss_hash = rte_le_to_cpu_32(desc->rss_hash);

	*bytes_written = bytes_written_flags &
		CQ_ENET_RQ_DESC_BYTES_WRITTEN_MASK;
	*packet_error = (bytes_written_flags &
		CQ_ENET_RQ_DESC_FLAGS_TRUNCATED) ? 1 : 0;
	*vlan_stripped = (bytes_written_flags &
		CQ_ENET_RQ_DESC_FLAGS_VLAN_STRIPPED) ? 1 : 0;

	/*
	 * Tag Control Information(16) = user_priority(3) + cfi(1) + vlan(12)
	 */
	*vlan_tci = rte_le_to_cpu_16(desc->vlan);

	if (*fcoe) {
		*fcoe_sof = (uint8_t)(rte_le_to_cpu_16(desc->checksum_fcoe) &
			CQ_ENET_RQ_DESC_FCOE_SOF_MASK);
		*fcoe_fc_crc_ok = (desc->flags &
			CQ_ENET_RQ_DESC_FCOE_FC_CRC_OK) ? 1 : 0;
		*fcoe_enc_error = (desc->flags &
			CQ_ENET_RQ_DESC_FCOE_ENC_ERROR) ? 1 : 0;
		*fcoe_eof = (uint8_t)((rte_le_to_cpu_16(desc->checksum_fcoe) >>
			CQ_ENET_RQ_DESC_FCOE_EOF_SHIFT) &
			CQ_ENET_RQ_DESC_FCOE_EOF_MASK);
		*checksum = 0;
	} else {
		*fcoe_sof = 0;
		*fcoe_fc_crc_ok = 0;
		*fcoe_enc_error = 0;
		*fcoe_eof = 0;
		*checksum = rte_le_to_cpu_16(desc->checksum_fcoe);
	}

	*tcp_udp_csum_ok =
		(desc->flags & CQ_ENET_RQ_DESC_FLAGS_TCP_UDP_CSUM_OK) ? 1 : 0;
	*udp = (desc->flags & CQ_ENET_RQ_DESC_FLAGS_UDP) ? 1 : 0;
	*tcp = (desc->flags & CQ_ENET_RQ_DESC_FLAGS_TCP) ? 1 : 0;
	*ipv4_csum_ok =
		(desc->flags & CQ_ENET_RQ_DESC_FLAGS_IPV4_CSUM_OK) ? 1 : 0;
	*ipv6 = (desc->flags & CQ_ENET_RQ_DESC_FLAGS_IPV6) ? 1 : 0;
	*ipv4 = (desc->flags & CQ_ENET_RQ_DESC_FLAGS_IPV4) ? 1 : 0;
	*ipv4_fragment =
		(desc->flags & CQ_ENET_RQ_DESC_FLAGS_IPV4_FRAGMENT) ? 1 : 0;
	*fcs_ok = (desc->flags & CQ_ENET_RQ_DESC_FLAGS_FCS_OK) ? 1 : 0;
}

#endif /* _CQ_ENET_DESC_H_ */
