/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_DEV_H__
#define __NFP_DEV_H__

#include <stdint.h>

#define PCI_VENDOR_ID_NETRONOME         0x19ee
#define PCI_VENDOR_ID_CORIGINE          0x1da8

#define PCI_DEVICE_ID_NFP3800_PF_NIC    0x3800
#define PCI_DEVICE_ID_NFP3800_VF_NIC    0x3803
#define PCI_DEVICE_ID_NFP4000_PF_NIC    0x4000
#define PCI_DEVICE_ID_NFP6000_PF_NIC    0x6000
#define PCI_DEVICE_ID_NFP6000_VF_NIC    0x6003  /* Include NFP4000VF */

enum nfp_dev_id {
	NFP_DEV_NFP3800,
	NFP_DEV_NFP3800_VF,
	NFP_DEV_NFP6000,
	NFP_DEV_NFP6000_VF,
	NFP_DEV_CNT,
};

struct nfp_dev_info {
	/* Required fields */
	uint32_t qc_idx_mask;
	uint32_t qc_addr_offset;
	uint32_t min_qc_size;
	uint32_t max_qc_size;

	/* PF-only fields */
	const char *chip_names;
	uint32_t pcie_cfg_expbar_offset;
	uint32_t qc_area_sz;
	uint8_t pf_num_per_unit;
};

const struct nfp_dev_info *nfp_dev_info_get(uint16_t device_id);

#endif /* __NFP_DEV_H__ */
