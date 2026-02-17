/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Cesnet
 * Copyright(c) 2019 Netcope Technologies, a.s. <info@netcope.com>
 * All rights reserved.
 */

#ifndef _NFB_H_
#define _NFB_H_

#include <nfb/nfb.h>
#include <nfb/ndp.h>
#include <netcope/rxmac.h>
#include <netcope/txmac.h>
#include <netcope/mdio_if_info.h>
#include <netcope/info.h>

extern int nfb_logtype;
#define RTE_LOGTYPE_NFB nfb_logtype
#define NFB_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFB, "%s(): ", __func__, __VA_ARGS__)

#include "nfb_rx.h"
#include "nfb_tx.h"

/* PCI Vendor ID */
#define PCI_VENDOR_ID_NETCOPE 0x1b26
#define PCI_VENDOR_ID_SILICOM 0x1c2c
#define PCI_VENDOR_ID_CESNET  0x18ec

/* PCI Device IDs */
#define PCI_DEVICE_ID_NFB_40G2  0xcb80
#define PCI_DEVICE_ID_NFB_100G2 0xc2c1
#define PCI_DEVICE_ID_NFB_200G2QL 0xc250
#define PCI_DEVICE_ID_NFB_200G2QL_E1 0xc251
#define PCI_DEVICE_ID_FB2CGG3   0x00d0
#define PCI_DEVICE_ID_FB2CGHH   0x00d2
#define PCI_DEVICE_ID_FB2CGG3D  0xc240
#define PCI_DEVICE_ID_COMBO400G1 0xc400
#define PCI_DEVICE_ID_CESNET_NDK_COMMON 0xc000

/* Max index of ndp rx/tx queues */
#define RTE_ETH_NDP_MAX_RX_QUEUES 32
#define RTE_ETH_NDP_MAX_TX_QUEUES 32

#define RTE_NFB_DRIVER_NAME net_nfb

/* Device arguments */
#define NFB_ARG_PORT "port"

#define NFB_COMMON_ARGS \
	NFB_ARG_PORT "=<number>" \
	""

struct eth_node {
	struct mdio_if_info if_info;    /**< MDIO interface handles */
};

/*
 * Handles obtained from the libnfb: each process must use own instance.
 * Stored inside dev->process_private.
 */
struct pmd_internals {
	uint16_t         max_rxmac;     /**< Count of valid rxmac items */
	uint16_t         max_txmac;     /**< Count of valid txmac items */
	uint16_t         max_eth;       /**< Count of valid eth nodes */
	struct nc_rxmac  **rxmac;       /**< Array of Rx MAC handles */
	struct nc_txmac  **txmac;       /**< Array of Tx MAC handles */
	struct eth_node	 *eth_node;     /**< Array of Eth nodes */
	struct nfb_device *nfb;

	TAILQ_ENTRY(pmd_internals) eth_dev_list;  /**< Item in list of all devices */
	struct rte_eth_dev *eth_dev;  /**< Handle for matching the device being removed */
};

/*
 * Common data, single instance usable in all processes.
 * Inited in the RTE_PROC_PRIMARY, stored in dev->data->dev_private.
 */
struct pmd_priv {
	uint16_t max_rx_queues;
	uint16_t max_tx_queues;

	/** Mapping from DPDK RX queue index to firmware queue ID */
	int             *queue_map_rx;
	/** Mapping from DPDK TX queue index to firmware queue ID */
	int             *queue_map_tx;
	bool ready;     /**< This structure is initialized for usage in secondary process */
};

/* Data for common device probing */
struct nfb_probe_params {
	/** Generic device information */
	struct rte_device *device;
	/** Port bus specific initialisation callback function */
	ethdev_bus_specific_init specific_init;
	/** Port bus specific initialisation parameters */
	void *specific_device;

	const char *path;       /**< libnfb device string */
	const char *args;	/**< Device arguments */
	int nfb_id;             /**< ID of NFB device in system */
	int ep_index;           /**< (PCIe) endpoint index for matching assigned interfaces */

	char name[RTE_DEV_NAME_MAX_LEN];  /**< Probed name (e.g. PCI device name) */
};


int nfb_eth_common_probe(struct nfb_probe_params *params);
int nfb_eth_common_remove(struct rte_device *dev);

#endif /* _NFB_H_ */
