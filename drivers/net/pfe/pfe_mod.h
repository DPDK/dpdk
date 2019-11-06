/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2018-2019 NXP
 */

#ifndef _PFE_MOD_H_
#define _PFE_MOD_H_

struct pfe;

#include <rte_ethdev.h>

#include "pfe.h"
#include "pfe_hif.h"
#include "pfe_hif_lib.h"
#include "pfe_eth.h"

#define PHYID_MAX_VAL 32

/* PFE DPDK driver supports two interfaces.
 */
#define PFE_CDEV_ETH_COUNT 2

/* PFE DPDK driver needs a kernel module named "pfe.ko", This module
 * is required for PHY initialisation and creates a character device
 * "pfe_us_cdev" for IOCTL support. PFE DPDK driver uses this character
 * device for link status.
 */
#define PFE_CDEV_PATH		"/dev/pfe_us_cdev"
#define PFE_CDEV_INVALID_FD	-1
#define PFE_NAME_PMD		net_pfe

/* used when 'read' call is issued, returning PFE_CDEV_ETH_COUNT number of
 * pfe_shared_info as array.
 */
struct pfe_shared_info {
	uint32_t phy_id; /* Link phy ID */
	uint8_t state;  /* Has either 0 or 1 */
};

struct pfe_eth {
	struct pfe_eth_priv_s *eth_priv[PFE_CDEV_ETH_COUNT];
};

struct pfe {
	uint64_t ddr_phys_baseaddr;
	void *ddr_baseaddr;
	uint64_t ddr_size;
	void *cbus_baseaddr;
	uint64_t cbus_size;
	struct ls1012a_pfe_platform_data platform_data;
	struct pfe_hif hif;
	struct pfe_eth eth;
	struct hif_client_s *hif_client[HIF_CLIENTS_MAX];
	int mdio_muxval[PHYID_MAX_VAL];
	uint8_t nb_devs;
	uint8_t max_intf;
	int cdev_fd;
};

/* IOCTL Commands */
#define PFE_CDEV_ETH0_STATE_GET		_IOR('R', 0, int)
#define PFE_CDEV_ETH1_STATE_GET		_IOR('R', 1, int)
#define PFE_CDEV_HIF_INTR_EN		_IOWR('R', 2, int)
#endif /* _PFE_MOD_H */
