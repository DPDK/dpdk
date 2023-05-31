/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <stdlib.h>

#include <rte_ethdev.h>
#include <rte_graph.h>

#include "rte_node_eth_api.h"

#include "ethdev_rx_priv.h"
#include "ethdev_tx_priv.h"
#include "ip4_rewrite_priv.h"
#include "ip6_rewrite_priv.h"
#include "node_private.h"

static struct ethdev_ctrl {
	uint16_t nb_graphs;
} ctrl;

int
rte_node_eth_config(struct rte_node_ethdev_config *conf, uint16_t nb_confs,
		    uint16_t nb_graphs)
{
	struct rte_node_register *ip4_rewrite_node;
	struct rte_node_register *ip6_rewrite_node;
	struct ethdev_tx_node_main *tx_node_data;
	uint16_t tx_q_used, rx_q_used, port_id;
	struct rte_node_register *tx_node;
	char name[RTE_NODE_NAMESIZE];
	const char *next_nodes = name;
	struct rte_mempool *mp;
	int i, j, rc;
	uint32_t id;

	ip4_rewrite_node = ip4_rewrite_node_get();
	ip6_rewrite_node = ip6_rewrite_node_get();
	tx_node_data = ethdev_tx_node_data_get();
	tx_node = ethdev_tx_node_get();
	for (i = 0; i < nb_confs; i++) {
		port_id = conf[i].port_id;

		if (!rte_eth_dev_is_valid_port(port_id))
			return -EINVAL;

		/* Check for mbuf minimum private size requirement */
		for (j = 0; j < conf[i].mp_count; j++) {
			mp = conf[i].mp[j];
			if (!mp)
				continue;
			/* Check for minimum private space */
			if (rte_pktmbuf_priv_size(mp) < NODE_MBUF_PRIV2_SIZE) {
				node_err("ethdev",
					 "Minimum mbuf priv size requirement not met by mp %s",
					 mp->name);
				return -EINVAL;
			}
		}

		rx_q_used = conf[i].num_rx_queues;
		tx_q_used = conf[i].num_tx_queues;
		/* Check if we have a txq for each worker */
		if (tx_q_used < nb_graphs)
			return -EINVAL;

		/* Create node for each rx port queue pair */
		for (j = 0; j < rx_q_used; j++) {
			struct ethdev_rx_node_main *rx_node_data;
			struct rte_node_register *rx_node;
			ethdev_rx_node_elem_t *elem;

			rx_node_data = ethdev_rx_get_node_data_get();
			rx_node = ethdev_rx_node_get();
			snprintf(name, sizeof(name), "%u-%u", port_id, j);
			/* Clone a new rx node with same edges as parent */
			id = rte_node_clone(rx_node->id, name);
			if (id == RTE_NODE_ID_INVALID)
				return -EIO;

			/* Add it to list of ethdev rx nodes for lookup */
			elem = malloc(sizeof(ethdev_rx_node_elem_t));
			if (elem == NULL)
				return -ENOMEM;
			memset(elem, 0, sizeof(ethdev_rx_node_elem_t));
			elem->ctx.port_id = port_id;
			elem->ctx.queue_id = j;
			elem->ctx.cls_next = ETHDEV_RX_NEXT_PKT_CLS;
			elem->nid = id;
			elem->next = rx_node_data->head;
			rx_node_data->head = elem;

			node_dbg("ethdev", "Rx node %s-%s: is at %u",
				 rx_node->name, name, id);
		}

		/* Create a per port tx node from base node */
		snprintf(name, sizeof(name), "%u", port_id);
		/* Clone a new node with same edges as parent */
		id = rte_node_clone(tx_node->id, name);
		tx_node_data->nodes[port_id] = id;

		node_dbg("ethdev", "Tx node %s-%s: is at %u", tx_node->name,
			 name, id);

		/* Prepare the actual name of the cloned node */
		snprintf(name, sizeof(name), "ethdev_tx-%u", port_id);

		/* Add this tx port node as next to ip4_rewrite_node */
		rte_node_edge_update(ip4_rewrite_node->id, RTE_EDGE_ID_INVALID,
				     &next_nodes, 1);
		/* Assuming edge id is the last one alloc'ed */
		rc = ip4_rewrite_set_next(
			port_id, rte_node_edge_count(ip4_rewrite_node->id) - 1);
		if (rc < 0)
			return rc;

		/* Add this tx port node as next to ip6_rewrite_node */
		rte_node_edge_update(ip6_rewrite_node->id, RTE_EDGE_ID_INVALID,
				     &next_nodes, 1);
		/* Assuming edge id is the last one alloc'ed */
		rc = ip6_rewrite_set_next(
			port_id, rte_node_edge_count(ip6_rewrite_node->id) - 1);
		if (rc < 0)
			return rc;

	}

	ctrl.nb_graphs = nb_graphs;
	return 0;
}
