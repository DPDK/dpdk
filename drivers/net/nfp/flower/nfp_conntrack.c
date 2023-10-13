/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Corigine, Inc.
 * All rights reserved.
 */

#include "nfp_conntrack.h"

#include <rte_malloc.h>
#include <rte_hash.h>
#include <rte_jhash.h>

#include "../nfp_logs.h"
#include "nfp_flower_cmsg.h"
#include "nfp_flower_representor.h"

struct ct_data {
	uint8_t  ct_state;        /* Connection state. */
	uint16_t ct_zone;         /* Connection zone. */
};

enum ct_entry_type {
	CT_TYPE_PRE_CT,
	CT_TYPE_POST_CT,
};

struct nfp_initial_flow {
	struct rte_flow_item *items;
	struct rte_flow_action *actions;
	uint8_t items_cnt;
	uint8_t actions_cnt;
};

struct nfp_ct_flow_entry {
	uint64_t cookie;
	LIST_ENTRY(nfp_ct_flow_entry) pre_ct_list;
	LIST_ENTRY(nfp_ct_flow_entry) post_ct_list;
	LIST_HEAD(, nfp_ct_merge_entry) children;
	enum ct_entry_type type;
	struct nfp_flower_representor *repr;
	struct nfp_ct_zone_entry *ze;
	struct nfp_initial_flow rule;
	struct nfp_fl_stats stats;
};

struct nfp_ct_map_entry {
	uint64_t cookie;
	struct nfp_ct_flow_entry *fe;
};

struct nfp_ct_zone_entry {
	uint32_t zone;
	struct nfp_flow_priv *priv;
	LIST_HEAD(, nfp_ct_flow_entry) pre_ct_list;
	LIST_HEAD(, nfp_ct_flow_entry) post_ct_list;
	struct rte_hash *ct_merge_table;
};

struct nfp_ct_merge_entry {
	uint64_t cookie[2];
	uint32_t ctx_id;
	LIST_ENTRY(nfp_ct_merge_entry) pre_ct_list;
	LIST_ENTRY(nfp_ct_merge_entry) post_ct_list;
	struct nfp_initial_flow rule;
	struct rte_flow *compiled_rule;
	struct nfp_ct_zone_entry *ze;
	struct nfp_ct_flow_entry *pre_ct_parent;
	struct nfp_ct_flow_entry *post_ct_parent;
};

/* OVS_KEY_ATTR_CT_STATE flags */
#define OVS_CS_F_NEW            0x01 /* Beginning of a new connection. */
#define OVS_CS_F_ESTABLISHED    0x02 /* Part of an existing connection. */
#define OVS_CS_F_RELATED        0x04 /* Related to an established connection. */
#define OVS_CS_F_REPLY_DIR      0x08 /* Flow is in the reply direction. */
#define OVS_CS_F_INVALID        0x10 /* Could not track connection. */
#define OVS_CS_F_TRACKED        0x20 /* Conntrack has occurred. */
#define OVS_CS_F_SRC_NAT        0x40 /* Packet's source address/port was mangled by NAT. */
#define OVS_CS_F_DST_NAT        0x80 /* Packet's destination address/port was mangled by NAT. */

typedef void (*nfp_action_free_fn)(void *field);
typedef bool (*nfp_action_copy_fn)(const void *src, void *dst);

static bool
is_pre_ct_flow(const struct ct_data *ct,
		const struct rte_flow_action *actions)
{
	const struct rte_flow_action *action;

	if (ct == NULL)
		return false;

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; ++action) {
		if (action->type == RTE_FLOW_ACTION_TYPE_CONNTRACK)
			return true;
	}

	return false;
}

static bool
is_post_ct_flow(const struct ct_data *ct)
{
	if (ct == NULL)
		return false;

	if ((ct->ct_state & OVS_CS_F_ESTABLISHED) != 0)
		return true;

	return false;
}

static bool
is_ct_commit_flow(const struct ct_data *ct)
{
	if (ct == NULL)
		return false;

	if ((ct->ct_state & OVS_CS_F_NEW) != 0)
		return true;

	return false;
}

static struct nfp_ct_merge_entry *
nfp_ct_merge_table_search(struct nfp_ct_zone_entry *ze,
		char *hash_data,
		uint32_t hash_len)
{
	int index;
	uint32_t hash_key;
	struct nfp_ct_merge_entry *m_ent;

	hash_key = rte_jhash(hash_data, hash_len, ze->priv->hash_seed);
	index = rte_hash_lookup_data(ze->ct_merge_table, &hash_key, (void **)&m_ent);
	if (index < 0) {
		PMD_DRV_LOG(DEBUG, "Data NOT found in the ct_merge table");
		return NULL;
	}

	return m_ent;
}

static bool
nfp_ct_merge_table_add(struct nfp_ct_zone_entry *ze,
		struct nfp_ct_merge_entry *merge_entry)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(merge_entry, sizeof(uint64_t) * 2, ze->priv->hash_seed);
	ret = rte_hash_add_key_data(ze->ct_merge_table, &hash_key, merge_entry);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add to ct_merge table failed");
		return false;
	}

	return true;
}

static void
nfp_ct_merge_table_delete(struct nfp_ct_zone_entry *ze,
		struct nfp_ct_merge_entry *m_ent)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(m_ent, sizeof(uint64_t) * 2, ze->priv->hash_seed);
	ret = rte_hash_del_key(ze->ct_merge_table, &hash_key);
	if (ret < 0)
		PMD_DRV_LOG(ERR, "Delete from ct_merge table failed, ret=%d", ret);
}

static void
nfp_ct_merge_entry_destroy(struct nfp_ct_merge_entry *m_ent)
{
	struct nfp_ct_zone_entry *ze;

	ze = m_ent->ze;
	nfp_ct_merge_table_delete(ze, m_ent);

	rte_free(m_ent->rule.actions);
	rte_free(m_ent->rule.items);
	LIST_REMOVE(m_ent, pre_ct_list);
	LIST_REMOVE(m_ent, post_ct_list);
	rte_free(m_ent);
}

struct nfp_ct_map_entry *
nfp_ct_map_table_search(struct nfp_flow_priv *priv,
		char *hash_data,
		uint32_t hash_len)
{
	int index;
	uint32_t hash_key;
	struct nfp_ct_map_entry *me;

	hash_key = rte_jhash(hash_data, hash_len, priv->hash_seed);
	index = rte_hash_lookup_data(priv->ct_map_table, &hash_key, (void **)&me);
	if (index < 0) {
		PMD_DRV_LOG(DEBUG, "Data NOT found in the ct_map table");
		return NULL;
	}

	return me;
}

