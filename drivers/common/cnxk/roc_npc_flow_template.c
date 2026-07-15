/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

/* Size of an action's conf for the supported set; -1 = nested/unsupported. */
static int
roc_npc_action_conf_size(enum roc_npc_action_type type)
{
	switch (type) {
	case ROC_NPC_ACTION_TYPE_END:
	case ROC_NPC_ACTION_TYPE_VOID:
	case ROC_NPC_ACTION_TYPE_FLAG:
	case ROC_NPC_ACTION_TYPE_DROP:
	case ROC_NPC_ACTION_TYPE_COUNT:
	case ROC_NPC_ACTION_TYPE_PF:
	case ROC_NPC_ACTION_TYPE_VLAN_STRIP:
		return 0; /* no conf */
	case ROC_NPC_ACTION_TYPE_MARK:
		return sizeof(struct roc_npc_action_mark);
	case ROC_NPC_ACTION_TYPE_VF:
		return sizeof(struct roc_npc_action_vf);
	case ROC_NPC_ACTION_TYPE_PORT_ID:
		return sizeof(struct roc_npc_action_port_id);
	case ROC_NPC_ACTION_TYPE_QUEUE:
		return sizeof(struct roc_npc_action_queue);
	case ROC_NPC_ACTION_TYPE_VLAN_INSERT:
		return sizeof(struct roc_npc_action_of_set_vlan_vid);
	case ROC_NPC_ACTION_TYPE_VLAN_ETHTYPE_INSERT:
		return sizeof(struct roc_npc_action_of_push_vlan);
	case ROC_NPC_ACTION_TYPE_VLAN_PCP_INSERT:
		return sizeof(struct roc_npc_action_of_set_vlan_pcp);
	case ROC_NPC_ACTION_TYPE_AGE:
		return sizeof(struct roc_npc_action_age);
	default:
		return -1; /* RSS handled separately; others unsupported */
	}
}

/* Deep-copy an item spec/last/mask payload; *dst = NULL when nothing to copy. */
static int
roc_npc_dup_item_field(const void *src, uint32_t sz, const void **dst)
{
	void *copy;

	*dst = NULL;
	if (src == NULL || sz == 0)
		return 0;
	copy = plt_zmalloc(sz, 0);
	if (copy == NULL)
		return -ENOMEM;
	memcpy(copy, src, sz);
	*dst = copy;
	return 0;
}

/* Deep-copy an action conf; RSS packs queue[]/key[] into one blob. */
static void *
roc_npc_dup_action_conf(const struct roc_npc_action *act)
{
	if (act->conf == NULL)
		return NULL;

	if (act->type == ROC_NPC_ACTION_TYPE_RSS) {
		const struct roc_npc_action_rss *rss = act->conf;
		size_t rss_queue_bytes = (size_t)rss->queue_num * sizeof(uint16_t);
		size_t rss_key_bytes = rss->key_len;
		size_t rss_blob_bytes = sizeof(*rss);
		struct roc_npc_action_rss *rss_copy;
		uint8_t *rss_blob;

		if ((rss_queue_bytes && rss->queue == NULL) || (rss_key_bytes && rss->key == NULL))
			return NULL;

		rss_blob = plt_zmalloc(rss_blob_bytes + rss_queue_bytes + rss_key_bytes, 0);
		if (rss_blob == NULL)
			return NULL;
		rss_copy = (struct roc_npc_action_rss *)rss_blob;
		*rss_copy = *rss;
		if (rss_queue_bytes) {
			memcpy(rss_blob + rss_blob_bytes, rss->queue, rss_queue_bytes);
			rss_copy->queue = (const uint16_t *)(rss_blob + rss_blob_bytes);
		} else {
			rss_copy->queue = NULL;
		}
		if (rss_key_bytes) {
			memcpy(rss_blob + rss_blob_bytes + rss_queue_bytes, rss->key,
			       rss_key_bytes);
			rss_copy->key =
				(const uint8_t *)(rss_blob + rss_blob_bytes + rss_queue_bytes);
		} else {
			rss_copy->key = NULL;
		}
		return rss_copy;
	}

	{
		int conf_size = roc_npc_action_conf_size(act->type);
		void *copy;

		if (conf_size <= 0)
			return NULL;
		copy = plt_zmalloc(conf_size, 0);
		if (copy != NULL)
			memcpy(copy, act->conf, conf_size);
		return copy;
	}
}

/* Free the deep-copied items/actions arrays retained for a rule. */
static void
roc_npc_template_flow_free_rule(struct roc_npc_template_flow *flow)
{
	uint16_t k;

	if (flow->items != NULL) {
		for (k = 0; k < flow->nb_items; k++) {
			plt_free((void *)(uintptr_t)flow->items[k].spec);
			plt_free((void *)(uintptr_t)flow->items[k].last);
			plt_free((void *)(uintptr_t)flow->items[k].mask);
		}

		plt_free(flow->items);
		flow->items = NULL;
	}
	if (flow->actions != NULL) {
		for (k = 0; k < flow->nb_actions; k++)
			plt_free((void *)(uintptr_t)flow->actions[k].conf);

		plt_free(flow->actions);
		flow->actions = NULL;
	}
}

