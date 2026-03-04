/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2026 Google LLC
 */

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include "base/gve_adminq.h"
#include "gve_ethdev.h"

static int
gve_validate_flow_attr(const struct rte_flow_attr *attr,
		       struct rte_flow_error *error)
{
	if (attr == NULL) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR, NULL,
				"Invalid flow attribute");
		return -EINVAL;
	}
	if (attr->egress || attr->transfer) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR, attr,
				"Only ingress is supported");
		return -EINVAL;
	}
	if (!attr->ingress) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_INGRESS, attr,
				"Ingress attribute must be set");
		return -EINVAL;
	}
	if (attr->priority != 0) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR_PRIORITY, attr,
				"Priority levels are not supported");
		return -EINVAL;
	}

	return 0;
}

static void
gve_parse_ipv4(const struct rte_flow_item *item,
	       struct gve_flow_rule_params *rule)
{
	if (item->spec) {
		const struct rte_flow_item_ipv4 *spec = item->spec;
		const struct rte_flow_item_ipv4 *mask =
			item->mask ? item->mask : &rte_flow_item_ipv4_mask;

		rule->key.src_ip[0] = spec->hdr.src_addr;
		rule->key.dst_ip[0] = spec->hdr.dst_addr;
		rule->mask.src_ip[0] = mask->hdr.src_addr;
		rule->mask.dst_ip[0] = mask->hdr.dst_addr;
	}
}

static void
gve_parse_ipv6(const struct rte_flow_item *item,
	       struct gve_flow_rule_params *rule)
{
	if (item->spec) {
		const struct rte_flow_item_ipv6 *spec = item->spec;
		const struct rte_flow_item_ipv6 *mask =
			item->mask ? item->mask : &rte_flow_item_ipv6_mask;
		const __be32 *src_ip = (const __be32 *)&spec->hdr.src_addr;
		const __be32 *src_mask = (const __be32 *)&mask->hdr.src_addr;
		const __be32 *dst_ip = (const __be32 *)&spec->hdr.dst_addr;
		const __be32 *dst_mask = (const __be32 *)&mask->hdr.dst_addr;
		int i;

		/*
		 * The device expects IPv6 addresses as an array of 4 32-bit words
		 * in reverse word order (the MSB word at index 3 and the LSB word
		 * at index 0). We must reverse the DPDK network byte order array.
		 */
		for (i = 0; i < 4; i++) {
			rule->key.src_ip[3 - i] = src_ip[i];
			rule->key.dst_ip[3 - i] = dst_ip[i];
			rule->mask.src_ip[3 - i] = src_mask[i];
			rule->mask.dst_ip[3 - i] = dst_mask[i];
		}
	}
}

static void
gve_parse_udp(const struct rte_flow_item *item,
	      struct gve_flow_rule_params *rule)
{
	if (item->spec) {
		const struct rte_flow_item_udp *spec = item->spec;
		const struct rte_flow_item_udp *mask =
			item->mask ? item->mask : &rte_flow_item_udp_mask;

		rule->key.src_port = spec->hdr.src_port;
		rule->key.dst_port = spec->hdr.dst_port;
		rule->mask.src_port = mask->hdr.src_port;
		rule->mask.dst_port = mask->hdr.dst_port;
	}
}

static void
gve_parse_tcp(const struct rte_flow_item *item,
	      struct gve_flow_rule_params *rule)
{
	if (item->spec) {
		const struct rte_flow_item_tcp *spec = item->spec;
		const struct rte_flow_item_tcp *mask =
			item->mask ? item->mask : &rte_flow_item_tcp_mask;

		rule->key.src_port = spec->hdr.src_port;
		rule->key.dst_port = spec->hdr.dst_port;
		rule->mask.src_port = mask->hdr.src_port;
		rule->mask.dst_port = mask->hdr.dst_port;
	}
}

static void
gve_parse_sctp(const struct rte_flow_item *item,
	       struct gve_flow_rule_params *rule)
{
	if (item->spec) {
		const struct rte_flow_item_sctp *spec = item->spec;
		const struct rte_flow_item_sctp *mask =
			item->mask ? item->mask : &rte_flow_item_sctp_mask;

		rule->key.src_port = spec->hdr.src_port;
		rule->key.dst_port = spec->hdr.dst_port;
		rule->mask.src_port = mask->hdr.src_port;
		rule->mask.dst_port = mask->hdr.dst_port;
	}
}