static bool
nfp_ct_map_table_add(struct nfp_flow_priv *priv,
		struct nfp_ct_map_entry *me)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(me, sizeof(uint64_t), priv->hash_seed);
	ret = rte_hash_add_key_data(priv->ct_map_table, &hash_key, me);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add to ct_map table failed");
		return false;
	}

	return true;
}

static void
nfp_ct_map_table_delete(struct nfp_flow_priv *priv,
		struct nfp_ct_map_entry *me)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(me, sizeof(uint64_t), priv->hash_seed);
	ret = rte_hash_del_key(priv->ct_map_table, &hash_key);
	if (ret < 0)
		PMD_DRV_LOG(ERR, "Delete form ct_map table failed");
}

static void
nfp_ct_map_entry_destroy(struct nfp_ct_map_entry *me)
{
	rte_free(me);
}

static void
nfp_ct_flow_item_free_real(void *field,
		enum rte_flow_item_type type)
{
	switch (type) {
	case RTE_FLOW_ITEM_TYPE_VOID:
		break;
	case RTE_FLOW_ITEM_TYPE_ETH:        /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_VLAN:       /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_IPV4:       /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_IPV6:       /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_TCP:        /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_UDP:        /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_SCTP:       /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_VXLAN:      /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_GRE:        /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_GRE_KEY:    /* FALLTHROUGH */
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		rte_free(field);
		break;
	default:
		break;
	}
}

static void
nfp_ct_flow_item_free(struct rte_flow_item *item)
{
	if (item->spec != NULL)
		nfp_ct_flow_item_free_real((void *)(ptrdiff_t)item->spec, item->type);

	if (item->mask != NULL)
		nfp_ct_flow_item_free_real((void *)(ptrdiff_t)item->mask, item->type);

	if (item->last != NULL)
		nfp_ct_flow_item_free_real((void *)(ptrdiff_t)item->last, item->type);
}

static void
nfp_ct_flow_items_free(struct rte_flow_item *items,
		uint8_t item_cnt)
{
	uint8_t loop;

	for (loop = 0; loop < item_cnt; ++loop)
		nfp_ct_flow_item_free(items + loop);
}

static bool
nfp_flow_item_conf_size_get(enum rte_flow_item_type type,
		size_t *size)
{
	size_t len = 0;

	switch (type) {
	case RTE_FLOW_ITEM_TYPE_VOID:
		break;
	case RTE_FLOW_ITEM_TYPE_ETH:
		len = sizeof(struct rte_flow_item_eth);
		break;
	case RTE_FLOW_ITEM_TYPE_VLAN:
		len = sizeof(struct rte_flow_item_vlan);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV4:
		len = sizeof(struct rte_flow_item_ipv4);
		break;
	case RTE_FLOW_ITEM_TYPE_IPV6:
		len = sizeof(struct rte_flow_item_ipv6);
		break;
	case RTE_FLOW_ITEM_TYPE_TCP:
		len = sizeof(struct rte_flow_item_tcp);
		break;
	case RTE_FLOW_ITEM_TYPE_UDP:
		len = sizeof(struct rte_flow_item_udp);
		break;
	case RTE_FLOW_ITEM_TYPE_SCTP:
		len = sizeof(struct rte_flow_item_sctp);
		break;
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		len = sizeof(struct rte_flow_item_vxlan);
		break;
	case RTE_FLOW_ITEM_TYPE_GRE:
		len = sizeof(struct rte_flow_item_gre);
		break;
	case RTE_FLOW_ITEM_TYPE_GRE_KEY:
		len = sizeof(rte_be32_t);
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
		len = sizeof(struct rte_flow_item_geneve);
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported item type: %d", type);
		*size = 0;
		return false;
	}

	*size = len;

	return true;
}

static void *
nfp_ct_flow_item_copy_real(const void *src,
		enum rte_flow_item_type type)
{
	bool ret;
	void *dst;
	size_t len;

	ret = nfp_flow_item_conf_size_get(type, &len);
	if (!ret) {
		PMD_DRV_LOG(ERR, "Get flow item conf size failed");
		return NULL;
	}

	dst = rte_zmalloc("flow_item", len, 0);
	if (dst == NULL) {
		PMD_DRV_LOG(ERR, "Malloc memory for ct item failed");
		return NULL;
	}

	rte_memcpy(dst, src, len);

	return dst;
}

static bool
nfp_ct_flow_item_copy(const struct rte_flow_item *src,
		struct rte_flow_item *dst)
{
	dst->type = src->type;

	if (src->spec != NULL) {
		dst->spec = nfp_ct_flow_item_copy_real(src->spec, src->type);
		if (dst->spec == NULL) {
			PMD_DRV_LOG(ERR, "Copy spec of ct item failed");
			goto end;
		}
	}

	if (src->mask != NULL) {
		dst->mask = nfp_ct_flow_item_copy_real(src->mask, src->type);
		if (dst->mask == NULL) {
			PMD_DRV_LOG(ERR, "Copy mask of ct item failed");
			goto free_spec;
		}
	}

	if (src->last != NULL) {
		dst->last = nfp_ct_flow_item_copy_real(src->last, src->type);
		if (dst->last == NULL) {
			PMD_DRV_LOG(ERR, "Copy last of ct item failed");
			goto free_mask;
		}
	}

	return true;

free_mask:
	nfp_ct_flow_item_free_real((void *)(ptrdiff_t)dst->mask, dst->type);
free_spec:
	nfp_ct_flow_item_free_real((void *)(ptrdiff_t)dst->spec, dst->type);
end:
	return false;
}

static bool
nfp_ct_flow_items_copy(const struct rte_flow_item *src,
		struct rte_flow_item *dst,
		uint8_t item_cnt)
{
	bool ret;
	uint8_t loop;

	for (loop = 0; loop < item_cnt; ++loop) {
		ret = nfp_ct_flow_item_copy(src + loop, dst + loop);
		if (!ret) {
			PMD_DRV_LOG(ERR, "Copy ct item failed");
			nfp_ct_flow_items_free(dst, loop);
			return false;
		}
	}

	return true;
}

static void
nfp_ct_flow_action_free_real(void *field,
		nfp_action_free_fn func)
{
	if (func != NULL)
		func(field);

	rte_free(field);
}

static void
nfp_ct_flow_action_free_vxlan(void *field)
{
	struct vxlan_data *vxlan = field;

	nfp_ct_flow_items_free(vxlan->items, ACTION_VXLAN_ENCAP_ITEMS_NUM);
}

static void
nfp_ct_flow_action_free_raw(void *field)
{
	struct rte_flow_action_raw_encap *raw_encap = field;

	rte_free(raw_encap->data);
}

