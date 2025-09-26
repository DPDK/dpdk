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

static void nbl_dev_mailbox_interrupt_handler(__rte_unused void *cn_arg)
{
	struct nbl_dev_mgt *dev_mgt = (struct nbl_dev_mgt *)cn_arg;
	const struct nbl_channel_ops *chan_ops = NBL_DEV_MGT_TO_CHAN_OPS(dev_mgt);

	chan_ops->notify_interrupt(NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt));
}

static int nbl_dev_common_start(struct nbl_dev_mgt *dev_mgt)
{
	struct nbl_dispatch_ops *disp_ops = NBL_DEV_MGT_TO_DISP_OPS(dev_mgt);
	const struct nbl_channel_ops *chan_ops = NBL_DEV_MGT_TO_CHAN_OPS(dev_mgt);
	struct nbl_dev_net_mgt *net_dev = NBL_DEV_MGT_TO_NET_DEV(dev_mgt);
	struct nbl_common_info *common = NBL_DEV_MGT_TO_COMMON(dev_mgt);
	struct nbl_board_port_info *board_info;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(net_dev->eth_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	u8 *mac;
	int ret;

	board_info = &dev_mgt->common->board_info;
	disp_ops->get_board_info(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), board_info);
	mac = net_dev->eth_dev->data->mac_addrs->addr_bytes;

	disp_ops->clear_flow(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);

	if (NBL_IS_NOT_COEXISTENCE(common)) {
		ret = disp_ops->configure_msix_map(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), 0, 1, 0);
		if (ret)
			goto configure_msix_map_failed;

		ret = disp_ops->enable_mailbox_irq(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), 0, true);
		if (ret)
			goto enable_mailbox_irq_failed;

		chan_ops->set_queue_state(NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt),
					  NBL_CHAN_INTERRUPT_READY, true);

		ret = rte_intr_callback_register(intr_handle,
						 nbl_dev_mailbox_interrupt_handler, dev_mgt);
		if (ret) {
			NBL_LOG(ERR, "mailbox interrupt handler register failed %d", ret);
			goto rte_intr_callback_register_failed;
		}

		ret = rte_intr_enable(intr_handle);
		if (ret) {
			NBL_LOG(ERR, "rte_intr_enable failed %d", ret);
			goto rte_intr_enable_failed;
		}

		ret = disp_ops->add_macvlan(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt),
					    mac, 0, net_dev->vsi_id);
		if (ret)
			goto add_macvlan_failed;

		ret = disp_ops->add_multi_rule(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);
		if (ret)
			goto add_multi_rule_failed;
	}

	return 0;

add_multi_rule_failed:
	if (NBL_IS_NOT_COEXISTENCE(common))
		disp_ops->del_macvlan(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), mac, 0, net_dev->vsi_id);
add_macvlan_failed:
	if (NBL_IS_NOT_COEXISTENCE(common))
		rte_intr_disable(intr_handle);
rte_intr_enable_failed:
	if (NBL_IS_NOT_COEXISTENCE(common))
		rte_intr_callback_unregister(intr_handle,
					     nbl_dev_mailbox_interrupt_handler, dev_mgt);
rte_intr_callback_register_failed:
	if (NBL_IS_NOT_COEXISTENCE(common)) {
		chan_ops->set_queue_state(NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt),
					  NBL_CHAN_INTERRUPT_READY, false);
		disp_ops->enable_mailbox_irq(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), 0, false);
	}
enable_mailbox_irq_failed:
	if (NBL_IS_NOT_COEXISTENCE(common))
		disp_ops->destroy_msix_map(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt));
configure_msix_map_failed:
	return ret;
}

static int nbl_dev_leonis_start(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_dev_mgt *dev_mgt = NBL_ADAPTER_TO_DEV_MGT(adapter);
	int ret = 0;

	dev_mgt->common = NBL_ADAPTER_TO_COMMON(adapter);
	ret = nbl_dev_common_start(dev_mgt);
	if (ret)
		return ret;
	return 0;
}

static void nbl_dev_leonis_stop(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_dev_mgt *dev_mgt = NBL_ADAPTER_TO_DEV_MGT(adapter);
	struct nbl_dev_net_mgt *net_dev = dev_mgt->net_dev;
	struct nbl_common_info *common = dev_mgt->common;
	struct nbl_dispatch_ops *disp_ops = NBL_DEV_MGT_TO_DISP_OPS(dev_mgt);
	const struct nbl_channel_ops *chan_ops = NBL_DEV_MGT_TO_CHAN_OPS(dev_mgt);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(net_dev->eth_dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	u8 *mac;

	mac = net_dev->eth_dev->data->mac_addrs->addr_bytes;
	if (NBL_IS_NOT_COEXISTENCE(common)) {
		rte_intr_disable(intr_handle);
		rte_intr_callback_unregister(intr_handle,
					     nbl_dev_mailbox_interrupt_handler, dev_mgt);
		chan_ops->set_queue_state(NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt),
					  NBL_CHAN_INTERRUPT_READY, false);
		/* wake up pipe read in nbl_chan_thread_polling_task */
		chan_ops->notify_interrupt(NBL_DEV_MGT_TO_CHAN_PRIV(dev_mgt));
		disp_ops->enable_mailbox_irq(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), 0, false);
		disp_ops->destroy_msix_map(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt));
		disp_ops->del_multi_rule(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);
		disp_ops->del_macvlan(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), mac, 0, net_dev->vsi_id);
	}
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

