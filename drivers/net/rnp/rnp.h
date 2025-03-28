/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Mucse IC Design Ltd.
 */

#ifndef __RNP_H__
#define __RNP_H__

#include <ethdev_driver.h>
#include <rte_interrupts.h>

#include "base/rnp_hw.h"

#define PCI_VENDOR_ID_MUCSE	(0x8848)
#define RNP_DEV_ID_N10G_X2	(0x1000)
#define RNP_DEV_ID_N10G_X4	(0x1020)
#define RNP_DEV_ID_N10G_X8	(0x1060)
#define RNP_MAX_VF_NUM		(64)
#define RNP_MISC_VEC_ID		RTE_INTR_VEC_ZERO_OFFSET
/* maximum frame size supported */
#define RNP_MAC_MAXFRM_SIZE	(9590)

struct rnp_port_attr {
	uint16_t max_mac_addrs;	/* max support mac address */
	uint16_t port_id;	/* platform manage port sequence id */
	uint8_t port_offset;	/* port queue offset */
	uint8_t sw_id;		/* software port init sequence id */
	uint16_t nr_lane;	/* phy lane of This PF:0~3 */
};

struct rnp_proc_priv {
	const struct rnp_mac_ops *mac_ops;
	const struct rnp_mbx_ops *mbx_ops;
};

struct rnp_eth_port {
	struct rnp_proc_priv *proc_priv;
	struct rte_ether_addr mac_addr;
	struct rte_eth_dev *eth_dev;
	struct rnp_port_attr attr;
	struct rnp_hw *hw;
};

struct rnp_eth_adapter {
	struct rnp_hw hw;
	struct rte_pci_device *pdev;
	struct rte_eth_dev *eth_dev; /* alloc eth_dev by platform */

	struct rnp_eth_port *ports[RNP_MAX_PORT_OF_PF];
	uint16_t closed_ports;
	uint16_t inited_ports;
	bool intr_registered;
};

#define RNP_DEV_TO_PORT(eth_dev) \
	((struct rnp_eth_port *)(eth_dev)->data->dev_private)
#define RNP_DEV_TO_ADAPTER(eth_dev) \
	((struct rnp_eth_adapter *)(RNP_DEV_TO_PORT(eth_dev))->hw->back)
#define RNP_DEV_TO_PROC_PRIV(eth_dev) \
	((struct rnp_proc_priv *)(eth_dev)->process_private)
#define RNP_DEV_PP_TO_MBX_OPS(priv) \
	(((RNP_DEV_TO_PROC_PRIV(priv))->mbx_ops))
#define RNP_DEV_PP_TO_MAC_OPS(priv) \
	(((RNP_DEV_TO_PROC_PRIV(priv))->mac_ops))

static inline int
rnp_pf_own_ports(uint32_t id)
{
	return (id == 0) ? 1 : (id == 1) ? 2 : 4;
}

static inline int
rnp_pf_is_multiple_ports(uint32_t device_id)
{
	uint32_t verbit = (device_id >> 5) & 0x3;

	return rnp_pf_own_ports(verbit) == 1 ? 0 : 1;
}

#endif /* __RNP_H__ */
