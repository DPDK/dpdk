/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_core.h"

const struct nbl_product_core_ops nbl_product_core_ops[NBL_PRODUCT_MAX] = {
	[NBL_LEONIS_TYPE] = {
		.hw_init	= nbl_hw_init_leonis_snic,
		.hw_remove	= nbl_hw_remove_leonis_snic,
		.res_init	= nbl_res_init_leonis,
		.res_remove	= nbl_res_remove_leonis,
		.chan_init	= nbl_chan_init_leonis,
		.chan_remove	= nbl_chan_remove_leonis,
	},
};

static const struct nbl_product_core_ops *
nbl_core_get_product_ops(enum nbl_product_type product_type)
{
	RTE_ASSERT(product_type < NBL_PRODUCT_MAX);
	return &nbl_product_core_ops[product_type];
}

static void nbl_init_func_caps(const struct rte_pci_device *pci_dev, struct nbl_func_caps *caps)
{
	caps->product_type = NBL_PRODUCT_MAX;
	if (pci_dev->id.device_id >= NBL_DEVICE_ID_M18110 &&
	    pci_dev->id.device_id <= NBL_DEVICE_ID_M18100_VF)
		caps->product_type = NBL_LEONIS_TYPE;
}

int nbl_core_init(struct nbl_adapter *adapter, const struct rte_eth_dev *eth_dev)
{
	const struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	const struct nbl_product_core_ops *product_base_ops = NULL;
	struct nbl_common_info *common = NBL_ADAPTER_TO_COMMON(adapter);
	int ret = 0;

	common->eth_dev = eth_dev;
	nbl_init_func_caps(pci_dev, &adapter->caps);

	product_base_ops = nbl_core_get_product_ops(adapter->caps.product_type);

	/* every product's hw/chan/res layer has a great difference, so call their own init ops */
	ret = product_base_ops->hw_init(adapter);
	if (ret)
		goto hw_init_fail;

	ret = product_base_ops->chan_init(adapter);
	if (ret)
		goto chan_init_fail;

	ret = product_base_ops->res_init(adapter, eth_dev);
	if (ret)
		goto res_init_fail;

	ret = nbl_disp_init(adapter);
	if (ret)
		goto disp_init_fail;

	ret = nbl_dev_init(adapter, eth_dev);
	if (ret)
		goto dev_init_fail;

	return 0;

dev_init_fail:
	nbl_disp_remove(adapter);
disp_init_fail:
	product_base_ops->res_remove(adapter);
res_init_fail:
	product_base_ops->chan_remove(adapter);
chan_init_fail:
	product_base_ops->hw_remove(adapter);
hw_init_fail:
	return ret;
}

void nbl_core_remove(struct nbl_adapter *adapter)
{
	const struct nbl_product_core_ops *product_base_ops = NULL;

	product_base_ops = nbl_core_get_product_ops(adapter->caps.product_type);

	nbl_dev_remove(adapter);
	nbl_disp_remove(adapter);
	product_base_ops->res_remove(adapter);
	product_base_ops->chan_remove(adapter);
	product_base_ops->hw_remove(adapter);
}

int nbl_core_start(struct nbl_adapter *adapter)
{
	int ret = 0;

	ret = nbl_dev_start(adapter);
	return ret;
}

void nbl_core_stop(struct nbl_adapter *adapter)
{
	nbl_dev_stop(adapter);
}
