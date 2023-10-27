/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_dev.h"

#include <nfp_platform.h>
#include <rte_bitops.h>

/*
 * Note: The value of 'max_qc_size' is different from kernel driver,
 * because DPDK use 'uint16_t' as the data type.
 */
const struct nfp_dev_info nfp_dev_info[NFP_DEV_CNT] = {
	[NFP_DEV_NFP3800] = {
		.qc_idx_mask            = GENMASK(8, 0),
		.qc_addr_offset         = 0x400000,
		.min_qc_size            = 512,
		.max_qc_size            = RTE_BIT32(15),    /**< 32K */

		.chip_names             = "NFP3800",
		.pcie_cfg_expbar_offset = 0x0a00,
		.qc_area_sz             = 0x100000,
		.pf_num_per_unit        = 4,
	},
	[NFP_DEV_NFP3800_VF] = {
		.qc_idx_mask            = GENMASK(8, 0),
		.qc_addr_offset         = 0,
		.min_qc_size            = 512,
		.max_qc_size            = RTE_BIT32(15),    /**< 32K */
	},
	[NFP_DEV_NFP6000] = {
		.qc_idx_mask            = GENMASK(7, 0),
		.qc_addr_offset         = 0x80000,
		.min_qc_size            = 256,
		.max_qc_size            = RTE_BIT32(15),    /**< 32K */

		.chip_names             = "NFP4000/NFP6000",
		.pcie_cfg_expbar_offset = 0x0400,
		.qc_area_sz             = 0x80000,
		.pf_num_per_unit        = 1,
	},
	[NFP_DEV_NFP6000_VF] = {
		.qc_idx_mask            = GENMASK(7, 0),
		.qc_addr_offset         = 0,
		.min_qc_size            = 256,
		.max_qc_size            = RTE_BIT32(15),    /**< 32K */
	},
};

const struct nfp_dev_info *
nfp_dev_info_get(uint16_t device_id)
{
	enum nfp_dev_id id;

	switch (device_id) {
	case PCI_DEVICE_ID_NFP3800_PF_NIC:
		id = NFP_DEV_NFP3800;
		break;
	case PCI_DEVICE_ID_NFP3800_VF_NIC:
		id = NFP_DEV_NFP3800_VF;
		break;
	case PCI_DEVICE_ID_NFP4000_PF_NIC:
	case PCI_DEVICE_ID_NFP6000_PF_NIC:
		id = NFP_DEV_NFP6000;
		break;
	case PCI_DEVICE_ID_NFP6000_VF_NIC:
		id = NFP_DEV_NFP6000_VF;
		break;
	default:
		id = NFP_DEV_CNT;
		break;
	}

	if (id >= NFP_DEV_CNT)
		return NULL;

	return &nfp_dev_info[id];
}
