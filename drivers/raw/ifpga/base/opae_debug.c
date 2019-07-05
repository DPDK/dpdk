/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#define OPAE_HW_DEBUG

#include "opae_hw_api.h"
#include "opae_debug.h"

void opae_manager_dump(struct opae_manager *mgr)
{
	opae_log("=====%s=====\n", __func__);
	opae_log("OPAE Manger %s\n", mgr->name);
	opae_log("OPAE Manger OPs = %p\n", mgr->ops);
	opae_log("OPAE Manager Private Data = %p\n", mgr->data);
	opae_log("OPAE Adapter(parent) = %p\n", mgr->adapter);
	opae_log("==========================\n");
}

void opae_bridge_dump(struct opae_bridge *br)
{
	opae_log("=====%s=====\n", __func__);
	opae_log("OPAE Bridge %s\n", br->name);
	opae_log("OPAE Bridge ID = %d\n", br->id);
	opae_log("OPAE Bridge OPs = %p\n", br->ops);
	opae_log("OPAE Bridge Private Data = %p\n", br->data);
	opae_log("OPAE Accelerator(under this bridge) = %p\n", br->acc);
	opae_log("==========================\n");
}

void opae_accelerator_dump(struct opae_accelerator *acc)
{
	opae_log("=====%s=====\n", __func__);
	opae_log("OPAE Accelerator %s\n", acc->name);
	opae_log("OPAE Accelerator Index = %d\n", acc->index);
	opae_log("OPAE Accelerator OPs = %p\n", acc->ops);
	opae_log("OPAE Accelerator Private Data = %p\n", acc->data);
	opae_log("OPAE Bridge (upstream) = %p\n", acc->br);
	opae_log("OPAE Manager (upstream) = %p\n", acc->mgr);
	opae_log("==========================\n");

	if (acc->br)
		opae_bridge_dump(acc->br);
}

static void opae_adapter_data_dump(void *data)
{
	struct opae_adapter_data *d = data;
	struct opae_adapter_data_pci *d_pci;
	struct opae_reg_region *r;
	int i;

	opae_log("=====%s=====\n", __func__);

	switch (d->type) {
	case OPAE_FPGA_PCI:
		d_pci = (struct opae_adapter_data_pci *)d;

		opae_log("OPAE Adapter Type = PCI\n");
		opae_log("PCI Device ID: 0x%04x\n", d_pci->device_id);
		opae_log("PCI Vendor ID: 0x%04x\n", d_pci->vendor_id);

		for (i = 0; i < PCI_MAX_RESOURCE; i++) {
			r = &d_pci->region[i];
			opae_log("PCI Bar %d: phy(%llx) len(%llx) addr(%p)\n",
				 i, (unsigned long long)r->phys_addr,
				 (unsigned long long)r->len, r->addr);
		}
		break;
	case OPAE_FPGA_NET:
		break;
	}

	opae_log("==========================\n");
}

void opae_adapter_dump(struct opae_adapter *adapter, int verbose)
{
	struct opae_accelerator *acc;

	if (verbose) {
		opae_log("=====%s=====\n", __func__);
		opae_log("OPAE Adapter %s\n", adapter->name);
		opae_log("OPAE Adapter OPs = %p\n", adapter->ops);
		opae_log("OPAE Adapter Private Data = %p\n", adapter->data);
		opae_log("OPAE Manager (downstream) = %p\n", adapter->mgr);

		if (adapter->mgr)
			opae_manager_dump(adapter->mgr);

		opae_adapter_for_each_acc(adapter, acc)
			opae_accelerator_dump(acc);

		if (adapter->data)
			opae_adapter_data_dump(adapter->data);

		opae_log("==========================\n");
	}
}