int
roc_npc_flow_configure(struct roc_npc *roc_npc, uint16_t nb_queues,
		       const struct roc_npc_flow_queue_attr *queue_attr[])
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_flow_queue *queues;
	int rc = -ENOMEM;
	uint16_t i;

	if (nb_queues == 0)
		return 0;

	if (queue_attr == NULL)
		return -EINVAL;

	if (npc->flow_queues != NULL) {
		/* Don't tear down live queues while async ops are in flight. */
		for (i = 0; i < npc->nb_flow_queues; i++) {
			if (npc->flow_queues[i].head != npc->flow_queues[i].tail)
				return -EBUSY;
		}
	}

	if (npc->flow_queues != NULL) {
		for (i = 0; i < npc->nb_flow_queues; i++)
			plt_free(npc->flow_queues[i].ops);

		plt_free(npc->flow_queues);
		npc->flow_queues = NULL;
		npc->nb_flow_queues = 0;
	}

	queues = plt_zmalloc(nb_queues * sizeof(struct roc_npc_flow_queue), 0);
	if (queues == NULL)
		return -ENOMEM;

	for (i = 0; i < nb_queues; i++) {
		if (queue_attr[i] == NULL || queue_attr[i]->size == 0) {
			rc = -EINVAL;
			goto err_free;
		}
		queues[i].size = plt_align32pow2(queue_attr[i]->size);
		queues[i].ops = plt_zmalloc(queues[i].size * sizeof(struct roc_npc_async_op), 0);
		if (queues[i].ops == NULL) {
			rc = -ENOMEM;
			goto err_free;
		}
		queues[i].head = 0;
		queues[i].pushed = 0;
		queues[i].tail = 0;
	}

	npc->flow_queues = queues;
	npc->nb_flow_queues = nb_queues;
	return 0;

err_free:
	for (i = 0; i < nb_queues; i++)
		plt_free(queues[i].ops);

	plt_free(queues);
	return rc;
}

struct roc_npc_pattern_template *
roc_npc_pattern_template_create(struct roc_npc *roc_npc,
				const struct roc_npc_pattern_template_attr *attr,
				const struct roc_npc_item_info pattern[], int *errcode)
{
	struct roc_npc_pattern_template *tmpl = NULL;
	int i, nb_items = 0, rc = -ENOMEM;
	uint16_t j;

	PLT_SET_USED(roc_npc);

	if (attr == NULL || pattern == NULL) {
		*errcode = -EINVAL;
		return NULL;
	}

	while (pattern[nb_items].type != ROC_NPC_ITEM_TYPE_END)
		nb_items++;

	/* Bound template size to the fixed-size rule arrays. */
	if (nb_items > ROC_NPC_ITEM_TYPE_END) {
		*errcode = -ENOTSUP;
		return NULL;
	}

	tmpl = plt_zmalloc(sizeof(*tmpl), 0);
	if (tmpl == NULL)
		goto err;

	tmpl->attr = *attr;
	tmpl->nb_items = nb_items;

	tmpl->pattern = plt_zmalloc((nb_items + 1) * sizeof(struct roc_npc_item_info), 0);
	if (tmpl->pattern == NULL)
		goto err;

	/* Reserve up to 3 payload copies per item. */
	tmpl->copies = plt_zmalloc(3 * (nb_items + 1) * sizeof(void *), 0);
	if (tmpl->copies == NULL)
		goto err;

	for (i = 0; i < nb_items; i++) {
		enum roc_npc_item_type t = pattern[i].type;
		uint32_t sz = pattern[i].size;

		/* RAW is not flat-copyable. */
		if (t == ROC_NPC_ITEM_TYPE_RAW) {
			rc = -ENOTSUP;
			goto err;
		}

		tmpl->pattern[i].type = t;
		tmpl->pattern[i].size = sz;
		/* Track each copy for the error path. */
		if (roc_npc_dup_item_field(pattern[i].spec, sz, &tmpl->pattern[i].spec))
			goto err;
		if (tmpl->pattern[i].spec != NULL)
			tmpl->copies[tmpl->nb_copies++] = (void *)(uintptr_t)tmpl->pattern[i].spec;
		if (roc_npc_dup_item_field(pattern[i].last, sz, &tmpl->pattern[i].last))
			goto err;
		if (tmpl->pattern[i].last != NULL)
			tmpl->copies[tmpl->nb_copies++] = (void *)(uintptr_t)tmpl->pattern[i].last;
		if (roc_npc_dup_item_field(pattern[i].mask, sz, &tmpl->pattern[i].mask))
			goto err;
		if (tmpl->pattern[i].mask != NULL)
			tmpl->copies[tmpl->nb_copies++] = (void *)(uintptr_t)tmpl->pattern[i].mask;
	}
	tmpl->pattern[nb_items].type = ROC_NPC_ITEM_TYPE_END;
	tmpl->refcnt = 0;

	return tmpl;

err:
	if (tmpl != NULL) {
		for (j = 0; j < tmpl->nb_copies; j++)
			plt_free(tmpl->copies[j]);
		plt_free(tmpl->copies);
		plt_free(tmpl->pattern);
		plt_free(tmpl);
	}
	*errcode = rc;
	return NULL;
}

int
roc_npc_pattern_template_destroy(struct roc_npc *roc_npc, struct roc_npc_pattern_template *tmpl)
{
	uint16_t i;

	PLT_SET_USED(roc_npc);

	if (tmpl == NULL)
		return 0;

	if (tmpl->refcnt != 0)
		return -EBUSY;

	for (i = 0; i < tmpl->nb_copies; i++)
		plt_free(tmpl->copies[i]);

	plt_free(tmpl->copies);
	plt_free(tmpl->pattern);
	plt_free(tmpl);
	return 0;
}

