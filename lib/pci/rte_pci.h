/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation.
 * Copyright 2013-2014 6WIND S.A.
 */

#ifndef _RTE_PCI_H_
#define _RTE_PCI_H_

/**
 * @file
 *
 * RTE PCI Library
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <inttypes.h>
#include <sys/types.h>

/*
 * Conventional PCI and PCI-X Mode 1 devices have 256 bytes of
 * configuration space.  PCI-X Mode 2 and PCIe devices have 4096 bytes of
 * configuration space.
 */
#define RTE_PCI_CFG_SPACE_SIZE		256
#define RTE_PCI_CFG_SPACE_EXP_SIZE	4096

#define RTE_PCI_STD_HEADER_SIZEOF	64

/* Standard register offsets in the PCI configuration space */
#define RTE_PCI_VENDOR_ID	0x00	/* 16 bits */
#define RTE_PCI_DEVICE_ID	0x02	/* 16 bits */
#define RTE_PCI_COMMAND		0x04	/* 16 bits */
#define RTE_PCI_STATUS		0x06	/* 16 bits */
#define RTE_PCI_BASE_ADDRESS_0	0x10	/* 32 bits */
#define RTE_PCI_CAPABILITY_LIST	0x34	/* 32 bits */

/* PCI Command Register (RTE_PCI_COMMAND) */
#define RTE_PCI_COMMAND_MEMORY		0x2	/* Enable response in Memory space */
#define RTE_PCI_COMMAND_MASTER		0x4	/* Bus Master Enable */
#define RTE_PCI_COMMAND_INTX_DISABLE	0x400	/* INTx Emulation Disable */

/* PCI Status Register (RTE_PCI_STATUS) */
#define RTE_PCI_STATUS_CAP_LIST		0x10	/* Support Capability List */

/* Base addresses (RTE_PCI_BASE_ADDRESS_*) */
#define RTE_PCI_BASE_ADDRESS_SPACE_IO	0x01

/* Capability registers (RTE_PCI_CAPABILITY_LIST) */
#define RTE_PCI_CAP_ID_PM		0x01	/* Power Management */
#define RTE_PCI_CAP_ID_MSI		0x05	/* Message Signalled Interrupts */
#define RTE_PCI_CAP_ID_VNDR		0x09	/* Vendor-Specific */
#define RTE_PCI_CAP_ID_EXP		0x10	/* PCI Express */
#define RTE_PCI_CAP_ID_MSIX		0x11	/* MSI-X */
#define RTE_PCI_CAP_SIZEOF		4
#define RTE_PCI_CAP_NEXT		1

/* Power Management Registers (RTE_PCI_CAP_ID_PM) */
#define RTE_PCI_PM_CTRL			4	/* PM control and status register */
#define RTE_PCI_PM_CTRL_STATE_MASK	0x0003	/* Current power state (D0 to D3) */
#define RTE_PCI_PM_CTRL_PME_ENABLE	0x0100	/* PME pin enable */
#define RTE_PCI_PM_CTRL_PME_STATUS	0x8000	/* PME pin status */

/* PCI Express capability registers (RTE_PCI_CAP_ID_EXP) */
#define RTE_PCI_EXP_TYPE_RC_EC		0xa	/* Root Complex Event Collector */
#define RTE_PCI_EXP_DEVCTL		0x08	/* Device Control */
#define RTE_PCI_EXP_DEVCTL_PAYLOAD	0x00e0	/* Max_Payload_Size */
#define RTE_PCI_EXP_DEVCTL_READRQ	0x7000	/* Max_Read_Request_Size */
#define RTE_PCI_EXP_DEVCTL_BCR_FLR	0x8000	/* Bridge Configuration Retry / FLR */
#define RTE_PCI_EXP_DEVSTA		0x0a	/* Device Status */
#define RTE_PCI_EXP_DEVSTA_TRPND	0x0020	/* Transactions Pending */
#define RTE_PCI_EXP_LNKCTL		0x10	/* Link Control */
#define RTE_PCI_EXP_LNKSTA		0x12	/* Link Status */
#define RTE_PCI_EXP_LNKSTA_CLS		0x000f	/* Current Link Speed */
#define RTE_PCI_EXP_LNKSTA_NLW		0x03f0	/* Negotiated Link Width */
#define RTE_PCI_EXP_SLTCTL		0x18	/* Slot Control */
#define RTE_PCI_EXP_RTCTL		0x1c	/* Root Control */
#define RTE_PCI_EXP_DEVCTL2		0x28	/* Device Control 2 */
#define RTE_PCI_EXP_LNKCTL2		0x30	/* Link Control 2 */
#define RTE_PCI_EXP_SLTCTL2		0x38	/* Slot Control 2 */

