/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _OTX2_EP_RAWDEV_H_
#define _OTX2_EP_RAWDEV_H_

#define PCI_DEVID_OCTEONTX2_EP_VF    0xB203  /* OCTEON TX2 EP mode */

/* SDP EP VF device */
struct sdp_device {
	/* PCI device pointer */
	struct rte_pci_device *pci_dev;
	uint16_t vf_num;

	/* Memory mapped h/w address */
	uint8_t *hw_addr;

};

#endif /* _OTX2_EP_RAWDEV_H_ */
