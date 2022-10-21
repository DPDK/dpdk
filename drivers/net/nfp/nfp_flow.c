/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include <rte_flow_driver.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <bus_pci_driver.h>

#include "nfp_common.h"
#include "nfp_flow.h"
#include "nfp_logs.h"
#include "flower/nfp_flower.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_rtsym.h"

struct nfp_mask_id_entry {
	uint32_t hash_key;
	uint32_t ref_cnt;
	uint8_t mask_id;
};

static int
nfp_mask_id_alloc(struct nfp_flow_priv *priv, uint8_t *mask_id)
{
	uint8_t temp_id;
	uint8_t freed_id;
	struct circ_buf *ring;

	/* Checking for unallocated entries first. */
	if (priv->mask_ids.init_unallocated > 0) {
		*mask_id = priv->mask_ids.init_unallocated;
		priv->mask_ids.init_unallocated--;
		return 0;
	}

	/* Checking if buffer is empty. */
	freed_id = NFP_FLOWER_MASK_ENTRY_RS - 1;
	ring = &priv->mask_ids.free_list;
	if (ring->head == ring->tail) {
		*mask_id = freed_id;
		return -ENOENT;
	}

	rte_memcpy(&temp_id, &ring->buf[ring->tail], NFP_FLOWER_MASK_ELEMENT_RS);
	*mask_id = temp_id;

	rte_memcpy(&ring->buf[ring->tail], &freed_id, NFP_FLOWER_MASK_ELEMENT_RS);
	ring->tail = (ring->tail + NFP_FLOWER_MASK_ELEMENT_RS) %
			(NFP_FLOWER_MASK_ENTRY_RS * NFP_FLOWER_MASK_ELEMENT_RS);

	return 0;
}

static int
nfp_mask_id_free(struct nfp_flow_priv *priv, uint8_t mask_id)
{
	struct circ_buf *ring;

	ring = &priv->mask_ids.free_list;

	/* Checking if buffer is full. */
	if (CIRC_SPACE(ring->head, ring->tail, NFP_FLOWER_MASK_ENTRY_RS) == 0)
		return -ENOBUFS;

	rte_memcpy(&ring->buf[ring->head], &mask_id, NFP_FLOWER_MASK_ELEMENT_RS);
	ring->head = (ring->head + NFP_FLOWER_MASK_ELEMENT_RS) %
			(NFP_FLOWER_MASK_ENTRY_RS * NFP_FLOWER_MASK_ELEMENT_RS);

	return 0;
}

static int
nfp_mask_table_add(struct nfp_flow_priv *priv,
		char *mask_data,
		uint32_t mask_len,
		uint8_t *id)
{
	int ret;
	uint8_t mask_id;
	uint32_t hash_key;
	struct nfp_mask_id_entry *mask_entry;

	mask_entry = rte_zmalloc("mask_entry", sizeof(struct nfp_mask_id_entry), 0);
	if (mask_entry == NULL) {
		ret = -ENOMEM;
		goto exit;
	}

	ret = nfp_mask_id_alloc(priv, &mask_id);
	if (ret != 0)
		goto mask_entry_free;

	hash_key = rte_jhash(mask_data, mask_len, priv->hash_seed);
	mask_entry->mask_id  = mask_id;
	mask_entry->hash_key = hash_key;
	mask_entry->ref_cnt  = 1;
	PMD_DRV_LOG(DEBUG, "hash_key=%#x id=%u ref=%u", hash_key,
			mask_id, mask_entry->ref_cnt);

	ret = rte_hash_add_key_data(priv->mask_table, &hash_key, mask_entry);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add to mask table failed.");
		goto mask_id_free;
	}

	*id = mask_id;
	return 0;

mask_id_free:
	nfp_mask_id_free(priv, mask_id);
mask_entry_free:
	rte_free(mask_entry);
exit:
	return ret;
}

