/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_ENIC_H_
#define _VNIC_ENIC_H_

/* Hardware intr coalesce timer is in units of 1.5us */
#define INTR_COALESCE_USEC_TO_HW(usec) ((usec) * 2 / 3)
#define INTR_COALESCE_HW_TO_USEC(usec) ((usec) * 3 / 2)

/* Device-specific region: enet configuration */
struct vnic_enet_config {
	uint32_t flags;
	uint32_t wq_desc_count;
	uint32_t rq_desc_count;
	uint16_t mtu;
	uint16_t intr_timer_deprecated;
	uint8_t intr_timer_type;
	uint8_t intr_mode;
	char devname[16];
	uint32_t intr_timer_usec;
	uint16_t loop_tag;
	uint16_t vf_rq_count;
	uint16_t num_arfs;
	uint64_t mem_paddr;
	uint16_t rdma_qp_id;
	uint16_t rdma_qp_count;
	uint16_t rdma_resgrp;
	uint32_t rdma_mr_id;
	uint32_t rdma_mr_count;
	uint32_t max_pkt_size;
};

#define VENETF_TSO		0x1	/* TSO enabled */
#define VENETF_LRO		0x2	/* LRO enabled */
#define VENETF_RXCSUM		0x4	/* RX csum enabled */
#define VENETF_TXCSUM		0x8	/* TX csum enabled */
#define VENETF_RSS		0x10	/* RSS enabled */
#define VENETF_RSSHASH_IPV4	0x20	/* Hash on IPv4 fields */
#define VENETF_RSSHASH_TCPIPV4	0x40	/* Hash on TCP + IPv4 fields */
#define VENETF_RSSHASH_IPV6	0x80	/* Hash on IPv6 fields */
#define VENETF_RSSHASH_TCPIPV6	0x100	/* Hash on TCP + IPv6 fields */
#define VENETF_RSSHASH_IPV6_EX	0x200	/* Hash on IPv6 extended fields */
#define VENETF_RSSHASH_TCPIPV6_EX 0x400	/* Hash on TCP + IPv6 ext. fields */
#define VENETF_LOOP		0x800	/* Loopback enabled */
#define VENETF_FAILOVER		0x1000	/* Fabric failover enabled */
#define VENETF_USPACE_NIC       0x2000	/* vHPC enabled */
#define VENETF_VMQ      0x4000 /* VMQ enabled */
#define VENETF_ARFS		0x8000  /* ARFS enabled */
#define VENETF_VXLAN    0x10000 /* VxLAN offload */
#define VENETF_NVGRE    0x20000 /* NVGRE offload */
#define VENETF_GRPINTR  0x40000 /* group interrupt */
#define VENETF_NICSWITCH        0x80000 /* NICSWITCH enabled */
#define VENETF_RSSHASH_UDPIPV4  0x100000 /* Hash on UDP + IPv4 fields */
#define VENETF_RSSHASH_UDPIPV6  0x200000 /* Hash on UDP + IPv6 fields */
#define VENETF_GENEVE		0x400000 /* GENEVE offload */

#define VENET_INTR_TYPE_MIN	0	/* Timer specs min interrupt spacing */
#define VENET_INTR_TYPE_IDLE	1	/* Timer specs idle time before irq */

#define VENET_INTR_MODE_ANY	0	/* Try MSI-X, then MSI, then INTx */
#define VENET_INTR_MODE_MSI	1	/* Try MSI then INTx */
#define VENET_INTR_MODE_INTX	2	/* Try INTx only */

#endif /* _VNIC_ENIC_H_ */
