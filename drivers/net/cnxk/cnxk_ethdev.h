/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef __CNXK_ETHDEV_H__
#define __CNXK_ETHDEV_H__

#include <math.h>
#include <stdint.h>

#include <ethdev_driver.h>
#include <ethdev_pci.h>

#include "roc_api.h"

#define CNXK_ETH_DEV_PMD_VERSION "1.0"

struct cnxk_eth_dev {
	/* ROC NIX */
	struct roc_nix nix;

	/* Max macfilter entries */
	uint8_t max_mac_entries;

	uint16_t flags;

	/* Pointer back to rte */
	struct rte_eth_dev *eth_dev;

	/* HW capabilities / Limitations */
	union {
		uint64_t hwcap;
	};

	/* Rx and Tx offload capabilities */
	uint64_t rx_offload_capa;
	uint64_t tx_offload_capa;
	uint32_t speed_capa;

	/* Default mac address */
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
};

static inline struct cnxk_eth_dev *
cnxk_eth_pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

/* Common ethdev ops */
extern struct eth_dev_ops cnxk_eth_dev_ops;

/* Ops */
int cnxk_nix_probe(struct rte_pci_driver *pci_drv,
		   struct rte_pci_device *pci_dev);
int cnxk_nix_remove(struct rte_pci_device *pci_dev);

#endif /* __CNXK_ETHDEV_H__ */