static void
nfp_ct_flow_action_free(struct rte_flow_action *action)
{
	nfp_action_free_fn func = NULL;

	if (action->conf == NULL)
		return;

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_VOID:           /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_DROP:           /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_COUNT:          /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_JUMP:           /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:    /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:    /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
		return;
	case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:    /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:    /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_PORT_ID:        /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_TTL:        /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:     /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
		break;
	case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		func = nfp_ct_flow_action_free_vxlan;
		break;
	case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		func = nfp_ct_flow_action_free_raw;
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported action type: %d", action->type);
		break;
	}

	nfp_ct_flow_action_free_real((void *)(ptrdiff_t)action->conf, func);
}

static void
nfp_ct_flow_actions_free(struct rte_flow_action *actions,
		uint8_t action_cnt)
{
	uint8_t loop;

	for (loop = 0; loop < action_cnt; ++loop)
		nfp_ct_flow_action_free(actions + loop);
}

static void *
nfp_ct_flow_action_copy_real(const void *src,
		size_t len,
		nfp_action_copy_fn func)
{
	bool ret;
	void *dst;

	dst = rte_zmalloc("flow_action", len, 0);
	if (dst == NULL) {
		PMD_DRV_LOG(ERR, "Malloc memory for ct action failed");
		return NULL;
	}

	if (func != NULL) {
		ret = func(src, dst);
		if (!ret) {
			PMD_DRV_LOG(ERR, "Copy ct action failed");
			return NULL;
		}

		return dst;
	}

	rte_memcpy(dst, src, len);

	return dst;
}

static bool
nfp_ct_flow_action_copy_vxlan(const void *src,
		void *dst)
{
	struct vxlan_data *vxlan_dst = dst;
	const struct vxlan_data *vxlan_src = src;

	vxlan_dst->conf.definition = vxlan_dst->items;
	return nfp_ct_flow_items_copy(vxlan_src->items, vxlan_dst->items,
			ACTION_VXLAN_ENCAP_ITEMS_NUM);
}

static bool
nfp_ct_flow_action_copy_raw(const void *src,
		void *dst)
{
	struct rte_flow_action_raw_encap *raw_dst = dst;
	const struct rte_flow_action_raw_encap *raw_src = src;

	raw_dst->size = raw_src->size;
	raw_dst->data = nfp_ct_flow_action_copy_real(raw_src->data,
			raw_src->size, NULL);
	if (raw_dst->data == NULL) {
		PMD_DRV_LOG(ERR, "Copy ct action process failed");
		return false;
	}

	return true;
}

static bool
nfp_ct_flow_action_copy(const struct rte_flow_action *src,
		struct rte_flow_action *dst)
{
	size_t len;
	nfp_action_copy_fn func = NULL;

	dst->type = src->type;

	if (src->conf == NULL)
		return true;

	switch (src->type) {
	case RTE_FLOW_ACTION_TYPE_VOID:         /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_DROP:         /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_COUNT:        /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_JUMP:         /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
		return true;
	case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
		len = sizeof(struct rte_flow_action_set_mac);
		break;
	case RTE_FLOW_ACTION_TYPE_PORT_ID:
		len = sizeof(struct rte_flow_action_port_id);
		break;
	case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
		len = sizeof(struct rte_flow_action_of_push_vlan);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
		len = sizeof(struct rte_flow_action_set_ipv4);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
		len = sizeof(struct rte_flow_action_set_dscp);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
		len = sizeof(struct rte_flow_action_set_ipv6);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_TTL:
		len = sizeof(struct rte_flow_action_set_ttl);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:  /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
		len = sizeof(struct rte_flow_action_set_tp);
		break;
	case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
		len = sizeof(struct vxlan_data);
		func = nfp_ct_flow_action_copy_vxlan;
		break;
	case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
		len = sizeof(struct rte_flow_action_raw_encap);
		func = nfp_ct_flow_action_copy_raw;
		break;
	default:
		PMD_DRV_LOG(DEBUG, "Unsupported action type: %d", src->type);
		return false;
	}

	dst->conf = nfp_ct_flow_action_copy_real(src->conf, len, func);
	if (dst->conf == NULL) {
		PMD_DRV_LOG(DEBUG, "Copy ct action process failed");
		return false;
	}

	return true;
}

static bool
nfp_ct_flow_actions_copy(const struct rte_flow_action *src,
		struct rte_flow_action *dst,
		uint8_t action_cnt)
{
	bool ret;
	uint8_t loop;

	for (loop = 0; loop < action_cnt; ++loop) {
		ret = nfp_ct_flow_action_copy(src + loop, dst + loop);
		if (!ret) {
			PMD_DRV_LOG(DEBUG, "Copy ct action failed");
			nfp_ct_flow_actions_free(dst, loop);
			return false;
		}
	}

	return true;
}

static struct nfp_ct_flow_entry *
nfp_ct_flow_entry_get(struct nfp_ct_zone_entry *ze,
		struct nfp_flower_representor *repr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		uint64_t cookie)
{
	bool ret;
	uint8_t loop;
	uint8_t item_cnt = 1;      /* The RTE_FLOW_ITEM_TYPE_END */
	uint8_t action_cnt = 1;    /* The RTE_FLOW_ACTION_TYPE_END */
	struct nfp_flow_priv *priv;
	struct nfp_ct_map_entry *me;
	struct nfp_ct_flow_entry *fe;

	fe = rte_zmalloc("ct_flow_entry", sizeof(*fe), 0);
	if (fe == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc ct_flow entry");
		return NULL;
	}

	fe->ze = ze;
	fe->repr = repr;
	fe->cookie = cookie;
	LIST_INIT(&fe->children);

	for (loop = 0; (items + loop)->type != RTE_FLOW_ITEM_TYPE_END; loop++)
		item_cnt++;
	for (loop = 0; (actions + loop)->type != RTE_FLOW_ACTION_TYPE_END; loop++)
		action_cnt++;

	fe->rule.items = rte_zmalloc("ct_flow_item",
			sizeof(struct rte_flow_item) * item_cnt, 0);
	if (fe->rule.items == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc ct flow items");
		goto free_flow_entry;
	}

	fe->rule.actions = rte_zmalloc("ct_flow_action",
			sizeof(struct rte_flow_action) * action_cnt, 0);
	if (fe->rule.actions == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc ct flow actions");
		goto free_flow_item;
	}

	/* Deep copy of items */
	ret = nfp_ct_flow_items_copy(items, fe->rule.items, item_cnt);
	if (!ret) {
		PMD_DRV_LOG(ERR, "Could not deep copy ct flow items");
		goto free_flow_action;
	}

