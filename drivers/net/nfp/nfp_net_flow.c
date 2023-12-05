/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_net_flow.h"

#include <rte_flow_driver.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_malloc.h>

#include "nfp_logs.h"

__rte_unused static int
nfp_net_flow_table_add(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	int ret;

	ret = rte_hash_add_key_data(priv->flow_table, &nfp_flow->hash_key, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add to flow table failed.");
		return ret;
	}

	return 0;
}

__rte_unused static int
nfp_net_flow_table_delete(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	int ret;

	ret = rte_hash_del_key(priv->flow_table, &nfp_flow->hash_key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Delete from flow table failed.");
		return ret;
	}

	return 0;
}

__rte_unused static struct rte_flow *
nfp_net_flow_table_search(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	int index;
	struct rte_flow *flow_find;

	index = rte_hash_lookup_data(priv->flow_table, &nfp_flow->hash_key,
			(void **)&flow_find);
	if (index < 0) {
		PMD_DRV_LOG(DEBUG, "Data NOT found in the flow table.");
		return NULL;
	}

	return flow_find;
}

__rte_unused static struct rte_flow *
nfp_net_flow_alloc(uint32_t match_len,
		uint32_t action_len,
		uint32_t port_id)
{
	char *data;
	struct rte_flow *nfp_flow;
	struct nfp_net_flow_payload *payload;

	nfp_flow = rte_zmalloc("nfp_flow", sizeof(struct rte_flow), 0);
	if (nfp_flow == NULL)
		return NULL;

	data = rte_zmalloc("nfp_flow_payload", match_len + action_len, 0);
	if (data == NULL)
		goto free_flow;

	nfp_flow->port_id      = port_id;
	payload                = &nfp_flow->payload;
	payload->match_len     = match_len;
	payload->action_len    = action_len;
	payload->match_data    = data;
	payload->action_data   = data + match_len;

	return nfp_flow;

free_flow:
	rte_free(nfp_flow);

	return NULL;
}

__rte_unused static void
nfp_net_flow_free(struct rte_flow *nfp_flow)
{
	rte_free(nfp_flow->payload.match_data);
	rte_free(nfp_flow);
}

int
nfp_net_flow_priv_init(struct nfp_pf_dev *pf_dev,
		uint16_t port)
{
	int ret = 0;
	struct nfp_net_priv *priv;
	char flow_name[RTE_HASH_NAMESIZE];
	struct nfp_app_fw_nic *app_fw_nic;
	const char *pci_name = strchr(pf_dev->pci_dev->name, ':') + 1;

	snprintf(flow_name, sizeof(flow_name), "%s_fl_%u", pci_name, port);

	struct rte_hash_parameters flow_hash_params = {
		.name       = flow_name,
		.entries    = NFP_NET_FLOW_LIMIT,
		.hash_func  = rte_jhash,
		.socket_id  = rte_socket_id(),
		.key_len    = sizeof(uint32_t),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY,
	};

	priv = rte_zmalloc("nfp_app_nic_priv", sizeof(struct nfp_net_priv), 0);
	if (priv == NULL) {
		PMD_INIT_LOG(ERR, "NFP app nic priv creation failed");
		ret = -ENOMEM;
		goto exit;
	}

	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);
	app_fw_nic->ports[port]->priv = priv;
	priv->hash_seed = (uint32_t)rte_rand();

	/* Flow table */
	flow_hash_params.hash_func_init_val = priv->hash_seed;
	priv->flow_table = rte_hash_create(&flow_hash_params);
	if (priv->flow_table == NULL) {
		PMD_INIT_LOG(ERR, "flow hash table creation failed");
		ret = -ENOMEM;
		goto free_priv;
	}

	return 0;

free_priv:
	rte_free(priv);
exit:
	return ret;
}

void
nfp_net_flow_priv_uninit(struct nfp_pf_dev *pf_dev,
		uint16_t port)
{
	struct nfp_net_priv *priv;
	struct nfp_app_fw_nic *app_fw_nic;

	if (pf_dev == NULL)
		return;

	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[port]->priv;
	if (priv != NULL)
		rte_hash_free(priv->flow_table);

	rte_free(priv);
}