static int
nfp_mask_table_del(struct nfp_flow_priv *priv,
		char *mask_data,
		uint32_t mask_len,
		uint8_t id)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(mask_data, mask_len, priv->hash_seed);
	ret = rte_hash_del_key(priv->mask_table, &hash_key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Delete from mask table failed.");
		return ret;
	}

	ret = nfp_mask_id_free(priv, id);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Free mask id failed.");
		return ret;
	}

	return 0;
}

static struct nfp_mask_id_entry *
nfp_mask_table_search(struct nfp_flow_priv *priv,
		char *mask_data,
		uint32_t mask_len)
{
	int index;
	uint32_t hash_key;
	struct nfp_mask_id_entry *entry;

	hash_key = rte_jhash(mask_data, mask_len, priv->hash_seed);
	index = rte_hash_lookup_data(priv->mask_table, &hash_key, (void **)&entry);
	if (index < 0) {
		PMD_DRV_LOG(DEBUG, "Data NOT found in the mask table.");
		return NULL;
	}

	return entry;
}

__rte_unused static bool
nfp_check_mask_add(struct nfp_flow_priv *priv,
		char *mask_data,
		uint32_t mask_len,
		uint8_t *meta_flags,
		uint8_t *mask_id)
{
	int ret;
	struct nfp_mask_id_entry *mask_entry;

	mask_entry = nfp_mask_table_search(priv, mask_data, mask_len);
	if (mask_entry == NULL) {
		/* mask entry does not exist, let's create one */
		ret = nfp_mask_table_add(priv, mask_data, mask_len, mask_id);
		if (ret != 0)
			return false;

		*meta_flags |= NFP_FL_META_FLAG_MANAGE_MASK;
	} else {
		/* mask entry already exist */
		mask_entry->ref_cnt++;
		*mask_id = mask_entry->mask_id;
	}

	return true;
}

__rte_unused static bool
nfp_check_mask_remove(struct nfp_flow_priv *priv,
		char *mask_data,
		uint32_t mask_len,
		uint8_t *meta_flags)
{
	int ret;
	struct nfp_mask_id_entry *mask_entry;

	mask_entry = nfp_mask_table_search(priv, mask_data, mask_len);
	if (mask_entry == NULL)
		return false;

	mask_entry->ref_cnt--;
	if (mask_entry->ref_cnt == 0) {
		ret = nfp_mask_table_del(priv, mask_data, mask_len,
				mask_entry->mask_id);
		if (ret != 0)
			return false;

		rte_free(mask_entry);
		if (meta_flags)
			*meta_flags &= ~NFP_FL_META_FLAG_MANAGE_MASK;
	}

	return true;
}