	/* Deep copy of actions */
	ret = nfp_ct_flow_actions_copy(actions, fe->rule.actions, action_cnt);
	if (!ret) {
		PMD_DRV_LOG(ERR, "Could not deep copy ct flow actions");
		goto free_copied_items;
	}

	fe->rule.items_cnt = item_cnt;
	fe->rule.actions_cnt = action_cnt;

	/* Now add a ct map entry */
	me = rte_zmalloc("ct_map_entry", sizeof(*me), 0);
	if (me == NULL) {
		PMD_DRV_LOG(ERR, "Malloc memory for ct map entry failed");
		goto free_copied_actions;
	}

	me->cookie = fe->cookie;
	me->fe = fe;

	priv = repr->app_fw_flower->flow_priv;
	ret = nfp_ct_map_table_add(priv, me);
	if (!ret) {
		PMD_DRV_LOG(ERR, "Add into ct map table failed");
		goto free_map_entry;
	}

	return fe;

free_map_entry:
	nfp_ct_map_entry_destroy(me);
free_copied_actions:
	nfp_ct_flow_actions_free(fe->rule.actions, action_cnt);
free_copied_items:
	nfp_ct_flow_items_free(fe->rule.items, item_cnt);
free_flow_action:
	rte_free(fe->rule.actions);
free_flow_item:
	rte_free(fe->rule.items);
free_flow_entry:
	rte_free(fe);

	return NULL;
}

static void
nfp_flow_children_merge_free(struct nfp_ct_flow_entry *fe)
{
	struct nfp_ct_merge_entry *m_ent;

	switch (fe->type) {
	case CT_TYPE_PRE_CT:
		LIST_FOREACH(m_ent, &fe->children, pre_ct_list)
			nfp_ct_merge_entry_destroy(m_ent);
		break;
	case CT_TYPE_POST_CT:
		LIST_FOREACH(m_ent, &fe->children, post_ct_list)
			nfp_ct_merge_entry_destroy(m_ent);
		break;
	default:
		break;
	}
}

static void
nfp_ct_flow_entry_destroy_partly(struct nfp_ct_flow_entry *fe)
{
	struct nfp_ct_map_entry *me;

	if (!LIST_EMPTY(&fe->children))
		nfp_flow_children_merge_free(fe);

	me = nfp_ct_map_table_search(fe->ze->priv, (char *)&fe->cookie, sizeof(uint64_t));
	if (me != NULL) {
		nfp_ct_map_table_delete(fe->ze->priv, me);
		nfp_ct_map_entry_destroy(me);
	}

	nfp_ct_flow_actions_free(fe->rule.actions, fe->rule.actions_cnt);
	nfp_ct_flow_items_free(fe->rule.items, fe->rule.items_cnt);
	rte_free(fe->rule.actions);
	rte_free(fe->rule.items);
	rte_free(fe);
}

static void
nfp_ct_flow_entry_destroy(struct nfp_ct_flow_entry *fe)
{
	LIST_REMOVE(fe, pre_ct_list);
	LIST_REMOVE(fe, post_ct_list);

	nfp_ct_flow_entry_destroy_partly(fe);
}

static struct nfp_ct_zone_entry *
nfp_ct_zone_table_search(struct nfp_flow_priv *priv,
		char *hash_data,
		uint32_t hash_len)
{
	int index;
	uint32_t hash_key;
	struct nfp_ct_zone_entry *ze;

	hash_key = rte_jhash(hash_data, hash_len, priv->hash_seed);
	index = rte_hash_lookup_data(priv->ct_zone_table, &hash_key, (void **)&ze);
	if (index < 0) {
		PMD_DRV_LOG(DEBUG, "Data NOT found in the ct_zone table");
		return NULL;
	}

	return ze;
}

static bool
nfp_ct_zone_table_add(struct nfp_flow_priv *priv,
		struct nfp_ct_zone_entry *ze)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(ze, sizeof(uint32_t), priv->hash_seed);
	ret = rte_hash_add_key_data(priv->ct_zone_table, &hash_key, ze);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add to the ct_zone table failed");
		return false;
	}

	return true;
}

static void
nfp_ct_zone_table_delete(struct nfp_flow_priv *priv,
		struct nfp_ct_zone_entry *ze)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(ze, sizeof(uint32_t), priv->hash_seed);
	ret = rte_hash_del_key(priv->ct_zone_table, &hash_key);
	if (ret < 0)
		PMD_DRV_LOG(ERR, "Delete from the ct_zone table failed");
}

static bool
nfp_ct_zone_entry_init(struct nfp_ct_zone_entry *ze,
		struct nfp_flow_priv *priv,
		uint32_t zone,
		bool wildcard)
{
	char hash_name[RTE_HASH_NAMESIZE];
	struct rte_hash_parameters ct_merge_hash_params = {
		.entries    = 1000,
		.hash_func  = rte_jhash,
		.socket_id  = rte_socket_id(),
		.key_len    = sizeof(uint32_t),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY,
	};

	if (wildcard) {
		ct_merge_hash_params.name = "ct_wc_merge_table";
	} else {
		snprintf(hash_name, sizeof(hash_name), "ct_%d_merge_table", ze->zone);
		ct_merge_hash_params.name = hash_name;
	}

	ct_merge_hash_params.hash_func_init_val = priv->hash_seed;
	ze->ct_merge_table = rte_hash_create(&ct_merge_hash_params);
	if (ze->ct_merge_table == NULL) {
		PMD_DRV_LOG(ERR, "ct merge table creation failed");
		return false;
	}

	ze->zone = zone;
	ze->priv = priv;
	LIST_INIT(&ze->pre_ct_list);
	LIST_INIT(&ze->post_ct_list);

	return true;
}

static void
nfp_ct_zone_entry_destroy(struct nfp_ct_zone_entry *ze)
{
	struct nfp_ct_flow_entry *fe;

	if (ze == NULL)
		return;

	rte_hash_free(ze->ct_merge_table);

	LIST_FOREACH(fe, &ze->pre_ct_list, pre_ct_list)
		nfp_ct_flow_entry_destroy(fe);

	LIST_FOREACH(fe, &ze->post_ct_list, post_ct_list)
		nfp_ct_flow_entry_destroy(fe);

	rte_free(ze);
}

