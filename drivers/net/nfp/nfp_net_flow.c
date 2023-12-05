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
#include "nfp_net_cmsg.h"

/* Static initializer for a list of subsequent item types */
#define NEXT_ITEM(...) \
	((const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	})

/* Process structure associated with a flow item */
struct nfp_net_flow_item_proc {
	/* Bit-mask for fields supported by this PMD. */
	const void *mask_support;
	/* Bit-mask to use when @p item->mask is not provided. */
	const void *mask_default;
	/* Size in bytes for @p mask_support and @p mask_default. */
	const uint32_t mask_sz;
	/* Merge a pattern item into a flow rule handle. */
	int (*merge)(struct rte_flow *nfp_flow,
			const struct rte_flow_item *item,
			const struct nfp_net_flow_item_proc *proc);
	/* List of possible subsequent items. */
	const enum rte_flow_item_type *const next_item;
};

static int
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

static int
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

static struct rte_flow *
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

static int
nfp_net_flow_position_acquire(struct nfp_net_priv *priv,
		uint32_t priority,
		struct rte_flow *nfp_flow)
{
	uint32_t i;

	if (priority != 0) {
		i = NFP_NET_FLOW_LIMIT - priority - 1;

		if (priv->flow_position[i]) {
			PMD_DRV_LOG(ERR, "There is already a flow rule in this place.");
			return -EAGAIN;
		}

		priv->flow_position[i] = true;
		nfp_flow->position = priority;
		return 0;
	}

	for (i = 0; i < NFP_NET_FLOW_LIMIT; i++) {
		if (!priv->flow_position[i]) {
			priv->flow_position[i] = true;
			break;
		}
	}

	if (i == NFP_NET_FLOW_LIMIT) {
		PMD_DRV_LOG(ERR, "The limited flow number is reach.");
		return -ERANGE;
	}

	nfp_flow->position = NFP_NET_FLOW_LIMIT - i - 1;

	return 0;
}

static void
nfp_net_flow_position_free(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	priv->flow_position[nfp_flow->position] = false;
}

static struct rte_flow *
nfp_net_flow_alloc(struct nfp_net_priv *priv,
		uint32_t priority,
		uint32_t match_len,
		uint32_t action_len,
		uint32_t port_id)
{
	int ret;
	char *data;
	struct rte_flow *nfp_flow;
	struct nfp_net_flow_payload *payload;

	nfp_flow = rte_zmalloc("nfp_flow", sizeof(struct rte_flow), 0);
	if (nfp_flow == NULL)
		return NULL;

	data = rte_zmalloc("nfp_flow_payload", match_len + action_len, 0);
	if (data == NULL)
		goto free_flow;

	ret = nfp_net_flow_position_acquire(priv, priority, nfp_flow);
	if (ret != 0)
		goto free_payload;

	nfp_flow->port_id      = port_id;
	payload                = &nfp_flow->payload;
	payload->match_len     = match_len;
	payload->action_len    = action_len;
	payload->match_data    = data;
	payload->action_data   = data + match_len;

	return nfp_flow;

free_payload:
	rte_free(data);
free_flow:
	rte_free(nfp_flow);

	return NULL;
}

static void
nfp_net_flow_free(struct nfp_net_priv *priv,
		struct rte_flow *nfp_flow)
{
	nfp_net_flow_position_free(priv, nfp_flow);
	rte_free(nfp_flow->payload.match_data);
	rte_free(nfp_flow);
}

static int
nfp_net_flow_calculate_items(const struct rte_flow_item items[],
		uint32_t *match_len)
{
	const struct rte_flow_item *item;

	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; ++item) {
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_ETH detected");
			*match_len = sizeof(struct nfp_net_cmsg_match_eth);
			return 0;
		default:
			PMD_DRV_LOG(ERR, "Can't calculate match length");
			*match_len = 0;
			return -ENOTSUP;
		}
	}

	return -EINVAL;
}