static int nbl_dev_setup_rings(struct nbl_dev_ring_mgt *ring_mgt)
{
	int i;
	u8 ring_num = ring_mgt->rx_ring_num;

	ring_num = ring_mgt->rx_ring_num;
	ring_mgt->rx_rings = rte_calloc("nbl_dev_rxring", ring_num,
					sizeof(*ring_mgt->rx_rings), 0);
	if (!ring_mgt->rx_rings)
		return -ENOMEM;

	for (i = 0; i < ring_num; i++)
		ring_mgt->rx_rings[i].index = i;

	ring_num = ring_mgt->tx_ring_num;
	ring_mgt->tx_rings = rte_calloc("nbl_dev_txring", ring_num,
					sizeof(*ring_mgt->tx_rings), 0);
	if (!ring_mgt->tx_rings) {
		rte_free(ring_mgt->rx_rings);
		ring_mgt->rx_rings = NULL;
		return -ENOMEM;
	}

	for (i = 0; i < ring_num; i++)
		ring_mgt->tx_rings[i].index = i;

	return 0;
}

static void nbl_dev_remove_rings(struct nbl_dev_ring_mgt *ring_mgt)
{
	rte_free(ring_mgt->rx_rings);
	ring_mgt->rx_rings = NULL;

	rte_free(ring_mgt->tx_rings);
	ring_mgt->tx_rings = NULL;
}

static void nbl_dev_remove_net_dev(struct nbl_dev_mgt *dev_mgt)
{
	struct nbl_dev_net_mgt *net_dev = NBL_DEV_MGT_TO_NET_DEV(dev_mgt);
	struct nbl_dev_ring_mgt *ring_mgt = &net_dev->ring_mgt;
	struct nbl_dispatch_ops *disp_ops = NBL_DEV_MGT_TO_DISP_OPS(dev_mgt);

	disp_ops->remove_rss(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);
	disp_ops->remove_q2vsi(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);
	disp_ops->free_txrx_queues(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);
	disp_ops->remove_rings(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt));
	nbl_dev_remove_rings(ring_mgt);
	disp_ops->unregister_net(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt));

	rte_free(net_dev);
	NBL_DEV_MGT_TO_NET_DEV(dev_mgt) = NULL;
}

static int nbl_dev_setup_net_dev(struct nbl_dev_mgt *dev_mgt,
				 const struct rte_eth_dev *eth_dev,
				 struct nbl_common_info *common)
{
	struct nbl_dev_net_mgt *net_dev;
	struct nbl_dispatch_ops *disp_ops = NBL_DEV_MGT_TO_DISP_OPS(dev_mgt);
	struct nbl_register_net_param register_param = { 0 };
	struct nbl_register_net_result register_result = { 0 };
	struct nbl_dev_ring_mgt *ring_mgt;
	const struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret = 0;

	net_dev = rte_zmalloc("nbl_dev_net", sizeof(struct nbl_dev_net_mgt), 0);
	if (!net_dev)
		return -ENOMEM;

	NBL_DEV_MGT_TO_NET_DEV(dev_mgt) = net_dev;
	NBL_DEV_MGT_TO_ETH_DEV(dev_mgt) = eth_dev;
	ring_mgt = &net_dev->ring_mgt;

	register_param.pf_bar_start = pci_dev->mem_resource[0].phys_addr;
	ret = disp_ops->register_net(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt),
				     &register_param, &register_result);
	if (ret)
		goto register_net_failed;

	ring_mgt->tx_ring_num = register_result.tx_queue_num;
	ring_mgt->rx_ring_num = register_result.rx_queue_num;
	ring_mgt->queue_offset = register_result.queue_offset;

	net_dev->vsi_id = disp_ops->get_vsi_id(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt));
	disp_ops->get_eth_id(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id,
			     &net_dev->eth_mode, &net_dev->eth_id);
	net_dev->trust = register_result.trusted;

	if (net_dev->eth_mode == NBL_TWO_ETHERNET_PORT)
		net_dev->max_mac_num = NBL_TWO_ETHERNET_MAX_MAC_NUM;
	else if (net_dev->eth_mode == NBL_FOUR_ETHERNET_PORT)
		net_dev->max_mac_num = NBL_FOUR_ETHERNET_MAX_MAC_NUM;

	common->vsi_id = net_dev->vsi_id;
	common->eth_id = net_dev->eth_id;

	disp_ops->clear_queues(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);
	disp_ops->register_vsi2q(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), NBL_VSI_DATA, net_dev->vsi_id,
				 register_result.queue_offset, ring_mgt->tx_ring_num);
	ret = nbl_dev_setup_rings(ring_mgt);
	if (ret)
		goto setup_rings_failed;

	ret = disp_ops->alloc_rings(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt),
				    register_result.tx_queue_num,
				    register_result.rx_queue_num,
				    register_result.queue_offset);
	if (ret) {
		NBL_LOG(ERR, "alloc_rings failed ret %d", ret);
		goto alloc_rings_failed;
	}

	ret = disp_ops->alloc_txrx_queues(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt),
					  net_dev->vsi_id,
					  register_result.tx_queue_num);
	if (ret) {
		NBL_LOG(ERR, "alloc_txrx_queues failed ret %d", ret);
		goto alloc_txrx_queues_failed;
	}

	ret = disp_ops->setup_q2vsi(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt), net_dev->vsi_id);
	if (ret) {
		NBL_LOG(ERR, "setup_q2vsi failed ret %d", ret);
		goto setup_q2vsi_failed;
	}

	ret = disp_ops->setup_rss(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt),
				  net_dev->vsi_id);

	return ret;