static struct nfp_ct_zone_entry *
nfp_ct_zone_entry_get(struct nfp_flow_priv *priv,
		uint32_t zone,
		bool wildcard)
{
	bool is_ok;
	struct nfp_ct_zone_entry *ze;

	if (wildcard) {
		if (priv->ct_zone_wc != NULL)
			return priv->ct_zone_wc;

		ze = rte_zmalloc("ct_zone_wc", sizeof(*ze), 0);
		if (ze == NULL) {
			PMD_DRV_LOG(ERR, "Could not alloc ct_zone_wc entry");
			return NULL;
		}

		is_ok = nfp_ct_zone_entry_init(ze, priv, zone, true);
		if (!is_ok) {
			PMD_DRV_LOG(ERR, "Init ct zone wc entry failed");
			goto free_ct_zone_entry;
		}

		priv->ct_zone_wc = ze;
	} else {
		ze = nfp_ct_zone_table_search(priv, (char *)&zone, sizeof(uint32_t));
		if (ze != NULL)
			return ze;

		ze = rte_zmalloc("ct_zone_entry", sizeof(*ze), 0);
		if (ze == NULL) {
			PMD_DRV_LOG(ERR, "Could not alloc ct_zone entry");
			return NULL;
		}

		is_ok = nfp_ct_zone_entry_init(ze, priv, zone, false);
		if (!is_ok) {
			PMD_DRV_LOG(ERR, "Init ct zone entry failed");
			goto free_ct_zone_entry;
		}

		is_ok = nfp_ct_zone_table_add(priv, ze);
		if (!is_ok) {
			PMD_DRV_LOG(ERR, "Add into ct zone table failed");
			goto free_ct_zone_entry;
		}
	}

	return ze;

free_ct_zone_entry:
	nfp_ct_zone_entry_destroy(ze);

	return NULL;
}

static void
nfp_ct_zone_entry_free(struct nfp_ct_zone_entry *ze,
		bool wildcard)
{
	if (LIST_EMPTY(&ze->pre_ct_list) && LIST_EMPTY(&ze->post_ct_list)) {
		if (!wildcard)
			nfp_ct_zone_table_delete(ze->priv, ze);

		nfp_ct_zone_entry_destroy(ze);
	}
}

static int
nfp_ct_offload_add(struct nfp_flower_representor *repr,
		struct nfp_ct_merge_entry *merge_entry)
{
	int ret;
	uint64_t cookie;
	struct rte_flow *nfp_flow;
	struct nfp_flow_priv *priv;
	const struct rte_flow_item *items;
	const struct rte_flow_action *actions;

	cookie = rte_rand();
	items = merge_entry->rule.items;
	actions = merge_entry->rule.actions;
	nfp_flow = nfp_flow_process(repr, items, actions, false, cookie, true, true);
	if (nfp_flow == NULL) {
		PMD_DRV_LOG(ERR, "Process the merged flow rule failed.");
		return -EINVAL;
	}

	merge_entry->ctx_id = rte_be_to_cpu_32(nfp_flow->payload.meta->host_ctx_id);

	/* Add the flow to hardware */
	priv = repr->app_fw_flower->flow_priv;
	ret = nfp_flower_cmsg_flow_add(repr->app_fw_flower, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add the merged flow to firmware failed.");
		goto flow_teardown;
	}

	/* Add the flow to flow hash table */
	ret = nfp_flow_table_add_merge(priv, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add the merged flow to flow table failed.");
		goto flow_teardown;
	}

	merge_entry->compiled_rule = nfp_flow;

	return 0;

flow_teardown:
	nfp_flow_teardown(priv, nfp_flow, false);
	nfp_flow_free(nfp_flow);

	return ret;
}

int
nfp_ct_offload_del(struct rte_eth_dev *dev,
		struct nfp_ct_map_entry *me,
		struct rte_flow_error *error)
{
	int ret;
	struct nfp_ct_flow_entry *fe;
	struct nfp_ct_merge_entry *m_ent;

	fe = me->fe;

	if (fe->type == CT_TYPE_PRE_CT) {
		LIST_FOREACH(m_ent, &fe->children, pre_ct_list) {
			if (m_ent->compiled_rule != NULL) {
				ret = nfp_flow_destroy(dev, m_ent->compiled_rule, error);
				if (ret != 0) {
					PMD_DRV_LOG(ERR, "Could not alloc ct_flow_item");
					return -EINVAL;
				}
				m_ent->compiled_rule = NULL;
			}

			m_ent->pre_ct_parent = NULL;
			LIST_REMOVE(m_ent, pre_ct_list);
			if (m_ent->post_ct_parent == NULL)
				nfp_ct_merge_entry_destroy(m_ent);
		}
	} else {
		LIST_FOREACH(m_ent, &fe->children, post_ct_list) {
			if (m_ent->compiled_rule != NULL) {
				ret = nfp_flow_destroy(dev, m_ent->compiled_rule, error);
				if (ret != 0) {
					PMD_DRV_LOG(ERR, "Could not alloc ct_flow_item");
					return -EINVAL;
				}
				m_ent->compiled_rule = NULL;
			}

			m_ent->post_ct_parent = NULL;
			LIST_REMOVE(m_ent, post_ct_list);
			if (m_ent->pre_ct_parent == NULL)
				nfp_ct_merge_entry_destroy(m_ent);
		}
	}

	nfp_ct_flow_entry_destroy_partly(fe);

	return 0;
}

static inline bool
is_item_check_pass(const struct rte_flow_item *item1,
		const struct rte_flow_item *item2,
		uint8_t *cnt_same)
{
	bool pass;
	uint32_t i;
	size_t size;
	const char *key1 = item1->spec;
	const char *key2 = item2->spec;
	const char *mask1 = item1->mask;
	const char *mask2 = item2->mask;

	if (item1->type != item2->type)
		return true;

	pass = nfp_flow_item_conf_size_get(item1->type, &size);
	if (!pass)
		return false;

	for (i = 0; i < size; i++) {
		if ((key1[i] & mask1[i] & mask2[i]) ^ (key2[i] & mask1[i] & mask2[i]))
			return false;
	}

	*cnt_same = *cnt_same + 1;

	return true;
}