struct roc_npc_actions_template *
roc_npc_actions_template_create(struct roc_npc *roc_npc,
				const struct roc_npc_actions_template_attr *attr,
				const struct roc_npc_action actions[],
				const struct roc_npc_action masks[], int *errcode)
{
	struct roc_npc_actions_template *tmpl = NULL;
	int i, nb_actions = 0, rc = -ENOMEM;
	uint16_t j;

	PLT_SET_USED(roc_npc);

	if (attr == NULL || actions == NULL) {
		*errcode = -EINVAL;
		return NULL;
	}

	while (actions[nb_actions].type != ROC_NPC_ACTION_TYPE_END)
		nb_actions++;

	/* Bound template size to the fixed-size rule arrays. */
	if (nb_actions > ROC_NPC_MAX_ACTION_COUNT - 1) {
		*errcode = -ENOTSUP;
		return NULL;
	}

	tmpl = plt_zmalloc(sizeof(*tmpl), 0);
	if (tmpl == NULL)
		goto err;

	tmpl->attr = *attr;
	tmpl->nb_actions = nb_actions;

	tmpl->actions = plt_zmalloc((nb_actions + 1) * sizeof(struct roc_npc_action), 0);
	if (tmpl->actions == NULL)
		goto err;

	tmpl->masks = plt_zmalloc((nb_actions + 1) * sizeof(struct roc_npc_action), 0);
	if (tmpl->masks == NULL)
		goto err;

	/* Reserve up to 2 conf copies per action. */
	tmpl->copies = plt_zmalloc(2 * (nb_actions + 1) * sizeof(void *), 0);
	if (tmpl->copies == NULL)
		goto err;

	for (i = 0; i < nb_actions; i++) {
		enum roc_npc_action_type t = actions[i].type;
		int sz = roc_npc_action_conf_size(t);

		if (sz < 0 && t != ROC_NPC_ACTION_TYPE_RSS) {
			rc = -ENOTSUP;
			goto err;
		}

		tmpl->actions[i].type = t;
		tmpl->actions[i].rss_repte_pf_func = actions[i].rss_repte_pf_func;

		/* A malformed RSS conf is invalid input, not an allocation failure. */
		if (t == ROC_NPC_ACTION_TYPE_RSS && actions[i].conf != NULL) {
			const struct roc_npc_action_rss *rss = actions[i].conf;

			if ((rss->queue_num && rss->queue == NULL) ||
			    (rss->key_len && rss->key == NULL)) {
				rc = -EINVAL;
				goto err;
			}
		}

		tmpl->actions[i].conf = roc_npc_dup_action_conf(&actions[i]);
		if (tmpl->actions[i].conf == NULL && actions[i].conf != NULL &&
		    (sz > 0 || t == ROC_NPC_ACTION_TYPE_RSS))
			goto err;
		if (tmpl->actions[i].conf != NULL)
			tmpl->copies[tmpl->nb_copies++] = (void *)(uintptr_t)tmpl->actions[i].conf;

		if (masks != NULL) {
			if (masks[i].type != t) {
				rc = -EINVAL;
				goto err;
			}
			tmpl->masks[i].type = t;
			/* RSS masks cannot carry a nested pointer. */
			if (t == ROC_NPC_ACTION_TYPE_RSS) {
				if (masks[i].conf != NULL) {
					rc = -ENOTSUP;
					goto err;
				}
				tmpl->masks[i].conf = NULL;
			} else {
				tmpl->masks[i].conf = roc_npc_dup_action_conf(&masks[i]);
				if (tmpl->masks[i].conf == NULL && masks[i].conf != NULL && sz > 0)
					goto err;
				if (tmpl->masks[i].conf != NULL)
					tmpl->copies[tmpl->nb_copies++] =
						(void *)(uintptr_t)tmpl->masks[i].conf;
			}
		}
	}

	tmpl->actions[nb_actions].type = ROC_NPC_ACTION_TYPE_END;
	tmpl->masks[nb_actions].type = ROC_NPC_ACTION_TYPE_END;
	tmpl->refcnt = 0;

	return tmpl;

err:
	if (tmpl != NULL) {
		for (j = 0; j < tmpl->nb_copies; j++)
			plt_free(tmpl->copies[j]);
		plt_free(tmpl->copies);
		plt_free(tmpl->masks);
		plt_free(tmpl->actions);
		plt_free(tmpl);
	}
	*errcode = rc;
	return NULL;
}

int
roc_npc_actions_template_destroy(struct roc_npc *roc_npc, struct roc_npc_actions_template *tmpl)
{
	uint16_t i;

	PLT_SET_USED(roc_npc);

	if (tmpl == NULL)
		return 0;

	if (tmpl->refcnt != 0)
		return -EBUSY;

	for (i = 0; i < tmpl->nb_copies; i++)
		plt_free(tmpl->copies[i]);

	plt_free(tmpl->copies);
	plt_free(tmpl->masks);
	plt_free(tmpl->actions);
	plt_free(tmpl);
	return 0;
}

