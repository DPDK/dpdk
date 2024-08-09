/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _VNIC_RESOURCE_H_
#define _VNIC_RESOURCE_H_

#define VNIC_RES_MAGIC		0x766E6963L	/* 'vnic' */
#define VNIC_RES_VERSION	0x00000000L
#define MGMTVNIC_MAGIC		0x544d474dL	/* 'MGMT' */
#define MGMTVNIC_VERSION	0x00000000L

/* The MAC address assigned to the CFG vNIC is fixed. */
#define MGMTVNIC_MAC		{ 0x02, 0x00, 0x54, 0x4d, 0x47, 0x4d }

/* vNIC resource types */
enum vnic_res_type {
	RES_TYPE_EOL,			/* End-of-list */
	RES_TYPE_WQ,			/* Work queues */
	RES_TYPE_RQ,			/* Receive queues */
	RES_TYPE_CQ,			/* Completion queues */
	RES_TYPE_MEM,			/* Window to dev memory */
	RES_TYPE_NIC_CFG,		/* Enet NIC config registers */
	RES_TYPE_RSS_KEY,		/* Enet RSS secret key */
	RES_TYPE_RSS_CPU,		/* Enet RSS indirection table */
	RES_TYPE_TX_STATS,		/* Netblock Tx statistic regs */
	RES_TYPE_RX_STATS,		/* Netblock Rx statistic regs */
	RES_TYPE_INTR_CTRL,		/* Interrupt ctrl table */
	RES_TYPE_INTR_TABLE,		/* MSI/MSI-X Interrupt table */
	RES_TYPE_INTR_PBA,		/* MSI/MSI-X PBA table */
	RES_TYPE_INTR_PBA_LEGACY,	/* Legacy intr status */
	RES_TYPE_DEBUG,			/* Debug-only info */
	RES_TYPE_DEV,			/* Device-specific region */
	RES_TYPE_DEVCMD,		/* Device command region */
	RES_TYPE_PASS_THRU_PAGE,	/* Pass-thru page */
	RES_TYPE_SUBVNIC,               /* subvnic resource type */
	RES_TYPE_MQ_WQ,                 /* MQ Work queues */
	RES_TYPE_MQ_RQ,                 /* MQ Receive queues */
	RES_TYPE_MQ_CQ,                 /* MQ Completion queues */
	RES_TYPE_DEPRECATED1,		/* Old version of devcmd 2 */
	RES_TYPE_DEPRECATED2,		/* Old version of devcmd 2 */
	RES_TYPE_DEVCMD2,		/* Device control region */
	RES_TYPE_RDMA_WQ,		/* RDMA WQ */
	RES_TYPE_RDMA_RQ,		/* RDMA RQ */
	RES_TYPE_RDMA_CQ,		/* RDMA CQ */
	RES_TYPE_RDMA_RKEY_TABLE,	/* RDMA RKEY table */
	RES_TYPE_RDMA_RQ_HEADER_TABLE,	/* RDMA RQ Header Table */
	RES_TYPE_RDMA_RQ_TABLE,		/* RDMA RQ Table */
	RES_TYPE_RDMA_RD_RESP_HEADER_TABLE,	/* RDMA Read Response Header Table */
	RES_TYPE_RDMA_RD_RESP_TABLE,	/* RDMA Read Response Table */
	RES_TYPE_RDMA_QP_STATS_TABLE,	/* RDMA per QP stats table */
	RES_TYPE_WQ_MREGS,		/* XXX snic proto only */
	RES_TYPE_GRPMBR_INTR,		/* Group member interrupt control */
	RES_TYPE_DPKT,			/* Direct Packet memory region */
	RES_TYPE_RDMA2_DATA_WQ,		/* RDMA datapath command WQ */
	RES_TYPE_RDMA2_REG_WQ,		/* RDMA registration command WQ */
	RES_TYPE_RDMA2_CQ,		/* RDMA datapath CQ */
	RES_TYPE_MQ_RDMA2_DATA_WQ,	/* RDMA datapath command WQ */
	RES_TYPE_MQ_RDMA2_REG_WQ,	/* RDMA registration command WQ */
	RES_TYPE_MQ_RDMA2_CQ,		/* RDMA datapath CQ */
	RES_TYPE_PTP,			/* PTP registers */
	RES_TYPE_INTR_CTRL2,		/* Extended INTR CTRL registers */
	RES_TYPE_SRIOV_INTR,		/* VF intr */
	RES_TYPE_VF_WQ,			/* VF WQ */
	RES_TYPE_VF_RQ,			/* VF RQ */
	RES_TYPE_VF_CQ,			/* VF CQ */
	RES_TYPE_ADMIN_WQ,		/* admin channel WQ */
	RES_TYPE_ADMIN_RQ,		/* admin channel RQ */
	RES_TYPE_ADMIN_CQ,		/* admin channel CQ */
	RES_TYPE_MAX,			/* Count of resource types */
};

struct vnic_resource_header {
	uint32_t magic;
	uint32_t version;
};

struct mgmt_barmap_hdr {
	uint32_t magic;			/* magic number */
	uint32_t version;		/* header format version */
	uint16_t lif;			/* loopback lif for mgmt frames */
	uint16_t pci_slot;		/* installed pci slot */
	char serial[16];		/* card serial number */
};

struct vnic_resource {
	uint8_t type;
	uint8_t bar;
	uint8_t pad[2];
	uint32_t bar_offset;
	uint32_t count;
};

#endif /* _VNIC_RESOURCE_H_ */