static bool
nfp_ct_merge_items_check(struct rte_flow_item *items1,
		struct rte_flow_item *items2,
		uint8_t *cnt_same)
{
	bool pass;
	bool is_tun_flow_1;
	bool is_tun_flow_2;
	const struct rte_flow_item *item1;
	const struct rte_flow_item *item2;
	const struct rte_flow_item *inner_item1 = NULL;
	const struct rte_flow_item *inner_item2 = NULL;

	is_tun_flow_1 = nfp_flow_inner_item_get(items1, &inner_item1);
	is_tun_flow_2 = nfp_flow_inner_item_get(items2, &inner_item2);

	if (is_tun_flow_1) {
		if (is_tun_flow_2) {
			/* Outer layer */
			for (item1 = items1; item1 != inner_item1; item1++) {
				for (item2 = items2; item2 != inner_item2; item2++) {
					pass = is_item_check_pass(item1, item2, cnt_same);
					if (!pass)
						return false;
				}
			}
			/* Inner layer */
			for (item1 = inner_item1; item1->type != RTE_FLOW_ITEM_TYPE_END; item1++) {
				for (item2 = inner_item2; item2->type != RTE_FLOW_ITEM_TYPE_END;
						item2++) {
					pass = is_item_check_pass(item1, item2, cnt_same);
					if (!pass)
						return false;
				}
			}
		} else {
			for (item1 = items1; item1 != inner_item1; item1++) {
				for (item2 = items2; item2->type != RTE_FLOW_ITEM_TYPE_END;
						item2++) {
					pass = is_item_check_pass(item1, item2, cnt_same);
					if (!pass)
						return false;
				}
			}
		}
	} else {
		if (is_tun_flow_2) {
			for (item1 = items1; item1->type != RTE_FLOW_ITEM_TYPE_END; item1++) {
				for (item2 = items2; item2 != inner_item2; item2++) {
					pass = is_item_check_pass(item1, item2, cnt_same);
					if (!pass)
						return false;
				}
			}
		} else {
			for (item1 = items1; item1->type != RTE_FLOW_ITEM_TYPE_END; item1++) {
				for (item2 = items2; item2->type != RTE_FLOW_ITEM_TYPE_END;
						item2++) {
					pass = is_item_check_pass(item1, item2, cnt_same);
					if (!pass)
						return false;
				}
			}
		}
	}

	return true;
}

static inline bool
is_action_pattern_check_pass(struct rte_flow_item *items,
		enum rte_flow_item_type type)
{
	struct rte_flow_item *item;

	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == type)
			return false;
	}

	return true;
}

static bool
nfp_ct_merge_action_check(struct rte_flow_action *action,
		struct rte_flow_item *items)
{
	bool pass = true;

	switch (action->type) {
	case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:    /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
		pass = is_action_pattern_check_pass(items, RTE_FLOW_ITEM_TYPE_ETH);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:
		pass = is_action_pattern_check_pass(items, RTE_FLOW_ITEM_TYPE_IPV4);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:   /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
		pass = is_action_pattern_check_pass(items, RTE_FLOW_ITEM_TYPE_IPV6);
		break;
	case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:    /* FALLTHROUGH */
	case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
		pass = is_action_pattern_check_pass(items, RTE_FLOW_ITEM_TYPE_UDP);
		pass |= is_action_pattern_check_pass(items, RTE_FLOW_ITEM_TYPE_TCP);
		pass |= is_action_pattern_check_pass(items, RTE_FLOW_ITEM_TYPE_SCTP);
		break;
	default:
		break;
	}

	return pass;
}

static bool
nfp_ct_merge_actions_check(struct rte_flow_action *actions,
		struct rte_flow_item *items,
		uint8_t *cnt_same)
{
	bool pass = true;
	struct rte_flow_action *action;

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:    /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:    /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:   /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:   /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:  /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:   /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:   /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:  /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:     /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			pass = nfp_ct_merge_action_check(action, items);
			break;
		case RTE_FLOW_ACTION_TYPE_CONNTRACK: /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_JUMP:      /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_COUNT:     /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_DROP:      /* FALLTHROUGH */
		case RTE_FLOW_ACTION_TYPE_VOID:
			*cnt_same = *cnt_same + 1;
			break;
		default:
			pass = false;
			break;
		}
	}

	return pass;
}

static void
nfp_ct_merge_item_real(const struct rte_flow_item *item_src,
		struct rte_flow_item *item_dst)
{
	uint32_t i;
	char *key_dst;
	char *mask_dst;
	size_t size = 0;
	const char *key_src;
	const char *mask_src;

	key_src = item_src->spec;
	mask_src = item_src->mask;
	key_dst = (char *)(ptrdiff_t)item_dst->spec;
	mask_dst = (char *)(ptrdiff_t)item_dst->mask;
	nfp_flow_item_conf_size_get(item_src->type, &size);

	for (i = 0; i < size; i++) {
		key_dst[i] |= key_src[i];
		mask_dst[i] |= mask_src[i];
	}
}

static bool
nfp_ct_merge_item(uint32_t index,
		const struct rte_flow_item *item1,
		const struct rte_flow_item *item2_start,
		const struct rte_flow_item *item2_end,
		struct nfp_ct_merge_entry *merge_entry)
{
	struct rte_flow_item *item;
	const struct rte_flow_item *item2;

	/* Copy to the merged items */
	item = &merge_entry->rule.items[index];
	*item = *item1;

	item2 = item2_start;
	if (item2_end != NULL) {
		for (; item2 != item2_end; item2++) {
			if (item1->type == item2->type) {
				nfp_ct_merge_item_real(item2, item);
				return true;
			}
		}
	} else {
		for (; item2->type != RTE_FLOW_ITEM_TYPE_END; item2++) {
			if (item1->type == item2->type) {
				nfp_ct_merge_item_real(item2, item);
				return true;
			}
		}
	}

	return false;
}

static void
nfp_ct_merge_items(struct nfp_ct_merge_entry *merge_entry)
{
	uint32_t index = 0;
	bool is_tun_flow_1;
	bool is_tun_flow_2;
	struct rte_flow_item *items1;
	struct rte_flow_item *items2;
	struct rte_flow_item *merge_item;
	const struct rte_flow_item *item;
	const struct rte_flow_item *inner1 = NULL;
	const struct rte_flow_item *inner2 = NULL;

	items1 = merge_entry->pre_ct_parent->rule.items;
	items2 = merge_entry->post_ct_parent->rule.items;
	is_tun_flow_1 = nfp_flow_inner_item_get(items1, &inner1);
	is_tun_flow_2 = nfp_flow_inner_item_get(items2, &inner2);

	if (is_tun_flow_1) {
		if (is_tun_flow_2) {
			/* Outer layer */
			for (item = items1; item != inner1; item++, index++) {
				if (nfp_ct_merge_item(index, item, items2, inner2, merge_entry))
					items2++;
			}

			/* Copy the remainning outer layer items */
			for (item = items2; item != inner2; item++, index++) {
				merge_item = &merge_entry->rule.items[index];
				*merge_item = *item;
			}

			/* Inner layer */
			for (item = inner1; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				if (nfp_ct_merge_item(index, item, inner2, NULL, merge_entry))
					items2++;
			}

			/* Copy the remainning inner layer items */
			for (item = items2; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				merge_item = &merge_entry->rule.items[index];
				*merge_item = *item;
			}
		} else {
			for (item = items1; item != inner1; item++, index++) {
				if (nfp_ct_merge_item(index, item, items2, NULL, merge_entry))
					items2++;
			}

			/* Copy the remainning items */
			for (item = items2; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				merge_item = &merge_entry->rule.items[index];
				*merge_item = *item;
			}

			/* Copy the inner layer items */
			for (item = inner1; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				merge_item = &merge_entry->rule.items[index];
				*merge_item = *item;
			}
		}
	} else {
		if (is_tun_flow_2) {
			for (item = items1; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				if (nfp_ct_merge_item(index, item, items2, inner2, merge_entry))
					items2++;
			}

			/* Copy the remainning items */
			for (item = items2; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				merge_item = &merge_entry->rule.items[index];
				*merge_item = *item;
			}
		} else {
			for (item = items1; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				if (nfp_ct_merge_item(index, item, items2, NULL, merge_entry))
					items2++;
			}

			/* Copy the remainning items */
			for (item = items2; item->type != RTE_FLOW_ITEM_TYPE_END; item++, index++) {
				merge_item = &merge_entry->rule.items[index];
				*merge_item = *item;
			}
		}
	}
}