struct roc_npc_template_table *
roc_npc_template_table_create(struct roc_npc *roc_npc,
			      const struct roc_npc_template_table_attr *attr,
			      struct roc_npc_pattern_template *pattern_templates[],
			      uint8_t nb_pattern_templates,
			      struct roc_npc_actions_template *actions_templates[],
			      uint8_t nb_actions_templates, int *errcode)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_template_table *table;
	uint32_t nb_flows;
	uint8_t i;

	if (attr == NULL) {
		*errcode = -EINVAL;
		return NULL;
	}

	nb_flows = attr->nb_flows;

	table = plt_zmalloc(sizeof(*table), 0);
	if (table == NULL)
		goto enomem;

	table->insertion_type = attr->insertion_type;
	table->nb_flows = nb_flows;
	table->nb_live = 0;
	table->cookie = attr->cookie;
	plt_spinlock_init(&table->lock);

	table->flow_attr = attr->flow_attr;
	table->transfer = attr->transfer;

	if (attr->insertion_type == ROC_NPC_TEMPLATE_INSERTION_INDEX_WITH_PATTERN) {
		int resp_count = 0;
		uint32_t j;
		int rc;

		table->mcam_ids = plt_zmalloc(nb_flows * sizeof(int), 0);
		if (table->mcam_ids == NULL)
			goto free_index;
		for (j = 0; j < nb_flows; j++)
			table->mcam_ids[j] = -1;

		table->flow_pool = plt_zmalloc(nb_flows * sizeof(struct roc_npc_template_flow), 0);
		if (table->flow_pool == NULL)
			goto free_index;

		rc = roc_npc_mcam_alloc_entries(roc_npc, 0, table->mcam_ids, nb_flows,
						NPC_MCAM_ANY_PRIO, &resp_count, false);
		if (rc != 0 || resp_count < (int)nb_flows) {
			for (j = 0; j < (uint32_t)resp_count; j++) {
				if (table->mcam_ids[j] != -1)
					roc_npc_mcam_free_entry(roc_npc, table->mcam_ids[j]);
			}

			*errcode = -ENOSPC;
			goto free_index;
		}

		for (j = 0; j < nb_flows; j++) {
			table->flow_pool[j].table = table;
			table->flow_pool[j].nb_items = 0;
			table->flow_pool[j].nb_actions = 0;
		}
	}
	/* PATTERN: nothing reserved; each push allocates at the right priority. */

	if (nb_pattern_templates != 0) {
		table->pattern_templates =
			plt_zmalloc(nb_pattern_templates * sizeof(*table->pattern_templates), 0);
		if (table->pattern_templates == NULL)
			goto free_table;
	}
	if (nb_actions_templates != 0) {
		table->actions_templates =
			plt_zmalloc(nb_actions_templates * sizeof(*table->actions_templates), 0);
		if (table->actions_templates == NULL)
			goto free_table;
	}

	for (i = 0; i < nb_pattern_templates; i++) {
		table->pattern_templates[i] = pattern_templates[i];
		table->pattern_templates[i]->refcnt++;
	}
	table->nb_pattern_templates = nb_pattern_templates;
	for (i = 0; i < nb_actions_templates; i++) {
		table->actions_templates[i] = actions_templates[i];
		table->actions_templates[i]->refcnt++;
	}
	table->nb_actions_templates = nb_actions_templates;

	/* Track the table on the port so a flush can reconcile it. */
	TAILQ_INIT(&table->live_flows);
	table->next = npc->flow_tables;
	npc->flow_tables = table;

	return table;

free_index:
	plt_free(table->mcam_ids);
	plt_free(table->flow_pool);
	table->mcam_ids = NULL;
	table->flow_pool = NULL;
	plt_free(table->pattern_templates);
	plt_free(table->actions_templates);
	plt_free(table);
	if (*errcode == 0)
		*errcode = -ENOMEM;
	return NULL;

free_table:
	if (attr->insertion_type == ROC_NPC_TEMPLATE_INSERTION_INDEX_WITH_PATTERN) {
		uint32_t j;

		if (table->mcam_ids != NULL) {
			for (j = 0; j < nb_flows; j++) {
				if (table->mcam_ids[j] != -1)
					roc_npc_mcam_free_entry(roc_npc, table->mcam_ids[j]);
			}

			plt_free(table->mcam_ids);
		}
		plt_free(table->flow_pool);
	}
	plt_free(table->pattern_templates);
	plt_free(table->actions_templates);
	plt_free(table);

enomem:
	*errcode = -ENOMEM;
	return NULL;
}

int
roc_npc_template_table_destroy(struct roc_npc *roc_npc, struct roc_npc_template_table *table)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_template_table **pp;
	uint8_t i;

	if (table == NULL)
		return 0;

	if (table->insertion_type == ROC_NPC_TEMPLATE_INSERTION_PATTERN) {
		if (table->nb_live != 0)
			return -EBUSY;
	} else {
		uint32_t j;

		/* A live (enabled) index rule still owns its MCAM entry. */
		if (!table->hw_released && table->flow_pool != NULL) {
			for (j = 0; j < table->nb_flows; j++) {
				if (table->flow_pool[j].flow.enable != 0)
					return -EBUSY;
			}
		}

		if (table->mcam_ids != NULL) {
			/* Skip if a flush already freed the entries. */
			if (!table->hw_released) {
				for (j = 0; j < table->nb_flows; j++) {
					int mcam_id = table->mcam_ids[j];

					if (mcam_id != -1)
						roc_npc_mcam_free_entry(roc_npc, mcam_id);
				}
			}
			plt_free(table->mcam_ids);
		}
		plt_free(table->flow_pool);
	}

	for (i = 0; i < table->nb_pattern_templates; i++)
		table->pattern_templates[i]->refcnt--;

	for (i = 0; i < table->nb_actions_templates; i++)
		table->actions_templates[i]->refcnt--;

	/* Unlink from the port's table list. */
	pp = &npc->flow_tables;
	while (*pp != NULL) {
		if (*pp == table) {
			*pp = table->next;
			break;
		}
		pp = &(*pp)->next;
	}

	plt_free(table->pattern_templates);
	plt_free(table->actions_templates);
	plt_free(table);
	return 0;
}

