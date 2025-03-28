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

struct rnp_proc_priv {
	const struct rnp_mbx_ops *mbx_ops;
};

struct rnp_eth_port {
};

struct rnp_eth_adapter {
	struct rnp_hw hw;
	struct rte_eth_dev *eth_dev; /* alloc eth_dev by platform */
};

#define RNP_DEV_TO_PROC_PRIV(eth_dev) \
	((struct rnp_proc_priv *)(eth_dev)->process_private)

#endif /* __RNP_H__ */
