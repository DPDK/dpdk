/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 HiSilicon Limited
 */

#ifndef HISI_DMADEV_H
#define HISI_DMADEV_H

#define PCI_VENDOR_ID_HUAWEI			0x19e5
#define HISI_DMA_DEVICE_ID			0xA122
#define HISI_DMA_PCI_REVISION_ID_REG		0x08
#define HISI_DMA_REVISION_HIP08B		0x21

/**
 * The HIP08B(HiSilicon IP08) and later Chip(e.g. HiSilicon IP09) are DMA iEPs,
 * they have the same pci device id but with different pci revision.
 * Unfortunately, they have different register layouts, so the layout
 * enumerations are defined.
 */
enum {
	HISI_DMA_REG_LAYOUT_INVALID = 0,
	HISI_DMA_REG_LAYOUT_HIP08
};

#endif /* HISI_DMADEV_H */