static void
nfp_ct_merge_actions(struct nfp_ct_merge_entry *merge_entry)
{
	struct rte_flow_action *action;
	struct rte_flow_action *merge_actions;

	merge_actions = merge_entry->rule.actions;

	action = merge_entry->pre_ct_parent->rule.actions;
	for (; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		if (action->type == RTE_FLOW_ACTION_TYPE_CONNTRACK ||
				action->type == RTE_FLOW_ACTION_TYPE_JUMP)
			continue;

		*merge_actions = *action;
		merge_actions++;
	}

	action = merge_entry->post_ct_parent->rule.actions;
	for (; action->type != RTE_FLOW_ACTION_TYPE_END; action++) {
		*merge_actions = *action;
		merge_actions++;
	}
}

static bool
nfp_ct_do_flow_merge(struct nfp_ct_zone_entry *ze,
		struct nfp_ct_flow_entry *pre_ct_entry,
		struct nfp_ct_flow_entry *post_ct_entry)
{
	bool ret;
	uint64_t new_cookie[2];
	uint8_t cnt_same_item = 0;
	uint8_t cnt_same_action = 0;
	struct nfp_ct_merge_entry *merge_entry;

	if (pre_ct_entry->repr != post_ct_entry->repr)
		return true;

	ret = nfp_ct_merge_items_check(pre_ct_entry->rule.items,
			post_ct_entry->rule.items, &cnt_same_item);
	if (!ret)
		return true;

	ret = nfp_ct_merge_actions_check(pre_ct_entry->rule.actions,
			post_ct_entry->rule.items, &cnt_same_action);
	if (!ret)
		return true;

	new_cookie[0] = pre_ct_entry->cookie;
	new_cookie[1] = post_ct_entry->cookie;
	merge_entry = nfp_ct_merge_table_search(ze, (char *)&new_cookie, sizeof(uint64_t) * 2);
	if (merge_entry != NULL)
		return true;

	merge_entry = rte_zmalloc("ct_merge_entry", sizeof(*merge_entry), 0);
	if (merge_entry == NULL) {
		PMD_DRV_LOG(ERR, "Malloc memory for ct merge entry failed");
		return false;
	}

	merge_entry->ze = ze;
	merge_entry->pre_ct_parent = pre_ct_entry;
	merge_entry->post_ct_parent = post_ct_entry;
	rte_memcpy(merge_entry->cookie, new_cookie, sizeof(new_cookie));
	merge_entry->rule.items_cnt = pre_ct_entry->rule.items_cnt +
			post_ct_entry->rule.items_cnt - cnt_same_item - 1;
	merge_entry->rule.actions_cnt = pre_ct_entry->rule.actions_cnt +
			post_ct_entry->rule.actions_cnt - cnt_same_action - 1;

	merge_entry->rule.items = rte_zmalloc("ct_flow_item",
			sizeof(struct rte_flow_item) * merge_entry->rule.items_cnt, 0);
	if (merge_entry->rule.items == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc items for merged flow");
		goto merge_exit;
	}

	merge_entry->rule.actions = rte_zmalloc("ct_flow_action",
			sizeof(struct rte_flow_action) * merge_entry->rule.actions_cnt, 0);
	if (merge_entry->rule.actions == NULL) {
		PMD_DRV_LOG(ERR, "Could not alloc actions for merged flow");
		goto free_items;
	}

	nfp_ct_merge_items(merge_entry);
	nfp_ct_merge_actions(merge_entry);

	/* Add this entry to the pre_ct and post_ct lists */
	LIST_INSERT_HEAD(&pre_ct_entry->children, merge_entry, pre_ct_list);
	LIST_INSERT_HEAD(&post_ct_entry->children, merge_entry, post_ct_list);

	ret = nfp_ct_merge_table_add(ze, merge_entry);
	if (!ret) {
		PMD_DRV_LOG(ERR, "Add into ct merge table failed");
		goto free_actions;
	}

	/* Send to firmware */
	ret = nfp_ct_offload_add(pre_ct_entry->repr, merge_entry);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Send the merged flow to firmware failed");
		goto merge_table_del;
	}

	return true;

merge_table_del:
	nfp_ct_merge_table_delete(ze, merge_entry);
free_actions:
	rte_free(merge_entry->rule.actions);
free_items:
	rte_free(merge_entry->rule.items);
merge_exit:
	LIST_REMOVE(merge_entry, post_ct_list);
	LIST_REMOVE(merge_entry, pre_ct_list);
	rte_free(merge_entry);

	return ret;
}

static bool
nfp_ct_merge_flow_entries(struct nfp_ct_flow_entry *fe,
		struct nfp_ct_zone_entry *ze_src,
		struct nfp_ct_zone_entry *ze_dst)
{
	bool ret;
	struct nfp_ct_flow_entry *fe_tmp;

	if (fe->type == CT_TYPE_PRE_CT) {
		LIST_FOREACH(fe_tmp, &ze_src->post_ct_list, post_ct_list) {
			ret = nfp_ct_do_flow_merge(ze_dst, fe, fe_tmp);
			if (!ret) {
				PMD_DRV_LOG(ERR, "Merge for ct pre flow failed");
				return false;
			}
		}
	} else {
		LIST_FOREACH(fe_tmp, &ze_src->pre_ct_list, pre_ct_list) {
			ret = nfp_ct_do_flow_merge(ze_dst, fe_tmp, fe);
			if (!ret) {
				PMD_DRV_LOG(ERR, "Merge for ct post flow failed");
				return false;
			}
		}
	}

	return true;
}