/* MSI-X registers (RTE_PCI_CAP_ID_MSIX) */
#define RTE_PCI_MSIX_FLAGS		2	/* Message Control */
#define RTE_PCI_MSIX_FLAGS_QSIZE	0x07ff	/* Table size */
#define RTE_PCI_MSIX_FLAGS_MASKALL	0x4000	/* Mask all vectors for this function */
#define RTE_PCI_MSIX_FLAGS_ENABLE	0x8000	/* MSI-X enable */

#define RTE_PCI_MSIX_TABLE		4	/* Table offset */
#define RTE_PCI_MSIX_TABLE_BIR		0x00000007 /* BAR index */
#define RTE_PCI_MSIX_TABLE_OFFSET	0xfffffff8 /* Offset into specified BAR */

/* Extended Capabilities (PCI-X 2.0 and Express) */
#define RTE_PCI_EXT_CAP_ID(header)	(header & 0x0000ffff)
#define RTE_PCI_EXT_CAP_NEXT(header)	((header >> 20) & 0xffc)

#define RTE_PCI_EXT_CAP_ID_ERR		0x01	/* Advanced Error Reporting */
#define RTE_PCI_EXT_CAP_ID_DSN		0x03	/* Device Serial Number */
#define RTE_PCI_EXT_CAP_ID_ACS		0x0d	/* Access Control Services */
#define RTE_PCI_EXT_CAP_ID_SRIOV	0x10	/* SR-IOV */
#define RTE_PCI_EXT_CAP_ID_PRI		0x13	/* Page Request Interface */

/* Advanced Error Reporting (RTE_PCI_EXT_CAP_ID_ERR) */
#define RTE_PCI_ERR_UNCOR_STATUS	0x04	/* Uncorrectable Error Status */
#define RTE_PCI_ERR_COR_STATUS		0x10	/* Correctable Error Status */
#define RTE_PCI_ERR_ROOT_STATUS		0x30

/* Access Control Service (RTE_PCI_EXT_CAP_ID_ACS) */
#define RTE_PCI_ACS_CAP			0x04	/* ACS Capability Register */
#define RTE_PCI_ACS_CTRL		0x06	/* ACS Control Register */
#define RTE_PCI_ACS_SV			0x0001	/* Source Validation */
#define RTE_PCI_ACS_RR			0x0004	/* P2P Request Redirect */
#define RTE_PCI_ACS_CR			0x0008	/* P2P Completion Redirect */
#define RTE_PCI_ACS_UF			0x0010	/* Upstream Forwarding */
#define RTE_PCI_ACS_EC			0x0020	/* P2P Egress Control */

/* Single Root I/O Virtualization (RTE_PCI_EXT_CAP_ID_SRIOV) */
#define RTE_PCI_SRIOV_CAP		0x04	/* SR-IOV Capabilities */
#define RTE_PCI_SRIOV_CTRL		0x08	/* SR-IOV Control */
#define RTE_PCI_SRIOV_INITIAL_VF	0x0c	/* Initial VFs */
#define RTE_PCI_SRIOV_TOTAL_VF		0x0e	/* Total VFs */
#define RTE_PCI_SRIOV_NUM_VF		0x10	/* Number of VFs */
#define RTE_PCI_SRIOV_FUNC_LINK		0x12	/* Function Dependency Link */
#define RTE_PCI_SRIOV_VF_OFFSET		0x14	/* First VF Offset */
#define RTE_PCI_SRIOV_VF_STRIDE		0x16	/* Following VF Stride */
#define RTE_PCI_SRIOV_VF_DID		0x1a	/* VF Device ID */
#define RTE_PCI_SRIOV_SUP_PGSIZE	0x1c	/* Supported Page Sizes */