static void
gve_parse_esp(const struct rte_flow_item *item,
	      struct gve_flow_rule_params *rule)
{
	if (item->spec) {
		const struct rte_flow_item_esp *spec = item->spec;
		const struct rte_flow_item_esp *mask =
			item->mask ? item->mask : &rte_flow_item_esp_mask;

		rule->key.spi = spec->hdr.spi;
		rule->mask.spi = mask->hdr.spi;
	}
}

static void
gve_parse_ah(const struct rte_flow_item *item, struct gve_flow_rule_params *rule)
{
	if (item->spec) {
		const struct rte_flow_item_ah *spec = item->spec;
		const struct rte_flow_item_ah *mask =
			item->mask ? item->mask : &rte_flow_item_ah_mask;

		rule->key.spi = spec->spi;
		rule->mask.spi = mask->spi;
	}
}

static int
gve_validate_and_parse_flow_pattern(const struct rte_flow_item pattern[],
				    struct rte_flow_error *error,
				    struct gve_flow_rule_params *rule)
{
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type l3_type = RTE_FLOW_ITEM_TYPE_VOID;
	enum rte_flow_item_type l4_type = RTE_FLOW_ITEM_TYPE_VOID;

	if (pattern == NULL) {
		rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ITEM_NUM, NULL,
				"Invalid flow pattern");
		return -EINVAL;
	}

	for (; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			/* Last and range are not supported as match criteria. */
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "No support for range");
			return -EINVAL;
		}
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			if (l3_type != RTE_FLOW_ITEM_TYPE_VOID) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Multiple L3 items not supported");
				return -EINVAL;
			}
			gve_parse_ipv4(item, rule);
			l3_type = item->type;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			if (l3_type != RTE_FLOW_ITEM_TYPE_VOID) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Multiple L3 items not supported");
				return -EINVAL;
			}
			gve_parse_ipv6(item, rule);
			l3_type = item->type;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			if (l4_type != RTE_FLOW_ITEM_TYPE_VOID) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Multiple L4 items not supported");
				return -EINVAL;
			}
			gve_parse_udp(item, rule);
			l4_type = item->type;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			if (l4_type != RTE_FLOW_ITEM_TYPE_VOID) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Multiple L4 items not supported");
				return -EINVAL;
			}
			gve_parse_tcp(item, rule);
			l4_type = item->type;
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			if (l4_type != RTE_FLOW_ITEM_TYPE_VOID) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Multiple L4 items not supported");
				return -EINVAL;
			}
			gve_parse_sctp(item, rule);
			l4_type = item->type;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			if (l4_type != RTE_FLOW_ITEM_TYPE_VOID) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Multiple L4 items not supported");
				return -EINVAL;
			}
			gve_parse_esp(item, rule);
			l4_type = item->type;
			break;
		case RTE_FLOW_ITEM_TYPE_AH:
			if (l4_type != RTE_FLOW_ITEM_TYPE_VOID) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Multiple L4 items not supported");
				return -EINVAL;
			}
			gve_parse_ah(item, rule);
			l4_type = item->type;
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM, item,
				   "Unsupported flow pattern item type");
			return -EINVAL;
		}
	}

	switch (l3_type) {
	case RTE_FLOW_ITEM_TYPE_IPV4:
		switch (l4_type) {
		case RTE_FLOW_ITEM_TYPE_TCP:
			rule->flow_type = GVE_FLOW_TYPE_TCPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			rule->flow_type = GVE_FLOW_TYPE_UDPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			rule->flow_type = GVE_FLOW_TYPE_SCTPV4;
			break;
		case RTE_FLOW_ITEM_TYPE_AH:
			rule->flow_type = GVE_FLOW_TYPE_AHV4;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			rule->flow_type = GVE_FLOW_TYPE_ESPV4;
			break;
		default:
			goto unsupported_flow;
		}
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		switch (l4_type) {
		case RTE_FLOW_ITEM_TYPE_TCP:
			rule->flow_type = GVE_FLOW_TYPE_TCPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			rule->flow_type = GVE_FLOW_TYPE_UDPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			rule->flow_type = GVE_FLOW_TYPE_SCTPV6;
			break;
		case RTE_FLOW_ITEM_TYPE_AH:
			rule->flow_type = GVE_FLOW_TYPE_AHV6;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			rule->flow_type = GVE_FLOW_TYPE_ESPV6;
			break;
		default:
			goto unsupported_flow;
		}
		break;
	default:
		goto unsupported_flow;
	}

	return 0;

unsupported_flow:
	rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_ITEM,
			   NULL, "Unsupported L3/L4 combination");
	return -EINVAL;
}

