/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include <cnxk_rep.h>

int
cnxk_rep_link_update(struct rte_eth_dev *ethdev, int wait_to_complete)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(wait_to_complete);
	return 0;
}

int
cnxk_rep_dev_info_get(struct rte_eth_dev *ethdev, struct rte_eth_dev_info *devinfo)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(devinfo);
	return 0;
}

int
cnxk_rep_dev_configure(struct rte_eth_dev *ethdev)
{
	PLT_SET_USED(ethdev);
	return 0;
}

int
cnxk_rep_dev_start(struct rte_eth_dev *ethdev)
{
	PLT_SET_USED(ethdev);
	return 0;
}

int
cnxk_rep_dev_close(struct rte_eth_dev *ethdev)
{
	PLT_SET_USED(ethdev);
	return 0;
}

int
cnxk_rep_dev_stop(struct rte_eth_dev *ethdev)
{
	PLT_SET_USED(ethdev);
	return 0;
}

int
cnxk_rep_rx_queue_setup(struct rte_eth_dev *ethdev, uint16_t rx_queue_id, uint16_t nb_rx_desc,
			unsigned int socket_id, const struct rte_eth_rxconf *rx_conf,
			struct rte_mempool *mb_pool)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(rx_queue_id);
	PLT_SET_USED(nb_rx_desc);
	PLT_SET_USED(socket_id);
	PLT_SET_USED(rx_conf);
	PLT_SET_USED(mb_pool);
	return 0;
}

void
cnxk_rep_rx_queue_release(struct rte_eth_dev *ethdev, uint16_t queue_id)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(queue_id);
}

int
cnxk_rep_tx_queue_setup(struct rte_eth_dev *ethdev, uint16_t tx_queue_id, uint16_t nb_tx_desc,
			unsigned int socket_id, const struct rte_eth_txconf *tx_conf)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(tx_queue_id);
	PLT_SET_USED(nb_tx_desc);
	PLT_SET_USED(socket_id);
	PLT_SET_USED(tx_conf);
	return 0;
}

void
cnxk_rep_tx_queue_release(struct rte_eth_dev *ethdev, uint16_t queue_id)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(queue_id);
}

int
cnxk_rep_stats_get(struct rte_eth_dev *ethdev, struct rte_eth_stats *stats)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(stats);
	return 0;
}

int
cnxk_rep_stats_reset(struct rte_eth_dev *ethdev)
{
	PLT_SET_USED(ethdev);
	return 0;
}

int
cnxk_rep_flow_ops_get(struct rte_eth_dev *ethdev, const struct rte_flow_ops **ops)
{
	PLT_SET_USED(ethdev);
	PLT_SET_USED(ops);
	return 0;
}

/* CNXK platform representor dev ops */
struct eth_dev_ops cnxk_rep_dev_ops = {
	.dev_infos_get = cnxk_rep_dev_info_get,
	.dev_configure = cnxk_rep_dev_configure,
	.dev_start = cnxk_rep_dev_start,
	.rx_queue_setup = cnxk_rep_rx_queue_setup,
	.rx_queue_release = cnxk_rep_rx_queue_release,
	.tx_queue_setup = cnxk_rep_tx_queue_setup,
	.tx_queue_release = cnxk_rep_tx_queue_release,
	.link_update = cnxk_rep_link_update,
	.dev_close = cnxk_rep_dev_close,
	.dev_stop = cnxk_rep_dev_stop,
	.stats_get = cnxk_rep_stats_get,
	.stats_reset = cnxk_rep_stats_reset,
	.flow_ops_get = cnxk_rep_flow_ops_get
};