setup_q2vsi_failed:
	disp_ops->free_txrx_queues(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt),
				   net_dev->vsi_id);
alloc_txrx_queues_failed:
	disp_ops->remove_rings(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt));
alloc_rings_failed:
	nbl_dev_remove_rings(ring_mgt);
setup_rings_failed:
	disp_ops->unregister_net(NBL_DEV_MGT_TO_DISP_PRIV(dev_mgt));
register_net_failed:
	rte_free(net_dev);

	return ret;
}

int nbl_dev_init(void *p, __rte_unused const struct rte_eth_dev *eth_dev)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_dev_mgt **dev_mgt;
	struct nbl_dev_ops_tbl **dev_ops_tbl;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_dispatch_ops_tbl *dispatch_ops_tbl;
	const struct nbl_product_dev_ops *product_dev_ops = NULL;
	struct nbl_common_info *common = NULL;
	struct nbl_dispatch_ops *disp_ops;
	int ret = 0;

	dev_mgt = (struct nbl_dev_mgt **)&NBL_ADAPTER_TO_DEV_MGT(adapter);
	dev_ops_tbl = &NBL_ADAPTER_TO_DEV_OPS_TBL(adapter);
	chan_ops_tbl = NBL_ADAPTER_TO_CHAN_OPS_TBL(adapter);
	dispatch_ops_tbl = NBL_ADAPTER_TO_DISP_OPS_TBL(adapter);
	common = NBL_ADAPTER_TO_COMMON(adapter);
	product_dev_ops = nbl_dev_get_product_ops(adapter->caps.product_type);

	*dev_mgt = rte_zmalloc("nbl_dev_mgt", sizeof(struct nbl_dev_mgt), 0);
	if (*dev_mgt == NULL) {
		NBL_LOG(ERR, "Failed to allocate nbl_dev_mgt memory");
		return -ENOMEM;
	}

	NBL_DEV_MGT_TO_CHAN_OPS_TBL(*dev_mgt) = chan_ops_tbl;
	NBL_DEV_MGT_TO_DISP_OPS_TBL(*dev_mgt) = dispatch_ops_tbl;
	disp_ops = NBL_DEV_MGT_TO_DISP_OPS(*dev_mgt);

	if (product_dev_ops->dev_init)
		ret = product_dev_ops->dev_init(adapter);

	if (ret)
		goto init_dev_failed;

	ret = nbl_dev_setup_ops(dev_ops_tbl, adapter);
	if (ret)
		goto set_ops_failed;

	ret = nbl_dev_setup_net_dev(*dev_mgt, eth_dev, common);
	if (ret)
		goto setup_net_dev_failed;

	eth_dev->data->mac_addrs =
		rte_zmalloc("nbl", RTE_ETHER_ADDR_LEN * (*dev_mgt)->net_dev->max_mac_num, 0);
	if (!eth_dev->data->mac_addrs) {
		NBL_LOG(ERR, "allocate memory to store mac addr failed");
		ret = -ENOMEM;
		goto alloc_mac_addrs_failed;
	}
	disp_ops->get_mac_addr(NBL_DEV_MGT_TO_DISP_PRIV(*dev_mgt),
			       eth_dev->data->mac_addrs[0].addr_bytes);

	adapter->state = NBL_ETHDEV_INITIALIZED;

	return 0;

alloc_mac_addrs_failed:
	nbl_dev_remove_net_dev(*dev_mgt);
setup_net_dev_failed:
	nbl_dev_remove_ops(dev_ops_tbl);
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
	const struct rte_eth_dev *eth_dev;

	dev_mgt = (struct nbl_dev_mgt **)&NBL_ADAPTER_TO_DEV_MGT(adapter);
	dev_ops_tbl = &NBL_ADAPTER_TO_DEV_OPS_TBL(adapter);
	product_dev_ops = nbl_dev_get_product_ops(adapter->caps.product_type);
	eth_dev = (*dev_mgt)->net_dev->eth_dev;

	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;

	nbl_dev_remove_net_dev(*dev_mgt);
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