static int
gve_validate_and_parse_flow_actions(struct rte_eth_dev *dev,
				    const struct rte_flow_action actions[],
				    struct rte_flow_error *error,
				    struct gve_flow_rule_params *rule)
{
	const struct rte_flow_action_queue *action_queue;
	const struct rte_flow_action *action = actions;
	int num_queue_actions = 0;

	if (actions == NULL) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM, NULL,
				   "Invalid flow actions");
		return -EINVAL;
	}

	while (action->type != RTE_FLOW_ACTION_TYPE_END) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			if (action->conf == NULL) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION_CONF,
						   action,
						   "QUEUE action config cannot be NULL.");
				return -EINVAL;
			}

			action_queue = action->conf;
			if (action_queue->index >= dev->data->nb_rx_queues) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION_CONF,
						   action, "Invalid Queue ID");
				return -EINVAL;
			}

			rule->action = action_queue->index;
			num_queue_actions++;
			break;
		default:
			rte_flow_error_set(error, ENOTSUP,
					   RTE_FLOW_ERROR_TYPE_ACTION,
					   action,
					   "Unsupported action. Only QUEUE is permitted.");
			return -ENOTSUP;
		}
		action++;
	}

	if (num_queue_actions == 0) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "A QUEUE action is required.");
		return -EINVAL;
	}

	if (num_queue_actions > 1) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION_NUM,
				   NULL, "Only a single QUEUE action is allowed.");
		return -EINVAL;
	}

	return 0;
}

static int
gve_validate_and_parse_flow(struct rte_eth_dev *dev,
			    const struct rte_flow_attr *attr,
			    const struct rte_flow_item pattern[],
			    const struct rte_flow_action actions[],
			    struct rte_flow_error *error,
			    struct gve_flow_rule_params *rule)
{
	int err;

	err = gve_validate_flow_attr(attr, error);
	if (err)
		return err;

	err = gve_validate_and_parse_flow_pattern(pattern, error, rule);
	if (err)
		return err;

	err = gve_validate_and_parse_flow_actions(dev, actions, error, rule);
	if (err)
		return err;

	return 0;
}

int
gve_flow_init_bmp(struct gve_priv *priv)
{
	priv->avail_flow_rule_bmp = rte_bitmap_init_with_all_set(priv->max_flow_rules,
			priv->avail_flow_rule_bmp_mem, priv->flow_rule_bmp_size);
	if (priv->avail_flow_rule_bmp == NULL) {
		PMD_DRV_LOG(ERR, "Flow subsystem failed: cannot init bitmap.");
		return -ENOMEM;
	}

	return 0;
}

void
gve_flow_free_bmp(struct gve_priv *priv)
{
	rte_free(priv->avail_flow_rule_bmp_mem);
	priv->avail_flow_rule_bmp_mem = NULL;
	priv->avail_flow_rule_bmp = NULL;
}

/*
 * The caller must acquire the flow rule lock before calling this function.
 */
int
gve_free_flow_rules(struct gve_priv *priv)
{
	struct gve_flow *flow;
	int err = 0;

	if (!TAILQ_EMPTY(&priv->active_flows)) {
		err = gve_adminq_reset_flow_rules(priv);
		if (err) {
			PMD_DRV_LOG(ERR,
				"Failed to reset flow rules, internal device err=%d",
				err);
		}

		/* Free flows even if AQ fails to avoid leaking memory. */
		while (!TAILQ_EMPTY(&priv->active_flows)) {
			flow = TAILQ_FIRST(&priv->active_flows);
			TAILQ_REMOVE(&priv->active_flows, flow, list_handle);
			free(flow);
		}
	}

	return err;
}

static struct rte_flow *
gve_create_flow_rule(struct rte_eth_dev *dev,
		     const struct rte_flow_attr *attr,
		     const struct rte_flow_item pattern[],
		     const struct rte_flow_action actions[],
		     struct rte_flow_error *error)
{
	struct gve_priv *priv = dev->data->dev_private;
	struct gve_flow_rule_params rule = {0};
	uint64_t slab_bits = 0;
	uint32_t slab_idx = 0;
	struct gve_flow *flow;
	int err;

	err = gve_validate_and_parse_flow(dev, attr, pattern, actions, error,
					  &rule);
	if (err)
		return NULL;

	flow = calloc(1, sizeof(struct gve_flow));
	if (flow == NULL) {
		rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to allocate memory for flow rule.");
		return NULL;
	}

	pthread_mutex_lock(&priv->flow_rule_lock);

