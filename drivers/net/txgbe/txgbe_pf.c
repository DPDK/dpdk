/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2015-2020
 */

#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_memcpy.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_bus_pci.h>

#include "base/txgbe.h"
#include "txgbe_ethdev.h"

static inline uint16_t
dev_num_vf(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	return pci_dev->max_vfs;
}

static inline
int txgbe_vf_perm_addr_gen(struct rte_eth_dev *dev, uint16_t vf_num)
{
	unsigned char vf_mac_addr[RTE_ETHER_ADDR_LEN];
	struct txgbe_vf_info *vfinfo = *TXGBE_DEV_VFDATA(dev);
	uint16_t vfn;

	for (vfn = 0; vfn < vf_num; vfn++) {
		rte_eth_random_addr(vf_mac_addr);
		/* keep the random address as default */
		memcpy(vfinfo[vfn].vf_mac_addresses, vf_mac_addr,
			   RTE_ETHER_ADDR_LEN);
	}

	return 0;
}

static inline int
txgbe_mb_intr_setup(struct rte_eth_dev *dev)
{
	struct txgbe_interrupt *intr = TXGBE_DEV_INTR(dev);

	intr->mask_misc |= TXGBE_ICRMISC_VFMBX;

	return 0;
}

void txgbe_pf_host_init(struct rte_eth_dev *eth_dev)
{
	struct txgbe_vf_info **vfinfo = TXGBE_DEV_VFDATA(eth_dev);
	struct txgbe_mirror_info *mirror_info = TXGBE_DEV_MR_INFO(eth_dev);
	struct txgbe_uta_info *uta_info = TXGBE_DEV_UTA_INFO(eth_dev);
	struct txgbe_hw *hw = TXGBE_DEV_HW(eth_dev);
	uint16_t vf_num;
	uint8_t nb_queue;

	PMD_INIT_FUNC_TRACE();

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return;

	*vfinfo = rte_zmalloc("vf_info",
			sizeof(struct txgbe_vf_info) * vf_num, 0);
	if (*vfinfo == NULL)
		rte_panic("Cannot allocate memory for private VF data\n");

	rte_eth_switch_domain_alloc(&(*vfinfo)->switch_domain_id);

	memset(mirror_info, 0, sizeof(struct txgbe_mirror_info));
	memset(uta_info, 0, sizeof(struct txgbe_uta_info));
	hw->mac.mc_filter_type = 0;

	if (vf_num >= ETH_32_POOLS) {
		nb_queue = 2;
		RTE_ETH_DEV_SRIOV(eth_dev).active = ETH_64_POOLS;
	} else if (vf_num >= ETH_16_POOLS) {
		nb_queue = 4;
		RTE_ETH_DEV_SRIOV(eth_dev).active = ETH_32_POOLS;
	} else {
		nb_queue = 8;
		RTE_ETH_DEV_SRIOV(eth_dev).active = ETH_16_POOLS;
	}

	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = nb_queue;
	RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx = vf_num;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx =
			(uint16_t)(vf_num * nb_queue);

	txgbe_vf_perm_addr_gen(eth_dev, vf_num);

	/* init_mailbox_params */
	hw->mbx.init_params(hw);

	/* set mb interrupt mask */
	txgbe_mb_intr_setup(eth_dev);
}

void txgbe_pf_host_uninit(struct rte_eth_dev *eth_dev)
{
	struct txgbe_vf_info **vfinfo;
	uint16_t vf_num;
	int ret;

	PMD_INIT_FUNC_TRACE();

	RTE_ETH_DEV_SRIOV(eth_dev).active = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).nb_q_per_pool = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).def_vmdq_idx = 0;
	RTE_ETH_DEV_SRIOV(eth_dev).def_pool_q_idx = 0;

	vf_num = dev_num_vf(eth_dev);
	if (vf_num == 0)
		return;

	vfinfo = TXGBE_DEV_VFDATA(eth_dev);
	if (*vfinfo == NULL)
		return;

	ret = rte_eth_switch_domain_free((*vfinfo)->switch_domain_id);
	if (ret)
		PMD_INIT_LOG(WARNING, "failed to free switch domain: %d", ret);

	rte_free(*vfinfo);
	*vfinfo = NULL;
}