static bool
nfp_flow_handle_pre_ct(const struct rte_flow_item *ct_item,
		struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		uint64_t cookie)
{
	bool ret;
	struct nfp_flow_priv *priv;
	struct nfp_ct_zone_entry *ze;
	struct nfp_ct_flow_entry *fe;
	const struct ct_data *ct = ct_item->spec;

	priv = representor->app_fw_flower->flow_priv;
	ze = nfp_ct_zone_entry_get(priv, ct->ct_zone, false);
	if (ze == NULL) {
		PMD_DRV_LOG(ERR, "Could not get ct zone entry");
		return false;
	}

	/* Add entry to pre_ct_list */
	fe = nfp_ct_flow_entry_get(ze, representor, items, actions, cookie);
	if (fe == NULL) {
		PMD_DRV_LOG(ERR, "Could not get ct flow entry");
		goto ct_zone_entry_free;
	}

	fe->type = CT_TYPE_PRE_CT;
	LIST_INSERT_HEAD(&ze->pre_ct_list, fe, pre_ct_list);

	ret = nfp_ct_merge_flow_entries(fe, ze, ze);
	if (!ret) {
		PMD_DRV_LOG(ERR, "Merge ct flow entries failed");
		goto ct_flow_entry_free;
	}

	/* Need to check and merge with tables in the wc_zone as well */
	if (priv->ct_zone_wc != NULL) {
		ret = nfp_ct_merge_flow_entries(fe, priv->ct_zone_wc, ze);
		if (!ret) {
			PMD_DRV_LOG(ERR, "Merge ct flow entries wildcast failed");
			goto ct_flow_entry_free;
		}
	}

	return true;

ct_flow_entry_free:
	nfp_ct_flow_entry_destroy(fe);

ct_zone_entry_free:
	nfp_ct_zone_entry_free(ze, false);

	return false;
}

static bool
nfp_flow_handle_post_ct(const struct rte_flow_item *ct_item,
		struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		uint64_t cookie)
{
	bool ret;
	void *next_data;
	uint32_t iter = 0;
	const void *next_key;
	bool wildcard = false;
	struct nfp_flow_priv *priv;
	struct nfp_ct_zone_entry *ze;
	struct nfp_ct_flow_entry *fe;
	const struct ct_data *ct = ct_item->spec;
	const struct ct_data *ct_mask = ct_item->mask;

	if (ct_mask->ct_zone == 0) {
		wildcard = true;
	} else if (ct_mask->ct_zone != UINT16_MAX) {
		PMD_DRV_LOG(ERR, "Partially wildcard ct_zone is not supported");
		return false;
	}

	priv = representor->app_fw_flower->flow_priv;
	ze = nfp_ct_zone_entry_get(priv, ct->ct_zone, wildcard);
	if (ze == NULL) {
		PMD_DRV_LOG(ERR, "Could not get ct zone entry");
		return false;
	}

	/* Add entry to post_ct_list */
	fe = nfp_ct_flow_entry_get(ze, representor, items, actions, cookie);
	if (fe == NULL) {
		PMD_DRV_LOG(ERR, "Could not get ct flow entry");
		goto ct_zone_entry_free;
	}

	fe->type = CT_TYPE_POST_CT;
	LIST_INSERT_HEAD(&ze->post_ct_list, fe, post_ct_list);

	if (wildcard) {
		while (rte_hash_iterate(priv->ct_zone_table, &next_key, &next_data, &iter) >= 0) {
			ze = (struct nfp_ct_zone_entry *)next_data;
			ret = nfp_ct_merge_flow_entries(fe, ze, ze);
			if (!ret) {
				PMD_DRV_LOG(ERR, "Merge ct flow entries wildcast failed");
				break;
			}
		}
	} else {
		ret = nfp_ct_merge_flow_entries(fe, ze, ze);
	}

	if (!ret)
		goto ct_flow_entry_free;

	return true;

ct_flow_entry_free:
	nfp_ct_flow_entry_destroy(fe);

ct_zone_entry_free:
	nfp_ct_zone_entry_free(ze, wildcard);

	return false;
}

struct rte_flow *
nfp_ct_flow_setup(struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		const struct rte_flow_item *ct_item,
		bool validate_flag,
		uint64_t cookie)
{
	const struct ct_data *ct;

	if (ct_item == NULL)
		return NULL;

	ct = ct_item->spec;

	if (is_ct_commit_flow(ct)) {
		return nfp_flow_process(representor, &items[1], actions,
				validate_flag, cookie, false, false);
	}

	if (is_post_ct_flow(ct)) {
		if (nfp_flow_handle_post_ct(ct_item, representor, &items[1],
				actions, cookie)) {
			return nfp_flow_process(representor, &items[1], actions,
					validate_flag, cookie, false, false);
		}

		PMD_DRV_LOG(ERR, "Handle nfp post ct flow failed.");
		return NULL;
	}

	if (is_pre_ct_flow(ct, actions)) {
		if (nfp_flow_handle_pre_ct(ct_item, representor, &items[1],
				actions, cookie)) {
			return nfp_flow_process(representor, &items[1], actions,
					validate_flag, cookie, false, false);
		}

		PMD_DRV_LOG(ERR, "Handle nfp pre ct flow failed.");
		return NULL;
	}

	PMD_DRV_LOG(ERR, "Unsupported ct flow type.");
	return NULL;
}

static inline void
nfp_ct_flow_stats_update(struct nfp_flow_priv *priv,
		struct nfp_ct_merge_entry *m_ent)
{
	uint32_t ctx_id;
	struct nfp_fl_stats *merge_stats;

	ctx_id = m_ent->ctx_id;
	merge_stats = &priv->stats[ctx_id];

	m_ent->pre_ct_parent->stats.bytes  += merge_stats->bytes;
	m_ent->pre_ct_parent->stats.pkts   += merge_stats->pkts;
	m_ent->post_ct_parent->stats.bytes += merge_stats->bytes;
	m_ent->post_ct_parent->stats.pkts  += merge_stats->pkts;

	merge_stats->bytes = 0;
	merge_stats->pkts = 0;
}

struct nfp_fl_stats *
nfp_ct_flow_stats_get(struct nfp_flow_priv *priv,
		struct nfp_ct_map_entry *me)
{
	struct nfp_ct_merge_entry *m_ent;

	rte_spinlock_lock(&priv->stats_lock);

	if (me->fe->type == CT_TYPE_PRE_CT) {
		LIST_FOREACH(m_ent, &me->fe->children, pre_ct_list)
			nfp_ct_flow_stats_update(priv, m_ent);
	} else {
		LIST_FOREACH(m_ent, &me->fe->children, post_ct_list)
			nfp_ct_flow_stats_update(priv, m_ent);
	}

	rte_spinlock_unlock(&priv->stats_lock);

	return &me->fe->stats;
}