__rte_unused static int
nfp_flow_table_add(struct nfp_flow_priv *priv,
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
nfp_flow_table_delete(struct nfp_flow_priv *priv,
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
nfp_flow_table_search(struct nfp_flow_priv *priv,
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
nfp_flow_alloc(struct nfp_fl_key_ls *key_layer)
{
	char *tmp;
	size_t len;
	struct rte_flow *nfp_flow;
	struct nfp_fl_payload *payload;

	nfp_flow = rte_zmalloc("nfp_flow", sizeof(struct rte_flow), 0);
	if (nfp_flow == NULL)
		goto exit;

	len = key_layer->key_size + key_layer->key_size + key_layer->act_size;
	tmp = rte_zmalloc("nfp_flow_payload", len + sizeof(struct nfp_fl_rule_metadata), 0);
	if (tmp == NULL)
		goto free_flow;

	nfp_flow->length = len;

	payload                = &nfp_flow->payload;
	payload->meta          = (struct nfp_fl_rule_metadata *)tmp;
	payload->unmasked_data = tmp + sizeof(struct nfp_fl_rule_metadata);
	payload->mask_data     = payload->unmasked_data + key_layer->key_size;
	payload->action_data   = payload->mask_data + key_layer->key_size;

	return nfp_flow;

free_flow:
	rte_free(nfp_flow);
exit:
	return NULL;
}

__rte_unused static void
nfp_flow_free(struct rte_flow *nfp_flow)
{
	rte_free(nfp_flow->payload.meta);
	rte_free(nfp_flow);
}

__rte_unused static int
nfp_stats_id_alloc(struct nfp_flow_priv *priv, uint32_t *ctx)
{
	struct circ_buf *ring;
	uint32_t temp_stats_id;
	uint32_t freed_stats_id;

	/* Check for unallocated entries first. */
	if (priv->stats_ids.init_unallocated > 0) {
		*ctx = ((priv->stats_ids.init_unallocated - 1) & NFP_FL_STAT_ID_STAT) |
				(priv->active_mem_unit & NFP_FL_STAT_ID_MU_NUM);
		if (++priv->active_mem_unit == priv->total_mem_units) {
			priv->stats_ids.init_unallocated--;
			priv->active_mem_unit = 0;
		}
		return 0;
	}

	/* Check if buffer is empty */
	ring = &priv->stats_ids.free_list;
	freed_stats_id = priv->stats_ring_size;
	if (ring->head == ring->tail) {
		*ctx = freed_stats_id;
		return -ENOENT;
	}

	memcpy(&temp_stats_id, &ring->buf[ring->tail], NFP_FL_STATS_ELEM_RS);
	*ctx = temp_stats_id;
	memcpy(&ring->buf[ring->tail], &freed_stats_id, NFP_FL_STATS_ELEM_RS);
	ring->tail = (ring->tail + NFP_FL_STATS_ELEM_RS) %
			(priv->stats_ring_size * NFP_FL_STATS_ELEM_RS);

	return 0;
}

__rte_unused static int
nfp_stats_id_free(struct nfp_flow_priv *priv, uint32_t ctx)
{
	struct circ_buf *ring;

	/* Check if buffer is full */
	ring = &priv->stats_ids.free_list;
	if (!CIRC_SPACE(ring->head, ring->tail, priv->stats_ring_size *
			NFP_FL_STATS_ELEM_RS - NFP_FL_STATS_ELEM_RS + 1))
		return -ENOBUFS;

	memcpy(&ring->buf[ring->head], &ctx, NFP_FL_STATS_ELEM_RS);
	ring->head = (ring->head + NFP_FL_STATS_ELEM_RS) %
			(priv->stats_ring_size * NFP_FL_STATS_ELEM_RS);

	return 0;
}

int
nfp_flow_priv_init(struct nfp_pf_dev *pf_dev)
{
	int ret = 0;
	size_t stats_size;
	uint64_t ctx_count;
	uint64_t ctx_split;
	struct nfp_flow_priv *priv;
	struct nfp_app_fw_flower *app_fw_flower;

	struct rte_hash_parameters mask_hash_params = {
		.name       = "mask_hash_table",
		.entries    = NFP_MASK_TABLE_ENTRIES,
		.hash_func  = rte_jhash,
		.socket_id  = rte_socket_id(),
		.key_len    = sizeof(uint32_t),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY,
	};

	struct rte_hash_parameters flow_hash_params = {
		.name       = "flow_hash_table",
		.hash_func  = rte_jhash,
		.socket_id  = rte_socket_id(),
		.key_len    = sizeof(uint32_t),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY,
	};

	ctx_count = nfp_rtsym_read_le(pf_dev->sym_tbl,
			"CONFIG_FC_HOST_CTX_COUNT", &ret);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Read CTX_COUNT from symbol table failed");
		goto exit;
	}

	ctx_split = nfp_rtsym_read_le(pf_dev->sym_tbl,
			"CONFIG_FC_HOST_CTX_SPLIT", &ret);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Read CTX_SPLIT from symbol table failed");
		goto exit;
	}

	priv = rte_zmalloc("nfp_app_flow_priv", sizeof(struct nfp_flow_priv), 0);
	if (priv == NULL) {
		PMD_INIT_LOG(ERR, "nfp app flow priv creation failed");
		ret = -ENOMEM;
		goto exit;
	}

	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);
	app_fw_flower->flow_priv = priv;
	priv->hash_seed = (uint32_t)rte_rand();
	priv->stats_ring_size = ctx_count;
	priv->total_mem_units = ctx_split;

	/* Init ring buffer and unallocated mask_ids. */
	priv->mask_ids.init_unallocated = NFP_FLOWER_MASK_ENTRY_RS - 1;
	priv->mask_ids.free_list.buf = rte_zmalloc("nfp_app_mask_ids",
			NFP_FLOWER_MASK_ENTRY_RS * NFP_FLOWER_MASK_ELEMENT_RS, 0);
	if (priv->mask_ids.free_list.buf == NULL) {
		PMD_INIT_LOG(ERR, "mask id free list creation failed");
		ret = -ENOMEM;
		goto free_priv;
	}

	/* Init ring buffer and unallocated stats_ids. */
	priv->stats_ids.init_unallocated = ctx_count / ctx_split;
	priv->stats_ids.free_list.buf = rte_zmalloc("nfp_app_stats_ids",
			priv->stats_ring_size * NFP_FL_STATS_ELEM_RS, 0);
	if (priv->stats_ids.free_list.buf == NULL) {
		PMD_INIT_LOG(ERR, "stats id free list creation failed");
		ret = -ENOMEM;
		goto free_mask_id;
	}

	/* flow stats */
	rte_spinlock_init(&priv->stats_lock);
	stats_size = (ctx_count & NFP_FL_STAT_ID_STAT) |
			((ctx_split - 1) & NFP_FL_STAT_ID_MU_NUM);
	PMD_INIT_LOG(INFO, "ctx_count:%0lx, ctx_split:%0lx, stats_size:%0lx ",
			ctx_count, ctx_split, stats_size);
	priv->stats = rte_zmalloc("nfp_flow_stats",
			stats_size * sizeof(struct nfp_fl_stats), 0);
	if (priv->stats == NULL) {
		PMD_INIT_LOG(ERR, "flow stats creation failed");
		ret = -ENOMEM;
		goto free_stats_id;
	}

	/* mask table */
	mask_hash_params.hash_func_init_val = priv->hash_seed;
	priv->mask_table = rte_hash_create(&mask_hash_params);
	if (priv->mask_table == NULL) {
		PMD_INIT_LOG(ERR, "mask hash table creation failed");
		ret = -ENOMEM;
		goto free_stats;
	}

	/* flow table */
	flow_hash_params.hash_func_init_val = priv->hash_seed;
	flow_hash_params.entries = ctx_count;
	priv->flow_table = rte_hash_create(&flow_hash_params);
	if (priv->flow_table == NULL) {
		PMD_INIT_LOG(ERR, "flow hash table creation failed");
		ret = -ENOMEM;
		goto free_mask_table;
	}

	return 0;

free_mask_table:
	rte_free(priv->mask_table);
free_stats:
	rte_free(priv->stats);
free_stats_id:
	rte_free(priv->stats_ids.free_list.buf);
free_mask_id:
	rte_free(priv->mask_ids.free_list.buf);
free_priv:
	rte_free(priv);
exit:
	return ret;
}

void
nfp_flow_priv_uninit(struct nfp_pf_dev *pf_dev)
{
	struct nfp_flow_priv *priv;
	struct nfp_app_fw_flower *app_fw_flower;

	app_fw_flower = NFP_PRIV_TO_APP_FW_FLOWER(pf_dev->app_fw_priv);
	priv = app_fw_flower->flow_priv;

	rte_hash_free(priv->flow_table);
	rte_hash_free(priv->mask_table);
	rte_free(priv->stats);
	rte_free(priv->stats_ids.free_list.buf);
	rte_free(priv->mask_ids.free_list.buf);
	rte_free(priv);
}