struct roc_npc_template_flow *
roc_npc_async_flow_create(struct roc_npc *roc_npc, uint32_t queue_id,
			  struct roc_npc_template_table *table,
			  const struct roc_npc_item_info pattern[],
			  const struct roc_npc_action actions[], uint16_t dst_pf_func,
			  uint64_t npc_default_action, void *user_data, int *errcode)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_template_flow *flow = NULL;
	int nb_items = 0, nb_actions = 0;
	struct roc_npc_flow_queue *q;
	struct roc_npc_async_op *op;
	int k;

	if (table == NULL || pattern == NULL || actions == NULL || npc->flow_queues == NULL ||
	    queue_id >= npc->nb_flow_queues) {
		*errcode = -EINVAL;
		return NULL;
	}
	if (table->insertion_type != ROC_NPC_TEMPLATE_INSERTION_PATTERN) {
		*errcode = -EINVAL;
		return NULL;
	}

	while (pattern[nb_items].type != ROC_NPC_ITEM_TYPE_END)
		nb_items++;
	while (actions[nb_actions].type != ROC_NPC_ACTION_TYPE_END)
		nb_actions++;
	if (nb_items > ROC_NPC_ITEM_TYPE_END || nb_actions > ROC_NPC_MAX_ACTION_COUNT - 1) {
		*errcode = -ENOTSUP;
		return NULL;
	}

	q = &npc->flow_queues[queue_id];
	if (q->head - q->tail >= q->size) {
		*errcode = -ENOSPC;
		return NULL;
	}

	flow = plt_zmalloc(sizeof(*flow), 0);
	if (flow == NULL) {
		*errcode = -ENOMEM;
		return NULL;
	}
	flow->table = table;
	flow->nb_items = nb_items;
	flow->nb_actions = nb_actions;
	flow->items = plt_zmalloc((nb_items + 1) * sizeof(struct roc_npc_item_info), 0);
	if (flow->items == NULL) {
		*errcode = -ENOMEM;
		goto err;
	}

	flow->actions = plt_zmalloc((nb_actions + 1) * sizeof(struct roc_npc_action), 0);
	if (flow->actions == NULL) {
		*errcode = -ENOMEM;
		goto err;
	}

	/* Deep-copy each item so push() owns a self-contained rule. */
	for (k = 0; k < nb_items; k++) {
		uint32_t sz = pattern[k].size;

		flow->items[k].type = pattern[k].type;
		flow->items[k].size = sz;
		if (roc_npc_dup_item_field(pattern[k].spec, sz, &flow->items[k].spec) ||
		    roc_npc_dup_item_field(pattern[k].last, sz, &flow->items[k].last) ||
		    roc_npc_dup_item_field(pattern[k].mask, sz, &flow->items[k].mask)) {
			*errcode = -ENOMEM;
			goto err;
		}
	}
	flow->items[nb_items].type = ROC_NPC_ITEM_TYPE_END;

	/* Deep-copy each action conf. */
	for (k = 0; k < nb_actions; k++) {
		int sz = roc_npc_action_conf_size(actions[k].type);

		flow->actions[k].type = actions[k].type;
		flow->actions[k].rss_repte_pf_func = actions[k].rss_repte_pf_func;

		/* A malformed RSS conf (non-zero length with a NULL nested
		 * pointer) is invalid input, not an allocation failure.
		 */
		if (actions[k].type == ROC_NPC_ACTION_TYPE_RSS && actions[k].conf != NULL) {
			const struct roc_npc_action_rss *rss = actions[k].conf;

			if ((rss->queue_num && rss->queue == NULL) ||
			    (rss->key_len && rss->key == NULL)) {
				*errcode = -EINVAL;
				goto err;
			}
		}

		flow->actions[k].conf = roc_npc_dup_action_conf(&actions[k]);
		/* A NULL copy from a non-NULL conf now means the deep copy failed. */
		if (flow->actions[k].conf == NULL && actions[k].conf != NULL &&
		    (sz > 0 || actions[k].type == ROC_NPC_ACTION_TYPE_RSS)) {
			*errcode = -ENOMEM;
			goto err;
		}
	}
	flow->actions[nb_actions].type = ROC_NPC_ACTION_TYPE_END;

	/* Capture the rule attributes used by push() to program hardware. */
	flow->attr = table->flow_attr;
	if (table->transfer)
		flow->attr.ingress = 1;
	flow->dst_pf_func = dst_pf_func;
	flow->npc_default_action = npc_default_action;
	/* Capture the caller's RSS flow key so batched rules keep their own. */
	flow->flowkey_cfg = roc_npc->flowkey_cfg_state;

	op = &q->ops[q->head & (q->size - 1)];
	op->flow = flow;
	op->is_create = true;
	op->is_update = false;
	op->user_data = user_data;
	op->done = false;
	op->rc = 0;
	q->head++;

	return flow;

err:
	roc_npc_template_flow_free_rule(flow);
	plt_free(flow);
	return NULL;
}

int
roc_npc_async_flow_destroy(struct roc_npc *roc_npc, uint32_t queue_id,
			   struct roc_npc_template_flow *flow, void *user_data)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_flow_queue *q;
	struct roc_npc_async_op *op;

	if (flow == NULL || npc->flow_queues == NULL || queue_id >= npc->nb_flow_queues)
		return -EINVAL;

	q = &npc->flow_queues[queue_id];
	if (q->head - q->tail >= q->size)
		return -ENOSPC;

	op = &q->ops[q->head & (q->size - 1)];
	op->flow = flow;
	op->is_create = false;
	op->is_update = false;
	op->user_data = user_data;
	op->done = false;
	op->rc = 0;
	q->head++;

	return 0;
}

/* Create a rule at an app-chosen index in an INDEX_WITH_PATTERN table. */
struct roc_npc_template_flow *
roc_npc_async_flow_create_by_index_with_pattern(struct roc_npc *roc_npc, uint32_t queue_id,
						struct roc_npc_template_table *table,
						uint32_t rule_index,
						const struct roc_npc_item_info pattern[],
						const struct roc_npc_action actions[],
						void *user_data, int *errcode)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_template_flow *flow;
	uint32_t mark_save, vtag_save;
	struct roc_npc_flow_queue *q;
	struct roc_npc_async_op *op;
	struct roc_npc_attr attr;
	int rc;

	if (table == NULL || pattern == NULL || actions == NULL || npc->flow_queues == NULL ||
	    queue_id >= npc->nb_flow_queues) {
		*errcode = -EINVAL;
		return NULL;
	}
	if (table->insertion_type != ROC_NPC_TEMPLATE_INSERTION_INDEX_WITH_PATTERN) {
		*errcode = -EINVAL;
		return NULL;
	}
	if (rule_index >= table->nb_flows) {
		*errcode = -EINVAL;
		return NULL;
	}

	/* Reject creates into a flushed table. */
	if (table->hw_released) {
		*errcode = -EINVAL;
		return NULL;
	}

	q = &npc->flow_queues[queue_id];
	if (q->head - q->tail >= q->size) {
		*errcode = -ENOSPC;
		return NULL;
	}

	/* App owns the index: use the reserved pool slot directly. */
	flow = &table->flow_pool[rule_index];
	flow->table = table;
	flow->slot = rule_index;
	memset(&flow->flow, 0, sizeof(flow->flow));

	attr = table->flow_attr;
	if (table->transfer)
		attr.ingress = 1;

	/* Snapshot MARK/VLAN refcounts before parsing. */
	mark_save = npc->mark_actions;
	vtag_save = npc->vtag_strip_actions;

	rc = roc_npc_flow_parse(roc_npc, &attr, pattern, actions, &flow->flow);
	if (rc != 0) {
		npc->mark_actions = mark_save;
		npc->vtag_strip_actions = vtag_save;
		*errcode = rc;
		return NULL;
	}

	roc_npc_mcam_write_rx_finalize(roc_npc, &flow->flow);
	flow->flow.mcam_id = table->mcam_ids[rule_index];
	flow->flow.enable = 1;

	op = &q->ops[q->head & (q->size - 1)];
	op->flow = flow;
	op->is_create = true;
	op->is_update = false;
	op->user_data = user_data;
	op->done = false;
	op->rc = 0;
	q->head++;

	return flow;
}