static int
nfp_net_flow_merge_eth(__rte_unused struct rte_flow *nfp_flow,
		const struct rte_flow_item *item,
		__rte_unused const struct nfp_net_flow_item_proc *proc)
{
	struct nfp_net_cmsg_match_eth *eth;
	const struct rte_flow_item_eth *spec;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(ERR, "NFP flow merge eth: no item->spec!");
		return -EINVAL;
	}

	nfp_flow->payload.cmsg_type = NFP_NET_CFG_MBOX_CMD_FS_ADD_ETHTYPE;

	eth = (struct nfp_net_cmsg_match_eth *)nfp_flow->payload.match_data;
	eth->ether_type = rte_be_to_cpu_16(spec->type);

	return 0;
}

/* Graph of supported items and associated process function */
static const struct nfp_net_flow_item_proc nfp_net_flow_item_proc_list[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_ETH),
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.merge = nfp_net_flow_merge_eth,
	},
};

static int
nfp_net_flow_item_check(const struct rte_flow_item *item,
		const struct nfp_net_flow_item_proc *proc)
{
	uint32_t i;
	int ret = 0;
	const uint8_t *mask;

	/* item->last and item->mask cannot exist without item->spec. */
	if (item->spec == NULL) {
		if (item->mask || item->last) {
			PMD_DRV_LOG(ERR, "'mask' or 'last' field provided"
					" without a corresponding 'spec'.");
			return -EINVAL;
		}

		/* No spec, no mask, no problem. */
		return 0;
	}

	mask = (item->mask != NULL) ? item->mask : proc->mask_default;

	/*
	 * Single-pass check to make sure that:
	 * - Mask is supported, no bits are set outside proc->mask_support.
	 * - Both item->spec and item->last are included in mask.
	 */
	for (i = 0; i != proc->mask_sz; ++i) {
		if (mask[i] == 0)
			continue;

		if ((mask[i] | ((const uint8_t *)proc->mask_support)[i]) !=
				((const uint8_t *)proc->mask_support)[i]) {
			PMD_DRV_LOG(ERR, "Unsupported field found in 'mask'.");
			ret = -EINVAL;
			break;
		}

		if (item->last != NULL &&
				(((const uint8_t *)item->spec)[i] & mask[i]) !=
				(((const uint8_t *)item->last)[i] & mask[i])) {
			PMD_DRV_LOG(ERR, "Range between 'spec' and 'last'"
					" is larger than 'mask'.");
			ret = -ERANGE;
			break;
		}
	}

	return ret;
}

static int
nfp_net_flow_compile_items(const struct rte_flow_item items[],
		struct rte_flow *nfp_flow)
{
	uint32_t i;
	int ret = 0;
	const struct rte_flow_item *item;
	const struct nfp_net_flow_item_proc *proc_list;

	proc_list = nfp_net_flow_item_proc_list;

	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; ++item) {
		const struct nfp_net_flow_item_proc *proc = NULL;

		for (i = 0; (proc_list->next_item != NULL) &&
				(proc_list->next_item[i] != RTE_FLOW_ITEM_TYPE_END); ++i) {
			if (proc_list->next_item[i] == item->type) {
				proc = &nfp_net_flow_item_proc_list[item->type];
				break;
			}
		}

		if (proc == NULL) {
			PMD_DRV_LOG(ERR, "No next item provided for %d", item->type);
			ret = -ENOTSUP;
			break;
		}

		/* Perform basic sanity checks */
		ret = nfp_net_flow_item_check(item, proc);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "NFP flow item %d check failed", item->type);
			ret = -EINVAL;
			break;
		}

		if (proc->merge == NULL) {
			PMD_DRV_LOG(ERR, "NFP flow item %d no proc function", item->type);
			ret = -ENOTSUP;
			break;
		}

		ret = proc->merge(nfp_flow, item, proc);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "NFP flow item %d exact merge failed", item->type);
			break;
		}

		proc_list = proc;
	}

	return ret;
}

