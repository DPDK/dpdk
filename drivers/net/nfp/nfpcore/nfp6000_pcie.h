/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Netronome Systems, Inc.
 * All rights reserved.
 */

#ifndef __NFP6000_PCIE_H__
#define __NFP6000_PCIE_H__

#include <ethdev_pci.h>

#include "nfp_cpp.h"
#include "nfp_dev.h"

const struct nfp_cpp_operations *nfp_cpp_transport_operations(void);

struct nfp_cpp *nfp_cpp_from_nfp6000_pcie(struct rte_pci_device *pci_dev,
		const struct nfp_dev_info *dev_info,
		bool driver_lock_needed);

#endif /* __NFP6000_PCIE_H__ */