/* Update a rule's actions in place and swap them in at push. */
int
roc_npc_async_flow_actions_update(struct roc_npc *roc_npc, uint32_t queue_id,
				  struct roc_npc_template_flow *flow,
				  const struct roc_npc_action actions[], void *user_data)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_item_info end_pattern[1];
	struct roc_npc_template_table *table;
	struct roc_npc_flow *fptr, tmpflow;
	uint32_t mark_save, vtag_save;
	struct roc_npc_flow_queue *q;
	struct roc_npc_async_op *op;
	struct roc_npc_attr attr;
	uint16_t old_match_id;
	bool old_vtag;
	int rc;

	if (flow == NULL || actions == NULL || npc->flow_queues == NULL ||
	    queue_id >= npc->nb_flow_queues)
		return -EINVAL;

	table = flow->table;
	if (table == NULL)
		return -EINVAL;
	/* PATTERN rules have roc_flow only after create commit. */
	if (table->insertion_type == ROC_NPC_TEMPLATE_INSERTION_PATTERN && flow->roc_flow == NULL)
		return -EINVAL;

	/* Reject a second update until the pending one is flushed. */
	if (flow->pending_update.pending)
		return -EBUSY;

	q = &npc->flow_queues[queue_id];
	if (q->head - q->tail >= q->size)
		return -ENOSPC;

	end_pattern[0].type = ROC_NPC_ITEM_TYPE_END;

	attr = table->flow_attr;
	if (table->transfer)
		attr.ingress = 1;

	memset(&tmpflow, 0, sizeof(tmpflow));

	/* Resolve the live flow before parsing the new actions. */
	fptr = flow->roc_flow ? flow->roc_flow : &flow->flow;
	old_match_id = (fptr->npc_action >> NPC_RX_ACT_MATCH_OFFSET) & NPC_RX_ACT_MATCH_MASK;
	old_vtag = fptr->nix_intf == ROC_NPC_INTF_RX && fptr->vtag_action != 0;
	mark_save = npc->mark_actions;
	vtag_save = npc->vtag_strip_actions;

	rc = roc_npc_flow_parse(roc_npc, &attr, end_pattern, actions, &tmpflow);
	if (rc != 0) {
		/* Undo any refcount bumps a partial parse made on failure. */
		npc->mark_actions = mark_save;
		npc->vtag_strip_actions = vtag_save;
		return rc;
	}

	/* Stash the parsed update so push can commit or roll it back. */
	flow->pending_update.npc_action = tmpflow.npc_action;
	flow->pending_update.npc_action2 = tmpflow.npc_action2;
	flow->pending_update.vtag_action = tmpflow.vtag_action;
	flow->pending_update.recv_queue = tmpflow.recv_queue;
	flow->pending_update.old_match_id = old_match_id != 0;
	flow->pending_update.old_vtag = old_vtag;
	flow->pending_update.new_match_id =
		((tmpflow.npc_action >> NPC_RX_ACT_MATCH_OFFSET) & NPC_RX_ACT_MATCH_MASK) != 0;
	flow->pending_update.new_vtag =
		tmpflow.nix_intf == ROC_NPC_INTF_RX && tmpflow.vtag_action != 0;
	flow->pending_update.pending = true;

	op = &q->ops[q->head & (q->size - 1)];
	op->flow = flow;
	op->is_create = false;
	op->is_update = true;
	op->user_data = user_data;
	op->done = false;
	op->rc = 0;
	q->head++;

	return 0;
}

/* Commit one retained create to hardware. */
static int
roc_npc_flow_commit_create(struct roc_npc *roc_npc, struct roc_npc_template_flow *flow)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	uint32_t mark_save, vtag_save;
	struct roc_npc_flow *rflow;
	int errcode = 0;

	/* Restore this rule's RSS flow key before programming (see create). */
	roc_npc->flowkey_cfg_state = flow->flowkey_cfg;

	/* Restore MARK/VLAN refcounts if create fails. */
	mark_save = npc->mark_actions;
	vtag_save = npc->vtag_strip_actions;

	rflow = roc_npc_flow_create(roc_npc, &flow->attr, flow->items, flow->actions,
				    flow->dst_pf_func, flow->npc_default_action, &errcode);
	if (rflow == NULL) {
		npc->mark_actions = mark_save;
		npc->vtag_strip_actions = vtag_save;
		return errcode ? errcode : -EINVAL;
	}

	flow->roc_flow = rflow;
	return 0;
}

/* Apply a pending actions update and roll it back on failure. */
static int
roc_npc_flow_apply_pattern_update(struct roc_npc *roc_npc, struct npc *npc,
				  struct roc_npc_template_flow *flow)
{
	struct roc_npc_flow *fptr = flow->roc_flow;
	uint64_t save_action2, save_action, save_vtag;
	uint32_t save_rq;
	int rc;

	if (fptr == NULL)
		return -EINVAL;

