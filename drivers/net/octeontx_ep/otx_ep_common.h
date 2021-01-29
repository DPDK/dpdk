/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#ifndef _OTX_EP_COMMON_H_
#define _OTX_EP_COMMON_H_

/* OTX_EP EP VF device data structure */
struct otx_ep_device {
	/* PCI device pointer */
	struct rte_pci_device *pdev;

	struct rte_eth_dev *eth_dev;
};
#endif  /* _OTX_EP_COMMON_H_ */
