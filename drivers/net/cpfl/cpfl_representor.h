/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_REPRESENTOR_H_
#define _CPFL_REPRESENTOR_H_

#include <ethdev_pci.h>
#include <rte_ethdev.h>

struct cpfl_repr_id {
	uint8_t host_id;
	uint8_t pf_id;
	uint8_t type;
	uint8_t vf_id;
};

struct cpfl_repr_param {
	struct cpfl_adapter_ext *adapter;
	struct cpfl_repr_id repr_id;
	struct cpfl_vport_info *vport_info;
};

int cpfl_repr_devargs_process(struct cpfl_adapter_ext *adapter);
int cpfl_repr_create(struct rte_pci_device *pci_dev, struct cpfl_adapter_ext *adapter);
#endif