	save_action2 = fptr->npc_action2;
	save_action = fptr->npc_action;
	save_vtag = fptr->vtag_action;
	save_rq = fptr->recv_queue;

	fptr->npc_action = flow->pending_update.npc_action;
	fptr->npc_action2 = flow->pending_update.npc_action2;
	fptr->vtag_action = flow->pending_update.vtag_action;
	fptr->recv_queue = flow->pending_update.recv_queue;
	fptr->enable = 1;
	roc_npc_mcam_write_rx_finalize(roc_npc, fptr);
	rc = roc_npc_mcam_write_entry(roc_npc, fptr);
	if (rc == 0) {
		/* Drop the old action set's refcount contribution. */
		if (flow->pending_update.old_match_id && npc->mark_actions > 0)
			npc->mark_actions--;
		if (flow->pending_update.old_vtag && npc->vtag_strip_actions > 0)
			npc->vtag_strip_actions--;
	} else {
		/* Restore the live entry and undo the new contribution. */
		fptr->npc_action = save_action;
		fptr->npc_action2 = save_action2;
		fptr->vtag_action = save_vtag;
		fptr->recv_queue = save_rq;
		if (flow->pending_update.new_match_id && npc->mark_actions > 0)
			npc->mark_actions--;
		if (flow->pending_update.new_vtag && npc->vtag_strip_actions > 0)
			npc->vtag_strip_actions--;
	}
	return rc;
}

int
roc_npc_flow_push(struct roc_npc *roc_npc, uint32_t queue_id)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_flow_queue *q;
	uint32_t i;

	if (npc->flow_queues == NULL || queue_id >= npc->nb_flow_queues)
		return -EINVAL;

	q = &npc->flow_queues[queue_id];

	for (i = q->pushed; i != q->head; i++) {
		struct roc_npc_async_op *op = &q->ops[i & (q->size - 1)];
		struct roc_npc_template_flow *flow = op->flow;
		struct roc_npc_template_table *table = flow->table;
		int rc;

		if (table->insertion_type == ROC_NPC_TEMPLATE_INSERTION_PATTERN) {
			if (op->is_update) {
				rc = roc_npc_flow_apply_pattern_update(roc_npc, npc, flow);
				/* Clear the pending update after push. */
				flow->pending_update.pending = false;
			} else if (op->is_create) {
				rc = roc_npc_flow_commit_create(roc_npc, flow);
				/* Rule arrays are consumed on push. */
				roc_npc_template_flow_free_rule(flow);
				if (rc == 0) {
					plt_spinlock_lock(&table->lock);
					table->nb_live++;
					TAILQ_INSERT_TAIL(&table->live_flows, flow, link);
					plt_spinlock_unlock(&table->lock);
				}
				/* Keep the handle valid on failure. */
			} else if (flow->roc_flow == NULL) {
				/* Never committed (or a failed create): just free it. */
				rc = 0;
				roc_npc_template_flow_free_rule(flow);
				plt_free(flow);
			} else {
				uint16_t match_id;
				uint8_t nix_intf;
				bool had_vtag;

				/* Capture match_id and interface before destroy. */
				match_id = (flow->roc_flow->npc_action >> NPC_RX_ACT_MATCH_OFFSET) &
					   NPC_RX_ACT_MATCH_MASK;
				nix_intf = flow->roc_flow->nix_intf;
				had_vtag = flow->roc_flow->vtag_action != 0;

				rc = roc_npc_flow_destroy(roc_npc, flow->roc_flow);
				if (rc == 0 && match_id)
					roc_npc_mark_actions_sub_return(roc_npc, 1);
				if (rc == 0 && nix_intf == ROC_NPC_INTF_RX && had_vtag)
					roc_npc_vtag_actions_sub_return(roc_npc, 1);
				if (rc == 0) {
					plt_spinlock_lock(&table->lock);
					table->nb_live--;
					TAILQ_REMOVE(&table->live_flows, flow, link);
					plt_spinlock_unlock(&table->lock);
					flow->roc_flow = NULL;
					roc_npc_template_flow_free_rule(flow);
					plt_free(flow);
				}
				/* Keep the handle on failure so the caller can retry. */
			}
		} else {
			/* INDEX_WITH_PATTERN: entry fixed at the app-chosen index. */
			struct roc_npc_flow *fptr = &flow->flow;
			bool destroy = !op->is_create && !op->is_update;
			uint16_t match_id = (fptr->npc_action >> NPC_RX_ACT_MATCH_OFFSET) &
					    NPC_RX_ACT_MATCH_MASK;
			uint64_t save_action2 = fptr->npc_action2;
			uint64_t save_action = fptr->npc_action;
			uint64_t save_vtag = fptr->vtag_action;
			uint32_t save_rq = fptr->recv_queue;

			/* Swap in the stashed update before writing hardware. */
			if (op->is_update) {
				fptr->npc_action = flow->pending_update.npc_action;
				fptr->npc_action2 = flow->pending_update.npc_action2;
				fptr->vtag_action = flow->pending_update.vtag_action;
				fptr->recv_queue = flow->pending_update.recv_queue;
			}

			/* Set enable before writing hardware. */
			fptr->enable = destroy ? 0 : 1;
			if (op->is_update)
				roc_npc_mcam_write_rx_finalize(roc_npc, fptr);
			rc = roc_npc_mcam_write_entry(roc_npc, fptr);
			if (rc != 0) {
				/* Restore the previous enable state on failure. */
				fptr->enable = destroy ? 1 : 0;
				/* Roll back the swapped-in action words too. */
				if (op->is_update) {
					fptr->npc_action = save_action;
					fptr->npc_action2 = save_action2;
					fptr->vtag_action = save_vtag;
					fptr->recv_queue = save_rq;
				}
			}

			/* Commit or roll back the update refcount change. */
			if (op->is_update) {
				if (rc == 0) {
					if (flow->pending_update.old_match_id &&
					    npc->mark_actions > 0)
						npc->mark_actions--;
					if (flow->pending_update.old_vtag &&
					    npc->vtag_strip_actions > 0)
						npc->vtag_strip_actions--;
				} else {
					if (flow->pending_update.new_match_id &&
					    npc->mark_actions > 0)
						npc->mark_actions--;
					if (flow->pending_update.new_vtag &&
					    npc->vtag_strip_actions > 0)
						npc->vtag_strip_actions--;
				}
				flow->pending_update.pending = false;
			}

			/* Release the COUNT counter on destroy. */
			if (destroy && rc == 0 && fptr->use_ctr &&
			    fptr->ctr_id != NPC_COUNTER_NONE) {
				roc_npc_mcam_free_counter(roc_npc, fptr->ctr_id);
				fptr->ctr_id = NPC_COUNTER_NONE;
			}

			/* Balance the mark_actions refcount for MARK/FLAG rules. */
			if (destroy && rc == 0 && match_id)
				roc_npc_mark_actions_sub_return(roc_npc, 1);

			/* Balance the vtag_strip_actions refcount for RX VLAN strip rules. */
			if (destroy && rc == 0 && fptr->nix_intf == ROC_NPC_INTF_RX &&
			    fptr->vtag_action != 0)
				roc_npc_vtag_actions_sub_return(roc_npc, 1);
		}

		op->rc = rc;
		op->done = true;
	}

	q->pushed = q->head;
	return 0;
}

