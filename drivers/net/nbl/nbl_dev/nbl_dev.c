/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_dev.h"

int nbl_dev_configure(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
	return 0;
}

int nbl_dev_port_start(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
	return 0;
}

int nbl_dev_port_stop(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
	return 0;
}

int nbl_dev_port_close(struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(eth_dev);
	return 0;
}

static int nbl_dev_setup_chan_queue(struct nbl_adapter *adapter)
{
	struct nbl_dev_mgt *dev_mgt = NBL_ADAPTER_TO_DEV_MGT(adapter);
	const struct nbl_channel_ops *chan_ops = NBL_DEV_MGT_TO_CHAN_OPS(dev_mgt);
	int ret = 0;

	ret = chan_ops->setup_queue(NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt));

	return ret;
}

static int nbl_dev_teardown_chan_queue(struct nbl_adapter *adapter)
{
	struct nbl_dev_mgt *dev_mgt = NBL_ADAPTER_TO_DEV_MGT(adapter);
	const struct nbl_channel_ops *chan_ops = NBL_DEV_MGT_TO_CHAN_OPS(dev_mgt);
	int ret = 0;

	ret = chan_ops->teardown_queue(NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt));

	return ret;
}

static int nbl_dev_leonis_init(void *adapter)
{
	return nbl_dev_setup_chan_queue((struct nbl_adapter *)adapter);
}

static void nbl_dev_leonis_uninit(void *adapter)
{
	nbl_dev_teardown_chan_queue((struct nbl_adapter *)adapter);
}

static int nbl_dev_leonis_start(void *p)
{
	RTE_SET_USED(p);
	return 0;
}

static void nbl_dev_leonis_stop(void *p)
{
	RTE_SET_USED(p);
}

static void nbl_dev_remove_ops(struct nbl_dev_ops_tbl **dev_ops_tbl)
{
	free(*dev_ops_tbl);
	*dev_ops_tbl = NULL;
}

static int nbl_dev_setup_ops(struct nbl_dev_ops_tbl **dev_ops_tbl,
			     struct nbl_adapter *adapter)
{
	*dev_ops_tbl = calloc(1, sizeof(struct nbl_dev_ops_tbl));
	if (!*dev_ops_tbl)
		return -ENOMEM;

	NBL_DEV_OPS_TBL_TO_OPS(*dev_ops_tbl) = NULL;
	NBL_DEV_OPS_TBL_TO_PRIV(*dev_ops_tbl) = adapter;

	return 0;
}

int nbl_dev_init(void *p, __rte_unused const struct rte_eth_dev *eth_dev)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_dev_mgt **dev_mgt;
	struct nbl_dev_ops_tbl **dev_ops_tbl;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_dispatch_ops_tbl *dispatch_ops_tbl;
	const struct nbl_product_dev_ops *product_dev_ops = NULL;
	int ret = 0;

	dev_mgt = (struct nbl_dev_mgt **)&NBL_ADAPTER_TO_DEV_MGT(adapter);
	dev_ops_tbl = &NBL_ADAPTER_TO_DEV_OPS_TBL(adapter);
	chan_ops_tbl = NBL_ADAPTER_TO_CHAN_OPS_TBL(adapter);
	dispatch_ops_tbl = NBL_ADAPTER_TO_DISP_OPS_TBL(adapter);
	product_dev_ops = nbl_dev_get_product_ops(adapter->caps.product_type);

	*dev_mgt = rte_zmalloc("nbl_dev_mgt", sizeof(struct nbl_dev_mgt), 0);
	if (*dev_mgt == NULL) {
		NBL_LOG(ERR, "Failed to allocate nbl_dev_mgt memory");
		return -ENOMEM;
	}

	NBL_DEV_MGT_TO_CHAN_OPS_TBL(*dev_mgt) = chan_ops_tbl;
	NBL_DEV_MGT_TO_DISP_OPS_TBL(*dev_mgt) = dispatch_ops_tbl;

	if (product_dev_ops->dev_init)
		ret = product_dev_ops->dev_init(adapter);

	if (ret)
		goto init_dev_failed;

	ret = nbl_dev_setup_ops(dev_ops_tbl, adapter);
	if (ret)
		goto set_ops_failed;

	adapter->state = NBL_ETHDEV_INITIALIZED;

	return 0;

set_ops_failed:
	if (product_dev_ops->dev_uninit)
		product_dev_ops->dev_uninit(adapter);
init_dev_failed:
	rte_free(*dev_mgt);
	*dev_mgt = NULL;
	return ret;
}

void nbl_dev_remove(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_dev_mgt **dev_mgt;
	struct nbl_dev_ops_tbl **dev_ops_tbl;
	const struct nbl_product_dev_ops *product_dev_ops = NULL;

	dev_mgt = (struct nbl_dev_mgt **)&NBL_ADAPTER_TO_DEV_MGT(adapter);
	dev_ops_tbl = &NBL_ADAPTER_TO_DEV_OPS_TBL(adapter);
	product_dev_ops = nbl_dev_get_product_ops(adapter->caps.product_type);

	nbl_dev_remove_ops(dev_ops_tbl);
	if (product_dev_ops->dev_uninit)
		product_dev_ops->dev_uninit(adapter);

	rte_free(*dev_mgt);
	*dev_mgt = NULL;
}

void nbl_dev_stop(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	const struct nbl_product_dev_ops *product_dev_ops = NULL;

	product_dev_ops = nbl_dev_get_product_ops(adapter->caps.product_type);
	if (product_dev_ops->dev_stop)
		return product_dev_ops->dev_stop(p);
}

int nbl_dev_start(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	const struct nbl_product_dev_ops *product_dev_ops = NULL;

	product_dev_ops = nbl_dev_get_product_ops(adapter->caps.product_type);
	if (product_dev_ops->dev_start)
		return product_dev_ops->dev_start(p);
	return 0;
}

const struct nbl_product_dev_ops nbl_product_dev_ops[NBL_PRODUCT_MAX] = {
	[NBL_LEONIS_TYPE] = {
		.dev_init	= nbl_dev_leonis_init,
		.dev_uninit	= nbl_dev_leonis_uninit,
		.dev_start	= nbl_dev_leonis_start,
		.dev_stop	= nbl_dev_leonis_stop,
	},
};

const struct nbl_product_dev_ops *nbl_dev_get_product_ops(enum nbl_product_type product_type)
{
	RTE_ASSERT(product_type < NBL_PRODUCT_MAX);
	return &nbl_product_dev_ops[product_type];
}