	if (!gve_get_flow_subsystem_ok(priv)) {
		rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to create flow, flow subsystem not initialized.");
		goto free_flow_and_unlock;
	}

	/* Try to allocate a new rule ID from the bitmap. */
	if (rte_bitmap_scan(priv->avail_flow_rule_bmp, &slab_idx,
			&slab_bits) == 1) {
		flow->rule_id = slab_idx + rte_ctz64(slab_bits);
		rte_bitmap_clear(priv->avail_flow_rule_bmp, flow->rule_id);
	} else {
		rte_flow_error_set(error, ENOMEM,
				RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				"Failed to create flow, could not allocate a new rule ID.");
		goto free_flow_and_unlock;
	}

	err = gve_adminq_add_flow_rule(priv, &rule, flow->rule_id);
	if (err) {
		rte_bitmap_set(priv->avail_flow_rule_bmp, flow->rule_id);
		rte_flow_error_set(error, -err,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to create flow rule, internal device error.");
		goto free_flow_and_unlock;
	}

	TAILQ_INSERT_TAIL(&priv->active_flows, flow, list_handle);

	pthread_mutex_unlock(&priv->flow_rule_lock);

	return (struct rte_flow *)flow;

free_flow_and_unlock:
	free(flow);
	pthread_mutex_unlock(&priv->flow_rule_lock);
	return NULL;
}

static int
gve_destroy_flow_rule(struct rte_eth_dev *dev, struct rte_flow *flow_handle,
		      struct rte_flow_error *error)
{
	struct gve_priv *priv = dev->data->dev_private;
	struct gve_flow *flow;
	bool flow_rule_active;
	int err;

	pthread_mutex_lock(&priv->flow_rule_lock);

	if (!gve_get_flow_subsystem_ok(priv)) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to destroy flow, flow subsystem not initialized.");
		err = -ENOTSUP;
		goto unlock;
	}

	flow = (struct gve_flow *)flow_handle;

	if (flow == NULL) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to destroy flow, invalid flow provided.");
		err = -EINVAL;
		goto unlock;
	}

	if (flow->rule_id >= priv->max_flow_rules) {
		PMD_DRV_LOG(ERR,
			"Cannot destroy flow rule with invalid ID %d.",
			flow->rule_id);
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to destroy flow, rule ID is invalid.");
		err = -EINVAL;
		goto unlock;
	}

	flow_rule_active = !rte_bitmap_get(priv->avail_flow_rule_bmp,
					   flow->rule_id);

	if (!flow_rule_active) {
		rte_flow_error_set(error, EINVAL,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to destroy flow, handle not found in active list.");
		err = -EINVAL;
		goto unlock;
	}

	err = gve_adminq_del_flow_rule(priv, flow->rule_id);
	if (err) {
		rte_flow_error_set(error, -err,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to destroy flow, internal device error.");
		goto unlock;
	}

	rte_bitmap_set(priv->avail_flow_rule_bmp, flow->rule_id);
	TAILQ_REMOVE(&priv->active_flows, flow, list_handle);
	free(flow);

	err = 0;

unlock:
	pthread_mutex_unlock(&priv->flow_rule_lock);
	return err;
}

static int
gve_flush_flow_rules(struct rte_eth_dev *dev, struct rte_flow_error *error)
{
	struct gve_priv *priv = dev->data->dev_private;
	int err;

	pthread_mutex_lock(&priv->flow_rule_lock);

	if (!gve_get_flow_subsystem_ok(priv)) {
		rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to flush rules, flow subsystem not initialized.");
		err = -ENOTSUP;
		goto unlock;
	}

	err = gve_free_flow_rules(priv);
	if (err) {
		rte_flow_error_set(error, -err,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to flush rules due to internal device error, disabling flow subsystem.");
		goto disable_and_free;
	}

	err = gve_flow_init_bmp(priv);
	if (err) {
		rte_flow_error_set(error, -err,
			RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
			"Failed to re-initialize rule ID bitmap, disabling flow subsystem.");
		goto disable_and_free;
	}

	pthread_mutex_unlock(&priv->flow_rule_lock);

	return 0;

disable_and_free:
	gve_clear_flow_subsystem_ok(priv);
	gve_flow_free_bmp(priv);
unlock:
	pthread_mutex_unlock(&priv->flow_rule_lock);
	return err;
}

const struct rte_flow_ops gve_flow_ops = {
	.create = gve_create_flow_rule,
	.destroy = gve_destroy_flow_rule,
	.flush = gve_flush_flow_rules,
};
