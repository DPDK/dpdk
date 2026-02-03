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

extern int nfb_logtype;
#define RTE_LOGTYPE_NFB nfb_logtype
#define NFB_LOG(level, ...) \
	RTE_LOG_LINE_PREFIX(level, NFB, "%s(): ", __func__, __VA_ARGS__)

#include "nfb_rx.h"
#include "nfb_tx.h"

/* PCI Vendor ID */
#define PCI_VENDOR_ID_NETCOPE 0x1b26
#define PCI_VENDOR_ID_SILICOM 0x1c2c

/* PCI Device IDs */
#define PCI_DEVICE_ID_NFB_40G2  0xcb80
#define PCI_DEVICE_ID_NFB_100G2 0xc2c1
#define PCI_DEVICE_ID_NFB_200G2QL 0xc250
#define PCI_DEVICE_ID_FB2CGG3   0x00d0
#define PCI_DEVICE_ID_FB2CGG3D  0xc240

/* Max index of ndp rx/tx queues */
#define RTE_ETH_NDP_MAX_RX_QUEUES 32
#define RTE_ETH_NDP_MAX_TX_QUEUES 32

/* Max index of rx/tx dmas */
#define RTE_MAX_NC_RXMAC 256
#define RTE_MAX_NC_TXMAC 256

#define RTE_NFB_DRIVER_NAME net_nfb

/*
 * Handles obtained from the libnfb: each process must use own instance.
 * Stored inside dev->process_private.
 */
struct pmd_internals {
	uint16_t         max_rxmac;
	uint16_t         max_txmac;
	struct nc_rxmac *rxmac[RTE_MAX_NC_RXMAC];
	struct nc_txmac *txmac[RTE_MAX_NC_TXMAC];
	struct nfb_device *nfb;
};

/*
 * Common data, single instance usable in all processes.
 * Inited in the RTE_PROC_PRIMARY, stored in dev->data->dev_private.
 */
struct pmd_priv {
	uint16_t max_rx_queues;
	uint16_t max_tx_queues;
};

#endif /* _NFB_H_ */