static void
nfp_net_flow_process_priority(__rte_unused struct rte_flow *nfp_flow,
		uint32_t match_len)
{
	switch (match_len) {
	default:
		break;
	}
}

static struct rte_flow *
nfp_net_flow_setup(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		__rte_unused const struct rte_flow_action actions[])
{
	int ret;
	char *hash_data;
	uint32_t port_id;
	uint32_t action_len;
	struct nfp_net_hw *hw;
	uint32_t match_len = 0;
	struct nfp_net_priv *priv;
	struct rte_flow *nfp_flow;
	struct rte_flow *flow_find;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	ret = nfp_net_flow_calculate_items(items, &match_len);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Key layers calculate failed.");
		return NULL;
	}

	action_len = sizeof(struct nfp_net_cmsg_action);
	port_id = ((struct nfp_net_hw *)dev->data->dev_private)->nfp_idx;

	nfp_flow = nfp_net_flow_alloc(priv, attr->priority, match_len, action_len, port_id);
	if (nfp_flow == NULL) {
		PMD_DRV_LOG(ERR, "Alloc nfp flow failed.");
		return NULL;
	}

	ret = nfp_net_flow_compile_items(items, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "NFP flow item process failed.");
		goto free_flow;
	}

	/* Calculate and store the hash_key for later use */
	hash_data = nfp_flow->payload.match_data;
	nfp_flow->hash_key = rte_jhash(hash_data, match_len + action_len,
			priv->hash_seed);

	/* Find the flow in hash table */
	flow_find = nfp_net_flow_table_search(priv, nfp_flow);
	if (flow_find != NULL) {
		PMD_DRV_LOG(ERR, "This flow is already exist.");
		goto free_flow;
	}

	priv->flow_count++;

	nfp_net_flow_process_priority(nfp_flow, match_len);

	return nfp_flow;

free_flow:
	nfp_net_flow_free(priv, nfp_flow);

	return NULL;
}

static int
nfp_net_flow_teardown(struct nfp_net_priv *priv,
		__rte_unused struct rte_flow *nfp_flow)
{
	priv->flow_count--;

	return 0;
}

static int
nfp_net_flow_offload(struct nfp_net_hw *hw,
		struct rte_flow *flow,
		bool delete_flag)
{
	int ret;
	char *tmp;
	uint32_t msg_size;
	struct nfp_net_cmsg *cmsg;

	msg_size = sizeof(uint32_t) + flow->payload.match_len +
			flow->payload.action_len;
	cmsg = nfp_net_cmsg_alloc(msg_size);
	if (cmsg == NULL) {
		PMD_DRV_LOG(ERR, "Alloc cmsg failed.");
		return -ENOMEM;
	}

	cmsg->cmd = flow->payload.cmsg_type;
	if (delete_flag)
		cmsg->cmd++;

	tmp = (char *)cmsg->data;
	rte_memcpy(tmp, flow->payload.match_data, flow->payload.match_len);
	tmp += flow->payload.match_len;
	rte_memcpy(tmp, flow->payload.action_data, flow->payload.action_len);

	ret = nfp_net_cmsg_xmit(hw, cmsg, msg_size);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Send cmsg failed.");
		ret = -EINVAL;
		goto free_cmsg;
	}

free_cmsg:
	nfp_net_cmsg_free(cmsg);

	return ret;
}

static int
nfp_net_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	int ret;
	struct nfp_net_hw *hw;
	struct rte_flow *nfp_flow;
	struct nfp_net_priv *priv;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	nfp_flow = nfp_net_flow_setup(dev, attr, items, actions);
	if (nfp_flow == NULL) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "This flow can not be offloaded.");
	}

	ret = nfp_net_flow_teardown(priv, nfp_flow);
	if (ret != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow resource free failed.");
	}

	nfp_net_flow_free(priv, nfp_flow);

	return 0;
}