int
roc_npc_flow_pull(struct roc_npc *roc_npc, uint32_t queue_id, struct roc_npc_flow_op_result res[],
		  uint16_t n_res)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_flow_queue *q;
	uint16_t cnt = 0;

	if (npc->flow_queues == NULL || queue_id >= npc->nb_flow_queues)
		return -EINVAL;

	q = &npc->flow_queues[queue_id];

	/* Return ops that were pushed but not yet reported. */
	while (q->tail != q->pushed && cnt < n_res) {
		struct roc_npc_async_op *op = &q->ops[q->tail & (q->size - 1)];

		res[cnt].user_data = op->user_data;
		res[cnt].rc = op->rc;
		cnt++;
		q->tail++;
	}

	return cnt;
}

/* Resolve a candidate handle to a template flow or a sync flow. */
bool
roc_npc_async_flow_resolve(struct roc_npc *roc_npc, void *handle, struct roc_npc_flow **flow)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_template_flow *cand = handle;
	struct roc_npc_template_table *table;
	uint16_t i;

	if (handle == NULL || flow == NULL)
		return false;

	for (table = npc->flow_tables; table != NULL; table = table->next) {
		struct roc_npc_template_flow *match;

		/* Integer range check: pointer comparison across objects is UB. */
		if (table->flow_pool != NULL) {
			uintptr_t p = (uintptr_t)cand;
			uintptr_t base = (uintptr_t)table->flow_pool;
			uintptr_t end = (uintptr_t)(table->flow_pool + table->nb_flows);

			if (p >= base && p < end) {
				*flow = &cand->flow;
				return true;
			}
		}

		plt_spinlock_lock(&table->lock);
		/* clang-format off */
		TAILQ_FOREACH(match, &table->live_flows, link) {
			if (match == cand) {
				plt_spinlock_unlock(&table->lock);
				*flow = cand->roc_flow;
				return true;
			}
		}
		/* clang-format on */
		plt_spinlock_unlock(&table->lock);
	}

	/* Handle still queued: a PATTERN rule not yet committed. */
	for (i = 0; i < npc->nb_flow_queues; i++) {
		struct roc_npc_flow_queue *q = &npc->flow_queues[i];
		uint32_t j;

		for (j = q->tail; j != q->head; j++) {
			struct roc_npc_async_op *op = &q->ops[j & (q->size - 1)];

			if (op->flow == cand) {
				*flow = cand->roc_flow;
				return true;
			}
		}
	}

	return false;
}

/* Return the table cookie used to recover update context. */
void *
roc_npc_async_flow_table_cookie(struct roc_npc_template_flow *flow)
{
	if (flow == NULL || flow->table == NULL)
		return NULL;

	return flow->table->cookie;
}

/* Reconcile template-table bookkeeping after flush or port close. */
void
npc_template_tables_flush_reconcile(struct npc *npc)
{
	struct roc_npc_template_table *table;
	struct roc_npc_template_flow *flow;
	struct roc_npc_flow *fptr;
	uint16_t match_id;
	uint32_t j;

	for (table = npc->flow_tables; table != NULL; table = table->next) {
		plt_spinlock_lock(&table->lock);
		while ((flow = TAILQ_FIRST(&table->live_flows)) != NULL) {
			TAILQ_REMOVE(&table->live_flows, flow, link);
			/* roc_flow is already freed by free_all_resources. */
			roc_npc_template_flow_free_rule(flow);
			plt_free(flow);
		}

		/* INDEX_WITH_PATTERN rules live in flow_pool[]; clear stale state. */
		if (table->flow_pool != NULL) {
			for (j = 0; j < table->nb_flows; j++) {
				fptr = &table->flow_pool[j].flow;

				if (fptr->enable == 0)
					continue;

				match_id = (fptr->npc_action >> NPC_RX_ACT_MATCH_OFFSET) &
					   NPC_RX_ACT_MATCH_MASK;
				if (match_id && npc->mark_actions > 0)
					npc->mark_actions--;

				if (fptr->nix_intf == ROC_NPC_INTF_RX && fptr->vtag_action != 0 &&
				    npc->vtag_strip_actions > 0)
					npc->vtag_strip_actions--;

				memset(fptr, 0, sizeof(*fptr));
				fptr->ctr_id = NPC_COUNTER_NONE;
			}
		}

		table->nb_live = 0;
		table->hw_released = true;
		plt_spinlock_unlock(&table->lock);
	}
}