/* Page Request Interface (RTE_PCI_EXT_CAP_ID_PRI) */
#define RTE_PCI_PRI_CTRL		0x04	/* PRI control register */
#define RTE_PCI_PRI_CTRL_ENABLE		0x0001	/* Enable */
#define RTE_PCI_PRI_ALLOC_REQ		0x0c	/* PRI max reqs allowed */

/** Formatting string for PCI device identifier: Ex: 0000:00:01.0 */
#define PCI_PRI_FMT "%.4" PRIx32 ":%.2" PRIx8 ":%.2" PRIx8 ".%" PRIx8
#define PCI_PRI_STR_SIZE sizeof("XXXXXXXX:XX:XX.X")

/** Short formatting string, without domain, for PCI device: Ex: 00:01.0 */
#define PCI_SHORT_PRI_FMT "%.2" PRIx8 ":%.2" PRIx8 ".%" PRIx8

/** Nb. of values in PCI device identifier format string. */
#define PCI_FMT_NVAL 4

/** Nb. of values in PCI resource format. */
#define PCI_RESOURCE_FMT_NVAL 3

/** Maximum number of PCI resources. */
#define PCI_MAX_RESOURCE 6

/**
 * A structure describing an ID for a PCI driver. Each driver provides a
 * table of these IDs for each device that it supports.
 */
struct rte_pci_id {
	uint32_t class_id;            /**< Class ID or RTE_CLASS_ANY_ID. */
	uint16_t vendor_id;           /**< Vendor ID or RTE_PCI_ANY_ID. */
	uint16_t device_id;           /**< Device ID or RTE_PCI_ANY_ID. */
	uint16_t subsystem_vendor_id; /**< Subsystem vendor ID or RTE_PCI_ANY_ID. */
	uint16_t subsystem_device_id; /**< Subsystem device ID or RTE_PCI_ANY_ID. */
};

/**
 * A structure describing the location of a PCI device.
 */
struct rte_pci_addr {
	uint32_t domain;                /**< Device domain */
	uint8_t bus;                    /**< Device bus */
	uint8_t devid;                  /**< Device ID */
	uint8_t function;               /**< Device function. */
};

/** Any PCI device identifier (vendor, device, ...) */
#define RTE_PCI_ANY_ID (0xffff)
/** @deprecated Replaced with RTE_PCI_ANY_ID */
#define PCI_ANY_ID RTE_DEPRECATED(PCI_ANY_ID) RTE_PCI_ANY_ID
#define RTE_CLASS_ANY_ID (0xffffff)

/**
 * Utility function to write a pci device name, this device name can later be
 * used to retrieve the corresponding rte_pci_addr using rte_pci_addr_parse().
 *
 * @param addr
 *	The PCI Bus-Device-Function address
 * @param output
 *	The output buffer string
 * @param size
 *	The output buffer size
 */
void rte_pci_device_name(const struct rte_pci_addr *addr,
		     char *output, size_t size);

/**
 * Utility function to compare two PCI device addresses.
 *
 * @param addr
 *	The PCI Bus-Device-Function address to compare
 * @param addr2
 *	The PCI Bus-Device-Function address to compare
 * @return
 *	0 on equal PCI address.
 *	Positive on addr is greater than addr2.
 *	Negative on addr is less than addr2, or error.
 */
int rte_pci_addr_cmp(const struct rte_pci_addr *addr,
		     const struct rte_pci_addr *addr2);


/**
 * Utility function to parse a string into a PCI location.
 *
 * @param str
 *	The string to parse
 * @param addr
 *	The reference to the structure where the location
 *	is stored.
 * @return
 *	0 on success
 *	<0 otherwise
 */
int rte_pci_addr_parse(const char *str, struct rte_pci_addr *addr);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_PCI_H_ */