static struct rte_flow *
nfp_net_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	int ret;
	struct nfp_net_hw *hw;
	struct rte_flow *nfp_flow;
	struct nfp_net_priv *priv;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	nfp_flow = nfp_net_flow_setup(dev, attr, items, actions);
	if (nfp_flow == NULL) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "This flow can not be offloaded.");
		return NULL;
	}

	/* Add the flow to flow hash table */
	ret = nfp_net_flow_table_add(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Add flow to the flow table failed.");
		goto flow_teardown;
	}

	/* Add the flow to hardware */
	ret = nfp_net_flow_offload(hw, nfp_flow, false);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Add flow to firmware failed.");
		goto table_delete;
	}

	return nfp_flow;

table_delete:
	nfp_net_flow_table_delete(priv, nfp_flow);
flow_teardown:
	nfp_net_flow_teardown(priv, nfp_flow);
	nfp_net_flow_free(priv, nfp_flow);

	return NULL;
}

static int
nfp_net_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *nfp_flow,
		struct rte_flow_error *error)
{
	int ret;
	struct nfp_net_hw *hw;
	struct nfp_net_priv *priv;
	struct rte_flow *flow_find;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw->pf_dev->app_fw_priv);
	priv = app_fw_nic->ports[hw->idx]->priv;

	/* Find the flow in flow hash table */
	flow_find = nfp_net_flow_table_search(priv, nfp_flow);
	if (flow_find == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow does not exist.");
		ret = -EINVAL;
		goto exit;
	}

	/* Delete the flow from hardware */
	ret = nfp_net_flow_offload(hw, nfp_flow, true);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Delete flow from firmware failed.");
		ret = -EINVAL;
		goto exit;
	}

	/* Delete the flow from flow hash table */
	ret = nfp_net_flow_table_delete(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Delete flow from the flow table failed.");
		ret = -EINVAL;
		goto exit;
	}

	ret = nfp_net_flow_teardown(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow teardown failed.");
		ret = -EINVAL;
		goto exit;
	}

exit:
	nfp_net_flow_free(priv, nfp_flow);

	return ret;
}

static int
nfp_net_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	int ret = 0;
	void *next_data;
	uint32_t iter = 0;
	const void *next_key;
	struct nfp_net_hw *hw;
	struct rte_flow *nfp_flow;
	struct rte_hash *flow_table;
	struct nfp_app_fw_nic *app_fw_nic;

	hw = dev->data->dev_private;
	app_fw_nic = NFP_PRIV_TO_APP_FW_NIC(hw->pf_dev->app_fw_priv);
	flow_table = app_fw_nic->ports[hw->idx]->priv->flow_table;

	while (rte_hash_iterate(flow_table, &next_key, &next_data, &iter) >= 0) {
		nfp_flow = next_data;
		ret = nfp_net_flow_destroy(dev, nfp_flow, error);
		if (ret != 0)
			break;
	}

	return ret;
}

static const struct rte_flow_ops nfp_net_flow_ops = {
	.validate                = nfp_net_flow_validate,
	.create                  = nfp_net_flow_create,
	.destroy                 = nfp_net_flow_destroy,
	.flush                   = nfp_net_flow_flush,
};

int
nfp_net_flow_ops_get(struct rte_eth_dev *dev,
		const struct rte_flow_ops **ops)
{
	struct nfp_net_hw *hw;

	if ((dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) != 0) {
		*ops = NULL;
		PMD_DRV_LOG(ERR, "Port is a representor.");
		return -EINVAL;
	}

	hw = dev->data->dev_private;
	if ((hw->super.ctrl_ext & NFP_NET_CFG_CTRL_FLOW_STEER) == 0) {
		*ops = NULL;
		return 0;
	}

	*ops = &nfp_net_flow_ops;

	return 0;
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
