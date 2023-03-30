/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#include <rte_flow_driver.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <bus_pci_driver.h>
#include <rte_malloc.h>

#include "nfp_common.h"
#include "nfp_ctrl.h"
#include "nfp_flow.h"
#include "nfp_logs.h"
#include "nfp_rxtx.h"
#include "flower/nfp_flower.h"
#include "flower/nfp_flower_cmsg.h"
#include "flower/nfp_flower_ctrl.h"
#include "flower/nfp_flower_representor.h"
#include "nfpcore/nfp_mip.h"
#include "nfpcore/nfp_rtsym.h"

/*
 * Maximum number of items in struct rte_flow_action_vxlan_encap.
 * ETH / IPv4(6) / UDP / VXLAN / END
 */
#define ACTION_VXLAN_ENCAP_ITEMS_NUM 5

struct vxlan_data {
	struct rte_flow_action_vxlan_encap conf;
	struct rte_flow_item items[ACTION_VXLAN_ENCAP_ITEMS_NUM];
};

/* Static initializer for a list of subsequent item types */
#define NEXT_ITEM(...) \
	((const enum rte_flow_item_type []){ \
		__VA_ARGS__, RTE_FLOW_ITEM_TYPE_END, \
	})

/* Data length of various conf of raw encap action */
#define GENEVE_V4_LEN    (sizeof(struct rte_ether_hdr) + \
				sizeof(struct rte_ipv4_hdr) + \
				sizeof(struct rte_udp_hdr) + \
				sizeof(struct rte_flow_item_geneve))
#define GENEVE_V6_LEN    (sizeof(struct rte_ether_hdr) + \
				sizeof(struct rte_ipv6_hdr) + \
				sizeof(struct rte_udp_hdr) + \
				sizeof(struct rte_flow_item_geneve))
#define NVGRE_V4_LEN     (sizeof(struct rte_ether_hdr) + \
				sizeof(struct rte_ipv4_hdr) + \
				sizeof(struct rte_flow_item_gre) + \
				sizeof(rte_be32_t))    /* gre key */
#define NVGRE_V6_LEN     (sizeof(struct rte_ether_hdr) + \
				sizeof(struct rte_ipv6_hdr) + \
				sizeof(struct rte_flow_item_gre) + \
				sizeof(rte_be32_t))    /* gre key */

/* Process structure associated with a flow item */
struct nfp_flow_item_proc {
	/* Bit-mask for fields supported by this PMD. */
	const void *mask_support;
	/* Bit-mask to use when @p item->mask is not provided. */
	const void *mask_default;
	/* Size in bytes for @p mask_support and @p mask_default. */
	const unsigned int mask_sz;
	/* Merge a pattern item into a flow rule handle. */
	int (*merge)(struct nfp_app_fw_flower *app_fw_flower,
			struct rte_flow *nfp_flow,
			char **mbuf_off,
			const struct rte_flow_item *item,
			const struct nfp_flow_item_proc *proc,
			bool is_mask,
			bool is_outer_layer);
	/* List of possible subsequent items. */
	const enum rte_flow_item_type *const next_item;
};

struct nfp_mask_id_entry {
	uint32_t hash_key;
	uint32_t ref_cnt;
	uint8_t mask_id;
};

struct nfp_pre_tun_entry {
	uint16_t mac_index;
	uint16_t ref_cnt;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
} __rte_aligned(32);

static inline struct nfp_flow_priv *
nfp_flow_dev_to_priv(struct rte_eth_dev *dev)
{
	struct nfp_flower_representor *repr;

	repr = (struct nfp_flower_representor *)dev->data->dev_private;
	return repr->app_fw_flower->flow_priv;
}

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

static bool
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

static bool
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
			*meta_flags |= NFP_FL_META_FLAG_MANAGE_MASK;
	}

	return true;
}

static int
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

static int
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

static struct rte_flow *
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

static struct rte_flow *
nfp_flow_alloc(struct nfp_fl_key_ls *key_layer, uint32_t port_id)
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
	nfp_flow->mtr_id       = NFP_MAX_MTR_CNT;
	nfp_flow->port_id      = port_id;
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

static void
nfp_flow_free(struct rte_flow *nfp_flow)
{
	rte_free(nfp_flow->payload.meta);
	rte_free(nfp_flow);
}

static int
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

static int
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

static int
nfp_tun_add_ipv4_off(struct nfp_app_fw_flower *app_fw_flower,
		rte_be32_t ipv4)
{
	struct nfp_flow_priv *priv;
	struct nfp_ipv4_addr_entry *entry;
	struct nfp_ipv4_addr_entry *tmp_entry;

	priv = app_fw_flower->flow_priv;

	rte_spinlock_lock(&priv->ipv4_off_lock);
	LIST_FOREACH(entry, &priv->ipv4_off_list, next) {
		if (entry->ipv4_addr == ipv4) {
			entry->ref_count++;
			rte_spinlock_unlock(&priv->ipv4_off_lock);
			return 0;
		}
	}
	rte_spinlock_unlock(&priv->ipv4_off_lock);

	tmp_entry = rte_zmalloc("nfp_ipv4_off", sizeof(struct nfp_ipv4_addr_entry), 0);
	if (tmp_entry == NULL) {
		PMD_DRV_LOG(ERR, "Mem error when offloading IP address.");
		return -ENOMEM;
	}

	tmp_entry->ipv4_addr = ipv4;
	tmp_entry->ref_count = 1;

	rte_spinlock_lock(&priv->ipv4_off_lock);
	LIST_INSERT_HEAD(&priv->ipv4_off_list, tmp_entry, next);
	rte_spinlock_unlock(&priv->ipv4_off_lock);

	return nfp_flower_cmsg_tun_off_v4(app_fw_flower);
}

static int
nfp_tun_del_ipv4_off(struct nfp_app_fw_flower *app_fw_flower,
		rte_be32_t ipv4)
{
	struct nfp_flow_priv *priv;
	struct nfp_ipv4_addr_entry *entry;

	priv = app_fw_flower->flow_priv;

	rte_spinlock_lock(&priv->ipv4_off_lock);
	LIST_FOREACH(entry, &priv->ipv4_off_list, next) {
		if (entry->ipv4_addr == ipv4) {
			entry->ref_count--;
			if (entry->ref_count == 0) {
				LIST_REMOVE(entry, next);
				rte_free(entry);
				rte_spinlock_unlock(&priv->ipv4_off_lock);
				return nfp_flower_cmsg_tun_off_v4(app_fw_flower);
			}
			break;
		}
	}
	rte_spinlock_unlock(&priv->ipv4_off_lock);

	return 0;
}

static int
nfp_tun_add_ipv6_off(struct nfp_app_fw_flower *app_fw_flower,
		uint8_t ipv6[])
{
	struct nfp_flow_priv *priv;
	struct nfp_ipv6_addr_entry *entry;
	struct nfp_ipv6_addr_entry *tmp_entry;

	priv = app_fw_flower->flow_priv;

	rte_spinlock_lock(&priv->ipv6_off_lock);
	LIST_FOREACH(entry, &priv->ipv6_off_list, next) {
		if (!memcmp(entry->ipv6_addr, ipv6, sizeof(entry->ipv6_addr))) {
			entry->ref_count++;
			rte_spinlock_unlock(&priv->ipv6_off_lock);
			return 0;
		}
	}
	rte_spinlock_unlock(&priv->ipv6_off_lock);

	tmp_entry = rte_zmalloc("nfp_ipv6_off", sizeof(struct nfp_ipv6_addr_entry), 0);
	if (tmp_entry == NULL) {
		PMD_DRV_LOG(ERR, "Mem error when offloading IP6 address.");
		return -ENOMEM;
	}
	memcpy(tmp_entry->ipv6_addr, ipv6, sizeof(tmp_entry->ipv6_addr));
	tmp_entry->ref_count = 1;

	rte_spinlock_lock(&priv->ipv6_off_lock);
	LIST_INSERT_HEAD(&priv->ipv6_off_list, tmp_entry, next);
	rte_spinlock_unlock(&priv->ipv6_off_lock);

	return nfp_flower_cmsg_tun_off_v6(app_fw_flower);
}

static int
nfp_tun_del_ipv6_off(struct nfp_app_fw_flower *app_fw_flower,
		uint8_t ipv6[])
{
	struct nfp_flow_priv *priv;
	struct nfp_ipv6_addr_entry *entry;

	priv = app_fw_flower->flow_priv;

	rte_spinlock_lock(&priv->ipv6_off_lock);
	LIST_FOREACH(entry, &priv->ipv6_off_list, next) {
		if (!memcmp(entry->ipv6_addr, ipv6, sizeof(entry->ipv6_addr))) {
			entry->ref_count--;
			if (entry->ref_count == 0) {
				LIST_REMOVE(entry, next);
				rte_free(entry);
				rte_spinlock_unlock(&priv->ipv6_off_lock);
				return nfp_flower_cmsg_tun_off_v6(app_fw_flower);
			}
			break;
		}
	}
	rte_spinlock_unlock(&priv->ipv6_off_lock);

	return 0;
}

static int
nfp_tun_check_ip_off_del(struct nfp_flower_representor *repr,
		struct rte_flow *nfp_flow)
{
	int ret;
	uint32_t key_layer2 = 0;
	struct nfp_flower_ipv4_udp_tun *udp4;
	struct nfp_flower_ipv6_udp_tun *udp6;
	struct nfp_flower_ipv4_gre_tun *gre4;
	struct nfp_flower_ipv6_gre_tun *gre6;
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_flower_ext_meta *ext_meta = NULL;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_EXT_META)
		ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);

	if (ext_meta != NULL)
		key_layer2 = rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2);

	if (key_layer2 & NFP_FLOWER_LAYER2_TUN_IPV6) {
		if (key_layer2 & NFP_FLOWER_LAYER2_GRE) {
			gre6 = (struct nfp_flower_ipv6_gre_tun *)(nfp_flow->payload.mask_data -
					sizeof(struct nfp_flower_ipv6_gre_tun));
			ret = nfp_tun_del_ipv6_off(repr->app_fw_flower, gre6->ipv6.ipv6_dst);
		} else {
			udp6 = (struct nfp_flower_ipv6_udp_tun *)(nfp_flow->payload.mask_data -
					sizeof(struct nfp_flower_ipv6_udp_tun));
			ret = nfp_tun_del_ipv6_off(repr->app_fw_flower, udp6->ipv6.ipv6_dst);
		}
	} else {
		if (key_layer2 & NFP_FLOWER_LAYER2_GRE) {
			gre4 = (struct nfp_flower_ipv4_gre_tun *)(nfp_flow->payload.mask_data -
					sizeof(struct nfp_flower_ipv4_gre_tun));
			ret = nfp_tun_del_ipv4_off(repr->app_fw_flower, gre4->ipv4.dst);
		} else {
			udp4 = (struct nfp_flower_ipv4_udp_tun *)(nfp_flow->payload.mask_data -
					sizeof(struct nfp_flower_ipv4_udp_tun));
			ret = nfp_tun_del_ipv4_off(repr->app_fw_flower, udp4->ipv4.dst);
		}
	}

	return ret;
}

static void
nfp_flower_compile_meta_tci(char *mbuf_off, struct nfp_fl_key_ls *key_layer)
{
	struct nfp_flower_meta_tci *tci_meta;

	tci_meta = (struct nfp_flower_meta_tci *)mbuf_off;
	tci_meta->nfp_flow_key_layer = key_layer->key_layer;
	tci_meta->mask_id = ~0;
	tci_meta->tci = rte_cpu_to_be_16(key_layer->vlan);
}

static void
nfp_flower_update_meta_tci(char *exact, uint8_t mask_id)
{
	struct nfp_flower_meta_tci *meta_tci;

	meta_tci = (struct nfp_flower_meta_tci *)exact;
	meta_tci->mask_id = mask_id;
}

static void
nfp_flower_compile_ext_meta(char *mbuf_off, struct nfp_fl_key_ls *key_layer)
{
	struct nfp_flower_ext_meta *ext_meta;

	ext_meta = (struct nfp_flower_ext_meta *)mbuf_off;
	ext_meta->nfp_flow_key_layer2 = rte_cpu_to_be_32(key_layer->key_layer_two);
}

static void
nfp_compile_meta_port(char *mbuf_off,
		struct nfp_fl_key_ls *key_layer,
		bool is_mask)
{
	struct nfp_flower_in_port *port_meta;

	port_meta = (struct nfp_flower_in_port *)mbuf_off;

	if (is_mask)
		port_meta->in_port = rte_cpu_to_be_32(~0);
	else if (key_layer->tun_type)
		port_meta->in_port = rte_cpu_to_be_32(NFP_FL_PORT_TYPE_TUN |
				key_layer->tun_type);
	else
		port_meta->in_port = rte_cpu_to_be_32(key_layer->port);
}

static void
nfp_flow_compile_metadata(struct nfp_flow_priv *priv,
		struct rte_flow *nfp_flow,
		struct nfp_fl_key_ls *key_layer,
		uint32_t stats_ctx)
{
	struct nfp_fl_rule_metadata *nfp_flow_meta;
	char *mbuf_off_exact;
	char *mbuf_off_mask;

	/*
	 * Convert to long words as firmware expects
	 * lengths in units of NFP_FL_LW_SIZ.
	 */
	nfp_flow_meta               = nfp_flow->payload.meta;
	nfp_flow_meta->key_len      = key_layer->key_size >> NFP_FL_LW_SIZ;
	nfp_flow_meta->mask_len     = key_layer->key_size >> NFP_FL_LW_SIZ;
	nfp_flow_meta->act_len      = key_layer->act_size >> NFP_FL_LW_SIZ;
	nfp_flow_meta->flags        = 0;
	nfp_flow_meta->host_ctx_id  = rte_cpu_to_be_32(stats_ctx);
	nfp_flow_meta->host_cookie  = rte_rand();
	nfp_flow_meta->flow_version = rte_cpu_to_be_64(priv->flower_version);

	mbuf_off_exact = nfp_flow->payload.unmasked_data;
	mbuf_off_mask  = nfp_flow->payload.mask_data;

	/* Populate Metadata */
	nfp_flower_compile_meta_tci(mbuf_off_exact, key_layer);
	nfp_flower_compile_meta_tci(mbuf_off_mask, key_layer);
	mbuf_off_exact += sizeof(struct nfp_flower_meta_tci);
	mbuf_off_mask  += sizeof(struct nfp_flower_meta_tci);

	/* Populate Extended Metadata if required */
	if (key_layer->key_layer & NFP_FLOWER_LAYER_EXT_META) {
		nfp_flower_compile_ext_meta(mbuf_off_exact, key_layer);
		nfp_flower_compile_ext_meta(mbuf_off_mask, key_layer);
		mbuf_off_exact += sizeof(struct nfp_flower_ext_meta);
		mbuf_off_mask  += sizeof(struct nfp_flower_ext_meta);
	}

	/* Populate Port Data */
	nfp_compile_meta_port(mbuf_off_exact, key_layer, false);
	nfp_compile_meta_port(mbuf_off_mask, key_layer, true);
	mbuf_off_exact += sizeof(struct nfp_flower_in_port);
	mbuf_off_mask  += sizeof(struct nfp_flower_in_port);
}

static int
nfp_flow_key_layers_calculate_items(const struct rte_flow_item items[],
		struct nfp_fl_key_ls *key_ls)
{
	struct rte_eth_dev *ethdev;
	bool outer_ip4_flag = false;
	bool outer_ip6_flag = false;
	const struct rte_flow_item *item;
	struct nfp_flower_representor *representor;
	const struct rte_flow_item_port_id *port_id;

	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; ++item) {
		switch (item->type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_ETH detected");
			/*
			 * eth is set with no specific params.
			 * NFP does not need this.
			 */
			if (item->spec == NULL)
				continue;
			key_ls->key_layer |= NFP_FLOWER_LAYER_MAC;
			key_ls->key_size += sizeof(struct nfp_flower_mac_mpls);
			break;
		case RTE_FLOW_ITEM_TYPE_PORT_ID:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_PORT_ID detected");
			port_id = item->spec;
			if (port_id->id >= RTE_MAX_ETHPORTS)
				return -ERANGE;
			ethdev = &rte_eth_devices[port_id->id];
			representor = (struct nfp_flower_representor *)
					ethdev->data->dev_private;
			key_ls->port = rte_cpu_to_be_32(representor->port_id);
			break;
		case RTE_FLOW_ITEM_TYPE_VLAN:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_VLAN detected");
			key_ls->vlan = NFP_FLOWER_MASK_VLAN_CFI;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_IPV4 detected");
			key_ls->key_layer |= NFP_FLOWER_LAYER_IPV4;
			key_ls->key_size += sizeof(struct nfp_flower_ipv4);
			if (!outer_ip4_flag)
				outer_ip4_flag = true;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_IPV6 detected");
			key_ls->key_layer |= NFP_FLOWER_LAYER_IPV6;
			key_ls->key_size += sizeof(struct nfp_flower_ipv6);
			if (!outer_ip6_flag)
				outer_ip6_flag = true;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_TCP detected");
			key_ls->key_layer |= NFP_FLOWER_LAYER_TP;
			key_ls->key_size += sizeof(struct nfp_flower_tp_ports);
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_UDP detected");
			key_ls->key_layer |= NFP_FLOWER_LAYER_TP;
			key_ls->key_size += sizeof(struct nfp_flower_tp_ports);
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_SCTP detected");
			key_ls->key_layer |= NFP_FLOWER_LAYER_TP;
			key_ls->key_size += sizeof(struct nfp_flower_tp_ports);
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_VXLAN detected");
			/* Clear IPv4 and IPv6 bits */
			key_ls->key_layer &= ~NFP_FLOWER_LAYER_IPV4;
			key_ls->key_layer &= ~NFP_FLOWER_LAYER_IPV6;
			key_ls->tun_type = NFP_FL_TUN_VXLAN;
			key_ls->key_layer |= NFP_FLOWER_LAYER_VXLAN;
			if (outer_ip4_flag) {
				key_ls->key_size += sizeof(struct nfp_flower_ipv4_udp_tun);
				/*
				 * The outer l3 layer information is
				 * in `struct nfp_flower_ipv4_udp_tun`
				 */
				key_ls->key_size -= sizeof(struct nfp_flower_ipv4);
			} else if (outer_ip6_flag) {
				key_ls->key_layer |= NFP_FLOWER_LAYER_EXT_META;
				key_ls->key_layer_two |= NFP_FLOWER_LAYER2_TUN_IPV6;
				key_ls->key_size += sizeof(struct nfp_flower_ext_meta);
				key_ls->key_size += sizeof(struct nfp_flower_ipv6_udp_tun);
				/*
				 * The outer l3 layer information is
				 * in `struct nfp_flower_ipv6_udp_tun`
				 */
				key_ls->key_size -= sizeof(struct nfp_flower_ipv6);
			} else {
				PMD_DRV_LOG(ERR, "No outer IP layer for VXLAN tunnel.");
				return -EINVAL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GENEVE:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_GENEVE detected");
			/* Clear IPv4 and IPv6 bits */
			key_ls->key_layer &= ~NFP_FLOWER_LAYER_IPV4;
			key_ls->key_layer &= ~NFP_FLOWER_LAYER_IPV6;
			key_ls->tun_type = NFP_FL_TUN_GENEVE;
			key_ls->key_layer |= NFP_FLOWER_LAYER_EXT_META;
			key_ls->key_layer_two |= NFP_FLOWER_LAYER2_GENEVE;
			key_ls->key_size += sizeof(struct nfp_flower_ext_meta);
			if (outer_ip4_flag) {
				key_ls->key_size += sizeof(struct nfp_flower_ipv4_udp_tun);
				/*
				 * The outer l3 layer information is
				 * in `struct nfp_flower_ipv4_udp_tun`
				 */
				key_ls->key_size -= sizeof(struct nfp_flower_ipv4);
			} else if (outer_ip6_flag) {
				key_ls->key_layer_two |= NFP_FLOWER_LAYER2_TUN_IPV6;
				key_ls->key_size += sizeof(struct nfp_flower_ipv6_udp_tun);
				/*
				 * The outer l3 layer information is
				 * in `struct nfp_flower_ipv6_udp_tun`
				 */
				key_ls->key_size -= sizeof(struct nfp_flower_ipv6);
			} else {
				PMD_DRV_LOG(ERR, "No outer IP layer for GENEVE tunnel.");
				return -EINVAL;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GRE:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_GRE detected");
			/* Clear IPv4 and IPv6 bits */
			key_ls->key_layer &= ~NFP_FLOWER_LAYER_IPV4;
			key_ls->key_layer &= ~NFP_FLOWER_LAYER_IPV6;
			key_ls->tun_type = NFP_FL_TUN_GRE;
			key_ls->key_layer |= NFP_FLOWER_LAYER_EXT_META;
			key_ls->key_layer_two |= NFP_FLOWER_LAYER2_GRE;
			key_ls->key_size += sizeof(struct nfp_flower_ext_meta);
			if (outer_ip4_flag) {
				key_ls->key_size += sizeof(struct nfp_flower_ipv4_gre_tun);
				/*
				 * The outer l3 layer information is
				 * in `struct nfp_flower_ipv4_gre_tun`
				 */
				key_ls->key_size -= sizeof(struct nfp_flower_ipv4);
			} else if (outer_ip6_flag) {
				key_ls->key_layer_two |= NFP_FLOWER_LAYER2_TUN_IPV6;
				key_ls->key_size += sizeof(struct nfp_flower_ipv6_gre_tun);
				/*
				 * The outer l3 layer information is
				 * in `struct nfp_flower_ipv6_gre_tun`
				 */
				key_ls->key_size -= sizeof(struct nfp_flower_ipv6);
			} else {
				PMD_DRV_LOG(ERR, "No outer IP layer for GRE tunnel.");
				return -1;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_GRE_KEY:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ITEM_TYPE_GRE_KEY detected");
			break;
		default:
			PMD_DRV_LOG(ERR, "Item type %d not supported.", item->type);
			return -ENOTSUP;
		}
	}

	return 0;
}

static int
nfp_flow_key_layers_calculate_actions(const struct rte_flow_action actions[],
		struct nfp_fl_key_ls *key_ls)
{
	int ret = 0;
	bool meter_flag = false;
	bool tc_hl_flag = false;
	bool mac_set_flag = false;
	bool ip_set_flag = false;
	bool tp_set_flag = false;
	bool ttl_tos_flag = false;
	const struct rte_flow_action *action;

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; ++action) {
		/* Make sure actions length no longer than NFP_FL_MAX_A_SIZ */
		if (key_ls->act_size > NFP_FL_MAX_A_SIZ) {
			PMD_DRV_LOG(ERR, "The action list is too long.");
			ret = -ERANGE;
			break;
		}

		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_VOID detected");
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_DROP detected");
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_COUNT detected");
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_JUMP detected");
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_PORT_ID detected");
			key_ls->act_size += sizeof(struct nfp_fl_act_output);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_MAC_SRC detected");
			if (!mac_set_flag) {
				key_ls->act_size += sizeof(struct nfp_fl_act_set_eth);
				mac_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_MAC_DST detected");
			if (!mac_set_flag) {
				key_ls->act_size += sizeof(struct nfp_fl_act_set_eth);
				mac_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_OF_POP_VLAN detected");
			key_ls->act_size += sizeof(struct nfp_fl_act_pop_vlan);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN detected");
			key_ls->act_size += sizeof(struct nfp_fl_act_push_vlan);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID detected");
			break;
		case RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP detected");
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC detected");
			if (!ip_set_flag) {
				key_ls->act_size +=
					sizeof(struct nfp_fl_act_set_ip4_addrs);
				ip_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_IPV4_DST detected");
			if (!ip_set_flag) {
				key_ls->act_size +=
					sizeof(struct nfp_fl_act_set_ip4_addrs);
				ip_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC detected");
			key_ls->act_size += sizeof(struct nfp_fl_act_set_ipv6_addr);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_IPV6_DST detected");
			key_ls->act_size += sizeof(struct nfp_fl_act_set_ipv6_addr);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_TP_SRC detected");
			if (!tp_set_flag) {
				key_ls->act_size += sizeof(struct nfp_fl_act_set_tport);
				tp_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_TP_DST detected");
			if (!tp_set_flag) {
				key_ls->act_size += sizeof(struct nfp_fl_act_set_tport);
				tp_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_TTL detected");
			if (key_ls->key_layer & NFP_FLOWER_LAYER_IPV4) {
				if (!ttl_tos_flag) {
					key_ls->act_size +=
						sizeof(struct nfp_fl_act_set_ip4_ttl_tos);
					ttl_tos_flag = true;
				}
			} else {
				if (!tc_hl_flag) {
					key_ls->act_size +=
						sizeof(struct nfp_fl_act_set_ipv6_tc_hl_fl);
					tc_hl_flag = true;
				}
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP detected");
			if (!ttl_tos_flag) {
				key_ls->act_size +=
					sizeof(struct nfp_fl_act_set_ip4_ttl_tos);
				ttl_tos_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP detected");
			if (!tc_hl_flag) {
				key_ls->act_size +=
					sizeof(struct nfp_fl_act_set_ipv6_tc_hl_fl);
				tc_hl_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP detected");
			key_ls->act_size += sizeof(struct nfp_fl_act_pre_tun);
			key_ls->act_size += sizeof(struct nfp_fl_act_set_tun);
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_RAW_ENCAP detected");
			key_ls->act_size += sizeof(struct nfp_fl_act_pre_tun);
			key_ls->act_size += sizeof(struct nfp_fl_act_set_tun);
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_VXLAN_DECAP detected");
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_RAW_DECAP detected");
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			PMD_DRV_LOG(DEBUG, "RTE_FLOW_ACTION_TYPE_METER detected");
			if (!meter_flag) {
				key_ls->act_size += sizeof(struct nfp_fl_act_meter);
				meter_flag = true;
			} else {
				PMD_DRV_LOG(ERR, "Only support one meter action.");
				return -ENOTSUP;
			}
			break;
		default:
			PMD_DRV_LOG(ERR, "Action type %d not supported.", action->type);
			return -ENOTSUP;
		}
	}

	return ret;
}

static int
nfp_flow_key_layers_calculate(const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct nfp_fl_key_ls *key_ls)
{
	int ret = 0;

	key_ls->key_layer_two = 0;
	key_ls->key_layer = NFP_FLOWER_LAYER_PORT;
	key_ls->key_size = sizeof(struct nfp_flower_meta_tci) +
			sizeof(struct nfp_flower_in_port);
	key_ls->act_size = 0;
	key_ls->port = ~0;
	key_ls->vlan = 0;
	key_ls->tun_type = NFP_FL_TUN_NONE;

	ret |= nfp_flow_key_layers_calculate_items(items, key_ls);
	ret |= nfp_flow_key_layers_calculate_actions(actions, key_ls);

	return ret;
}

static bool
nfp_flow_is_tunnel(struct rte_flow *nfp_flow)
{
	uint32_t key_layer2;
	struct nfp_flower_ext_meta *ext_meta;
	struct nfp_flower_meta_tci *meta_tci;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_VXLAN)
		return true;

	if (!(meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_EXT_META))
		return false;

	ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);
	key_layer2 = rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2);
	if (key_layer2 & (NFP_FLOWER_LAYER2_GENEVE | NFP_FLOWER_LAYER2_GRE))
		return true;

	return false;
}

static int
nfp_flow_merge_eth(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		__rte_unused struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	struct nfp_flower_mac_mpls *eth;
	const struct rte_flow_item_eth *spec;
	const struct rte_flow_item_eth *mask;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge eth: no item->spec!");
		goto eth_end;
	}

	mask = item->mask ? item->mask : proc->mask_default;
	eth = (void *)*mbuf_off;

	if (is_mask) {
		memcpy(eth->mac_src, mask->hdr.src_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
		memcpy(eth->mac_dst, mask->hdr.dst_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	} else {
		memcpy(eth->mac_src, spec->hdr.src_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
		memcpy(eth->mac_dst, spec->hdr.dst_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	}

	eth->mpls_lse = 0;

eth_end:
	*mbuf_off += sizeof(struct nfp_flower_mac_mpls);

	return 0;
}

static int
nfp_flow_merge_vlan(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		__rte_unused char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	struct nfp_flower_meta_tci *meta_tci;
	const struct rte_flow_item_vlan *spec;
	const struct rte_flow_item_vlan *mask;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge vlan: no item->spec!");
		return 0;
	}

	mask = item->mask ? item->mask : proc->mask_default;
	if (is_mask) {
		meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.mask_data;
		meta_tci->tci |= mask->hdr.vlan_tci;
	} else {
		meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
		meta_tci->tci |= spec->hdr.vlan_tci;
	}

	return 0;
}

static int
nfp_flow_merge_ipv4(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		bool is_outer_layer)
{
	struct nfp_flower_ipv4 *ipv4;
	const struct rte_ipv4_hdr *hdr;
	struct nfp_flower_meta_tci *meta_tci;
	const struct rte_flow_item_ipv4 *spec;
	const struct rte_flow_item_ipv4 *mask;
	struct nfp_flower_ext_meta *ext_meta = NULL;
	struct nfp_flower_ipv4_udp_tun *ipv4_udp_tun;
	struct nfp_flower_ipv4_gre_tun *ipv4_gre_tun;

	spec = item->spec;
	mask = item->mask ? item->mask : proc->mask_default;
	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_EXT_META)
		ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);

	if (is_outer_layer && nfp_flow_is_tunnel(nfp_flow)) {
		if (spec == NULL) {
			PMD_DRV_LOG(DEBUG, "nfp flow merge ipv4: no item->spec!");
			return 0;
		}

		hdr = is_mask ? &mask->hdr : &spec->hdr;

		if (ext_meta && (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
				NFP_FLOWER_LAYER2_GRE)) {
			ipv4_gre_tun = (struct nfp_flower_ipv4_gre_tun *)*mbuf_off;

			ipv4_gre_tun->ip_ext.tos = hdr->type_of_service;
			ipv4_gre_tun->ip_ext.ttl = hdr->time_to_live;
			ipv4_gre_tun->ipv4.src   = hdr->src_addr;
			ipv4_gre_tun->ipv4.dst   = hdr->dst_addr;
		} else {
			ipv4_udp_tun = (struct nfp_flower_ipv4_udp_tun *)*mbuf_off;

			ipv4_udp_tun->ip_ext.tos = hdr->type_of_service;
			ipv4_udp_tun->ip_ext.ttl = hdr->time_to_live;
			ipv4_udp_tun->ipv4.src   = hdr->src_addr;
			ipv4_udp_tun->ipv4.dst   = hdr->dst_addr;
		}
	} else {
		if (spec == NULL) {
			PMD_DRV_LOG(DEBUG, "nfp flow merge ipv4: no item->spec!");
			goto ipv4_end;
		}

		/*
		 * reserve space for L4 info.
		 * rte_flow has ipv4 before L4 but NFP flower fw requires L4 before ipv4
		 */
		if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_TP)
			*mbuf_off += sizeof(struct nfp_flower_tp_ports);

		hdr = is_mask ? &mask->hdr : &spec->hdr;
		ipv4 = (struct nfp_flower_ipv4 *)*mbuf_off;

		ipv4->ip_ext.tos   = hdr->type_of_service;
		ipv4->ip_ext.proto = hdr->next_proto_id;
		ipv4->ip_ext.ttl   = hdr->time_to_live;
		ipv4->ipv4_src     = hdr->src_addr;
		ipv4->ipv4_dst     = hdr->dst_addr;

ipv4_end:
		*mbuf_off += sizeof(struct nfp_flower_ipv4);
	}

	return 0;
}

static int
nfp_flow_merge_ipv6(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		bool is_outer_layer)
{
	struct nfp_flower_ipv6 *ipv6;
	const struct rte_ipv6_hdr *hdr;
	struct nfp_flower_meta_tci *meta_tci;
	const struct rte_flow_item_ipv6 *spec;
	const struct rte_flow_item_ipv6 *mask;
	struct nfp_flower_ext_meta *ext_meta = NULL;
	struct nfp_flower_ipv6_udp_tun *ipv6_udp_tun;
	struct nfp_flower_ipv6_gre_tun *ipv6_gre_tun;

	spec = item->spec;
	mask = item->mask ? item->mask : proc->mask_default;
	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_EXT_META)
		ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);

	if (is_outer_layer && nfp_flow_is_tunnel(nfp_flow)) {
		if (spec == NULL) {
			PMD_DRV_LOG(DEBUG, "nfp flow merge ipv6: no item->spec!");
			return 0;
		}

		hdr = is_mask ? &mask->hdr : &spec->hdr;

		if (ext_meta && (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
				NFP_FLOWER_LAYER2_GRE)) {
			ipv6_gre_tun = (struct nfp_flower_ipv6_gre_tun *)*mbuf_off;

			ipv6_gre_tun->ip_ext.tos = (hdr->vtc_flow &
					RTE_IPV6_HDR_TC_MASK) >> RTE_IPV6_HDR_TC_SHIFT;
			ipv6_gre_tun->ip_ext.ttl = hdr->hop_limits;
			memcpy(ipv6_gre_tun->ipv6.ipv6_src, hdr->src_addr,
					sizeof(ipv6_gre_tun->ipv6.ipv6_src));
			memcpy(ipv6_gre_tun->ipv6.ipv6_dst, hdr->dst_addr,
					sizeof(ipv6_gre_tun->ipv6.ipv6_dst));
		} else {
			ipv6_udp_tun = (struct nfp_flower_ipv6_udp_tun *)*mbuf_off;

			ipv6_udp_tun->ip_ext.tos = (hdr->vtc_flow &
					RTE_IPV6_HDR_TC_MASK) >> RTE_IPV6_HDR_TC_SHIFT;
			ipv6_udp_tun->ip_ext.ttl = hdr->hop_limits;
			memcpy(ipv6_udp_tun->ipv6.ipv6_src, hdr->src_addr,
					sizeof(ipv6_udp_tun->ipv6.ipv6_src));
			memcpy(ipv6_udp_tun->ipv6.ipv6_dst, hdr->dst_addr,
					sizeof(ipv6_udp_tun->ipv6.ipv6_dst));
		}
	} else {
		if (spec == NULL) {
			PMD_DRV_LOG(DEBUG, "nfp flow merge ipv6: no item->spec!");
			goto ipv6_end;
		}

		/*
		 * reserve space for L4 info.
		 * rte_flow has ipv4 before L4 but NFP flower fw requires L4 before ipv6
		 */
		if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_TP)
			*mbuf_off += sizeof(struct nfp_flower_tp_ports);

		hdr = is_mask ? &mask->hdr : &spec->hdr;
		ipv6 = (struct nfp_flower_ipv6 *)*mbuf_off;

		ipv6->ip_ext.tos   = (hdr->vtc_flow & RTE_IPV6_HDR_TC_MASK) >>
				RTE_IPV6_HDR_TC_SHIFT;
		ipv6->ip_ext.proto = hdr->proto;
		ipv6->ip_ext.ttl   = hdr->hop_limits;
		memcpy(ipv6->ipv6_src, hdr->src_addr, sizeof(ipv6->ipv6_src));
		memcpy(ipv6->ipv6_dst, hdr->dst_addr, sizeof(ipv6->ipv6_dst));

ipv6_end:
		*mbuf_off += sizeof(struct nfp_flower_ipv6);
	}

	return 0;
}

static int
nfp_flow_merge_tcp(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	uint8_t tcp_flags;
	struct nfp_flower_tp_ports *ports;
	struct nfp_flower_ipv4 *ipv4 = NULL;
	struct nfp_flower_ipv6 *ipv6 = NULL;
	const struct rte_flow_item_tcp *spec;
	const struct rte_flow_item_tcp *mask;
	struct nfp_flower_meta_tci *meta_tci;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge tcp: no item->spec!");
		return 0;
	}

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_IPV4) {
		ipv4  = (struct nfp_flower_ipv4 *)
			(*mbuf_off - sizeof(struct nfp_flower_ipv4));
		ports = (struct nfp_flower_tp_ports *)
			((char *)ipv4 - sizeof(struct nfp_flower_tp_ports));
	} else { /* IPv6 */
		ipv6  = (struct nfp_flower_ipv6 *)
			(*mbuf_off - sizeof(struct nfp_flower_ipv6));
		ports = (struct nfp_flower_tp_ports *)
			((char *)ipv6 - sizeof(struct nfp_flower_tp_ports));
	}

	mask = item->mask ? item->mask : proc->mask_default;
	if (is_mask) {
		ports->port_src = mask->hdr.src_port;
		ports->port_dst = mask->hdr.dst_port;
		tcp_flags       = mask->hdr.tcp_flags;
	} else {
		ports->port_src = spec->hdr.src_port;
		ports->port_dst = spec->hdr.dst_port;
		tcp_flags       = spec->hdr.tcp_flags;
	}

	if (ipv4) {
		if (tcp_flags & RTE_TCP_FIN_FLAG)
			ipv4->ip_ext.flags |= NFP_FL_TCP_FLAG_FIN;
		if (tcp_flags & RTE_TCP_SYN_FLAG)
			ipv4->ip_ext.flags |= NFP_FL_TCP_FLAG_SYN;
		if (tcp_flags & RTE_TCP_RST_FLAG)
			ipv4->ip_ext.flags |= NFP_FL_TCP_FLAG_RST;
		if (tcp_flags & RTE_TCP_PSH_FLAG)
			ipv4->ip_ext.flags |= NFP_FL_TCP_FLAG_PSH;
		if (tcp_flags & RTE_TCP_URG_FLAG)
			ipv4->ip_ext.flags |= NFP_FL_TCP_FLAG_URG;
	} else {  /* IPv6 */
		if (tcp_flags & RTE_TCP_FIN_FLAG)
			ipv6->ip_ext.flags |= NFP_FL_TCP_FLAG_FIN;
		if (tcp_flags & RTE_TCP_SYN_FLAG)
			ipv6->ip_ext.flags |= NFP_FL_TCP_FLAG_SYN;
		if (tcp_flags & RTE_TCP_RST_FLAG)
			ipv6->ip_ext.flags |= NFP_FL_TCP_FLAG_RST;
		if (tcp_flags & RTE_TCP_PSH_FLAG)
			ipv6->ip_ext.flags |= NFP_FL_TCP_FLAG_PSH;
		if (tcp_flags & RTE_TCP_URG_FLAG)
			ipv6->ip_ext.flags |= NFP_FL_TCP_FLAG_URG;
	}

	return 0;
}

static int
nfp_flow_merge_udp(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		bool is_outer_layer)
{
	char *ports_off;
	struct nfp_flower_tp_ports *ports;
	const struct rte_flow_item_udp *spec;
	const struct rte_flow_item_udp *mask;
	struct nfp_flower_meta_tci *meta_tci;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge udp: no item->spec!");
		return 0;
	}

	/* Don't add L4 info if working on a inner layer pattern */
	if (!is_outer_layer) {
		PMD_DRV_LOG(INFO, "Detected inner layer UDP, skipping.");
		return 0;
	}

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_IPV4) {
		ports_off = *mbuf_off - sizeof(struct nfp_flower_ipv4) -
			sizeof(struct nfp_flower_tp_ports);
	} else {/* IPv6 */
		ports_off = *mbuf_off - sizeof(struct nfp_flower_ipv6) -
			sizeof(struct nfp_flower_tp_ports);
	}
	ports = (struct nfp_flower_tp_ports *)ports_off;

	mask = item->mask ? item->mask : proc->mask_default;
	if (is_mask) {
		ports->port_src = mask->hdr.src_port;
		ports->port_dst = mask->hdr.dst_port;
	} else {
		ports->port_src = spec->hdr.src_port;
		ports->port_dst = spec->hdr.dst_port;
	}

	return 0;
}

static int
nfp_flow_merge_sctp(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	char *ports_off;
	struct nfp_flower_tp_ports *ports;
	struct nfp_flower_meta_tci *meta_tci;
	const struct rte_flow_item_sctp *spec;
	const struct rte_flow_item_sctp *mask;

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge sctp: no item->spec!");
		return 0;
	}

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_IPV4) {
		ports_off = *mbuf_off - sizeof(struct nfp_flower_ipv4) -
			sizeof(struct nfp_flower_tp_ports);
	} else { /* IPv6 */
		ports_off = *mbuf_off - sizeof(struct nfp_flower_ipv6) -
			sizeof(struct nfp_flower_tp_ports);
	}
	ports = (struct nfp_flower_tp_ports *)ports_off;

	mask = item->mask ? item->mask : proc->mask_default;
	if (is_mask) {
		ports->port_src = mask->hdr.src_port;
		ports->port_dst = mask->hdr.dst_port;
	} else {
		ports->port_src = spec->hdr.src_port;
		ports->port_dst = spec->hdr.dst_port;
	}

	return 0;
}

static int
nfp_flow_merge_vxlan(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	int ret = 0;
	const struct rte_vxlan_hdr *hdr;
	struct nfp_flower_ipv4_udp_tun *tun4;
	struct nfp_flower_ipv6_udp_tun *tun6;
	struct nfp_flower_meta_tci *meta_tci;
	const struct rte_flow_item_vxlan *spec;
	const struct rte_flow_item_vxlan *mask;
	struct nfp_flower_ext_meta *ext_meta = NULL;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_EXT_META)
		ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge vxlan: no item->spec!");
		goto vxlan_end;
	}

	mask = item->mask ? item->mask : proc->mask_default;
	hdr = is_mask ? &mask->hdr : &spec->hdr;

	if (ext_meta && (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
			NFP_FLOWER_LAYER2_TUN_IPV6)) {
		tun6 = (struct nfp_flower_ipv6_udp_tun *)*mbuf_off;
		tun6->tun_id = hdr->vx_vni;
		if (!is_mask)
			ret = nfp_tun_add_ipv6_off(app_fw_flower, tun6->ipv6.ipv6_dst);
	} else {
		tun4 = (struct nfp_flower_ipv4_udp_tun *)*mbuf_off;
		tun4->tun_id = hdr->vx_vni;
		if (!is_mask)
			ret = nfp_tun_add_ipv4_off(app_fw_flower, tun4->ipv4.dst);
	}

vxlan_end:
	if (ext_meta && (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
			NFP_FLOWER_LAYER2_TUN_IPV6))
		*mbuf_off += sizeof(struct nfp_flower_ipv6_udp_tun);
	else
		*mbuf_off += sizeof(struct nfp_flower_ipv4_udp_tun);

	return ret;
}

static int
nfp_flow_merge_geneve(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	int ret = 0;
	struct nfp_flower_ipv4_udp_tun *tun4;
	struct nfp_flower_ipv6_udp_tun *tun6;
	struct nfp_flower_meta_tci *meta_tci;
	const struct rte_flow_item_geneve *spec;
	const struct rte_flow_item_geneve *mask;
	const struct rte_flow_item_geneve *geneve;
	struct nfp_flower_ext_meta *ext_meta = NULL;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_EXT_META)
		ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge geneve: no item->spec!");
		goto geneve_end;
	}

	mask = item->mask ? item->mask : proc->mask_default;
	geneve = is_mask ? mask : spec;

	if (ext_meta && (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
			NFP_FLOWER_LAYER2_TUN_IPV6)) {
		tun6 = (struct nfp_flower_ipv6_udp_tun *)*mbuf_off;
		tun6->tun_id = rte_cpu_to_be_32((geneve->vni[0] << 16) |
				(geneve->vni[1] << 8) | (geneve->vni[2]));
		if (!is_mask)
			ret = nfp_tun_add_ipv6_off(app_fw_flower, tun6->ipv6.ipv6_dst);
	} else {
		tun4 = (struct nfp_flower_ipv4_udp_tun *)*mbuf_off;
		tun4->tun_id = rte_cpu_to_be_32((geneve->vni[0] << 16) |
				(geneve->vni[1] << 8) | (geneve->vni[2]));
		if (!is_mask)
			ret = nfp_tun_add_ipv4_off(app_fw_flower, tun4->ipv4.dst);
	}

geneve_end:
	if (ext_meta && (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
			NFP_FLOWER_LAYER2_TUN_IPV6)) {
		*mbuf_off += sizeof(struct nfp_flower_ipv6_udp_tun);
	} else {
		*mbuf_off += sizeof(struct nfp_flower_ipv4_udp_tun);
	}

	return ret;
}

static int
nfp_flow_merge_gre(__rte_unused struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		__rte_unused const struct rte_flow_item *item,
		__rte_unused const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_flower_ext_meta *ext_meta;
	struct nfp_flower_ipv4_gre_tun *tun4;
	struct nfp_flower_ipv6_gre_tun *tun6;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);

	/* NVGRE is the only supported GRE tunnel type */
	if (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
			NFP_FLOWER_LAYER2_TUN_IPV6) {
		tun6 = (struct nfp_flower_ipv6_gre_tun *)*mbuf_off;
		if (is_mask)
			tun6->ethertype = rte_cpu_to_be_16(~0);
		else
			tun6->ethertype = rte_cpu_to_be_16(0x6558);
	} else {
		tun4 = (struct nfp_flower_ipv4_gre_tun *)*mbuf_off;
		if (is_mask)
			tun4->ethertype = rte_cpu_to_be_16(~0);
		else
			tun4->ethertype = rte_cpu_to_be_16(0x6558);
	}

	return 0;
}

static int
nfp_flow_merge_gre_key(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		char **mbuf_off,
		const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc,
		bool is_mask,
		__rte_unused bool is_outer_layer)
{
	int ret = 0;
	rte_be32_t tun_key;
	const rte_be32_t *spec;
	const rte_be32_t *mask;
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_flower_ext_meta *ext_meta;
	struct nfp_flower_ipv4_gre_tun *tun4;
	struct nfp_flower_ipv6_gre_tun *tun6;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	ext_meta = (struct nfp_flower_ext_meta *)(meta_tci + 1);

	spec = item->spec;
	if (spec == NULL) {
		PMD_DRV_LOG(DEBUG, "nfp flow merge gre key: no item->spec!");
		goto gre_key_end;
	}

	mask = item->mask ? item->mask : proc->mask_default;
	tun_key = is_mask ? *mask : *spec;

	if (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
			NFP_FLOWER_LAYER2_TUN_IPV6) {
		tun6 = (struct nfp_flower_ipv6_gre_tun *)*mbuf_off;
		tun6->tun_key = tun_key;
		tun6->tun_flags = rte_cpu_to_be_16(NFP_FL_GRE_FLAG_KEY);
		if (!is_mask)
			ret = nfp_tun_add_ipv6_off(app_fw_flower, tun6->ipv6.ipv6_dst);
	} else {
		tun4 = (struct nfp_flower_ipv4_gre_tun *)*mbuf_off;
		tun4->tun_key = tun_key;
		tun4->tun_flags = rte_cpu_to_be_16(NFP_FL_GRE_FLAG_KEY);
		if (!is_mask)
			ret = nfp_tun_add_ipv4_off(app_fw_flower, tun4->ipv4.dst);
	}

gre_key_end:
	if (rte_be_to_cpu_32(ext_meta->nfp_flow_key_layer2) &
			NFP_FLOWER_LAYER2_TUN_IPV6)
		*mbuf_off += sizeof(struct nfp_flower_ipv6_gre_tun);
	else
		*mbuf_off += sizeof(struct nfp_flower_ipv4_gre_tun);

	return ret;
}

const rte_be32_t nfp_flow_item_gre_key = 0xffffffff;

/* Graph of supported items and associated process function */
static const struct nfp_flow_item_proc nfp_flow_item_proc_list[] = {
	[RTE_FLOW_ITEM_TYPE_END] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_ETH,
			RTE_FLOW_ITEM_TYPE_IPV4,
			RTE_FLOW_ITEM_TYPE_IPV6),
	},
	[RTE_FLOW_ITEM_TYPE_ETH] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_VLAN,
			RTE_FLOW_ITEM_TYPE_IPV4,
			RTE_FLOW_ITEM_TYPE_IPV6),
		.mask_support = &(const struct rte_flow_item_eth){
			.hdr = {
				.dst_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
				.src_addr.addr_bytes = "\xff\xff\xff\xff\xff\xff",
				.ether_type          = RTE_BE16(0xffff),
			},
			.has_vlan = 1,
		},
		.mask_default = &rte_flow_item_eth_mask,
		.mask_sz = sizeof(struct rte_flow_item_eth),
		.merge = nfp_flow_merge_eth,
	},
	[RTE_FLOW_ITEM_TYPE_VLAN] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_IPV4,
			RTE_FLOW_ITEM_TYPE_IPV6),
		.mask_support = &(const struct rte_flow_item_vlan){
			.hdr = {
				.vlan_tci  = RTE_BE16(0xefff),
				.eth_proto = RTE_BE16(0xffff),
			},
			.has_more_vlan = 1,
		},
		.mask_default = &rte_flow_item_vlan_mask,
		.mask_sz = sizeof(struct rte_flow_item_vlan),
		.merge = nfp_flow_merge_vlan,
	},
	[RTE_FLOW_ITEM_TYPE_IPV4] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_TCP,
			RTE_FLOW_ITEM_TYPE_UDP,
			RTE_FLOW_ITEM_TYPE_SCTP,
			RTE_FLOW_ITEM_TYPE_GRE),
		.mask_support = &(const struct rte_flow_item_ipv4){
			.hdr = {
				.type_of_service = 0xff,
				.fragment_offset = RTE_BE16(0xffff),
				.time_to_live    = 0xff,
				.next_proto_id   = 0xff,
				.src_addr        = RTE_BE32(0xffffffff),
				.dst_addr        = RTE_BE32(0xffffffff),
			},
		},
		.mask_default = &rte_flow_item_ipv4_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv4),
		.merge = nfp_flow_merge_ipv4,
	},
	[RTE_FLOW_ITEM_TYPE_IPV6] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_TCP,
			RTE_FLOW_ITEM_TYPE_UDP,
			RTE_FLOW_ITEM_TYPE_SCTP,
			RTE_FLOW_ITEM_TYPE_GRE),
		.mask_support = &(const struct rte_flow_item_ipv6){
			.hdr = {
				.vtc_flow   = RTE_BE32(0x0ff00000),
				.proto      = 0xff,
				.hop_limits = 0xff,
				.src_addr   = "\xff\xff\xff\xff\xff\xff\xff\xff"
					"\xff\xff\xff\xff\xff\xff\xff\xff",
				.dst_addr   = "\xff\xff\xff\xff\xff\xff\xff\xff"
					"\xff\xff\xff\xff\xff\xff\xff\xff",
			},
			.has_frag_ext = 1,
		},
		.mask_default = &rte_flow_item_ipv6_mask,
		.mask_sz = sizeof(struct rte_flow_item_ipv6),
		.merge = nfp_flow_merge_ipv6,
	},
	[RTE_FLOW_ITEM_TYPE_TCP] = {
		.mask_support = &(const struct rte_flow_item_tcp){
			.hdr = {
				.tcp_flags = 0xff,
				.src_port  = RTE_BE16(0xffff),
				.dst_port  = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_tcp_mask,
		.mask_sz = sizeof(struct rte_flow_item_tcp),
		.merge = nfp_flow_merge_tcp,
	},
	[RTE_FLOW_ITEM_TYPE_UDP] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_VXLAN,
			RTE_FLOW_ITEM_TYPE_GENEVE),
		.mask_support = &(const struct rte_flow_item_udp){
			.hdr = {
				.src_port = RTE_BE16(0xffff),
				.dst_port = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_udp_mask,
		.mask_sz = sizeof(struct rte_flow_item_udp),
		.merge = nfp_flow_merge_udp,
	},
	[RTE_FLOW_ITEM_TYPE_SCTP] = {
		.mask_support = &(const struct rte_flow_item_sctp){
			.hdr = {
				.src_port  = RTE_BE16(0xffff),
				.dst_port  = RTE_BE16(0xffff),
			},
		},
		.mask_default = &rte_flow_item_sctp_mask,
		.mask_sz = sizeof(struct rte_flow_item_sctp),
		.merge = nfp_flow_merge_sctp,
	},
	[RTE_FLOW_ITEM_TYPE_VXLAN] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_ETH),
		.mask_support = &(const struct rte_flow_item_vxlan){
			.hdr = {
				.vx_vni = RTE_BE32(0xffffff00),
			},
		},
		.mask_default = &rte_flow_item_vxlan_mask,
		.mask_sz = sizeof(struct rte_flow_item_vxlan),
		.merge = nfp_flow_merge_vxlan,
	},
	[RTE_FLOW_ITEM_TYPE_GENEVE] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_ETH),
		.mask_support = &(const struct rte_flow_item_geneve){
			.vni = "\xff\xff\xff",
		},
		.mask_default = &rte_flow_item_geneve_mask,
		.mask_sz = sizeof(struct rte_flow_item_geneve),
		.merge = nfp_flow_merge_geneve,
	},
	[RTE_FLOW_ITEM_TYPE_GRE] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_GRE_KEY),
		.mask_support = &(const struct rte_flow_item_gre){
			.c_rsvd0_ver = RTE_BE16(0xa000),
			.protocol = RTE_BE16(0xffff),
		},
		.mask_default = &rte_flow_item_gre_mask,
		.mask_sz = sizeof(struct rte_flow_item_gre),
		.merge = nfp_flow_merge_gre,
	},
	[RTE_FLOW_ITEM_TYPE_GRE_KEY] = {
		.next_item = NEXT_ITEM(RTE_FLOW_ITEM_TYPE_ETH),
		.mask_support = &nfp_flow_item_gre_key,
		.mask_default = &nfp_flow_item_gre_key,
		.mask_sz = sizeof(rte_be32_t),
		.merge = nfp_flow_merge_gre_key,
	},
};

static int
nfp_flow_item_check(const struct rte_flow_item *item,
		const struct nfp_flow_item_proc *proc)
{
	int ret = 0;
	unsigned int i;
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

	mask = item->mask ?
		(const uint8_t *)item->mask :
		(const uint8_t *)proc->mask_default;

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

		if (item->last && (((const uint8_t *)item->spec)[i] & mask[i]) !=
				(((const uint8_t *)item->last)[i] & mask[i])) {
			PMD_DRV_LOG(ERR, "Range between 'spec' and 'last'"
					" is larger than 'mask'.");
			ret = -ERANGE;
			break;
		}
	}

	return ret;
}

static bool
nfp_flow_is_tun_item(const struct rte_flow_item *item)
{
	if (item->type == RTE_FLOW_ITEM_TYPE_VXLAN ||
			item->type == RTE_FLOW_ITEM_TYPE_GENEVE ||
			item->type == RTE_FLOW_ITEM_TYPE_GRE_KEY)
		return true;

	return false;
}

static bool
nfp_flow_inner_item_get(const struct rte_flow_item items[],
		const struct rte_flow_item **inner_item)
{
	const struct rte_flow_item *item;

	*inner_item = items;

	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END; ++item) {
		if (nfp_flow_is_tun_item(item)) {
			*inner_item = ++item;
			return true;
		}
	}

	return false;
}

static int
nfp_flow_compile_item_proc(struct nfp_flower_representor *repr,
		const struct rte_flow_item items[],
		struct rte_flow *nfp_flow,
		char **mbuf_off_exact,
		char **mbuf_off_mask,
		bool is_outer_layer)
{
	int i;
	int ret = 0;
	bool continue_flag = true;
	const struct rte_flow_item *item;
	const struct nfp_flow_item_proc *proc_list;
	struct nfp_app_fw_flower *app_fw_flower = repr->app_fw_flower;

	proc_list = nfp_flow_item_proc_list;
	for (item = items; item->type != RTE_FLOW_ITEM_TYPE_END && continue_flag; ++item) {
		const struct nfp_flow_item_proc *proc = NULL;

		if (nfp_flow_is_tun_item(item))
			continue_flag = false;

		for (i = 0; proc_list->next_item && proc_list->next_item[i]; ++i) {
			if (proc_list->next_item[i] == item->type) {
				proc = &nfp_flow_item_proc_list[item->type];
				break;
			}
		}

		if (proc == NULL) {
			PMD_DRV_LOG(ERR, "No next item provided for %d", item->type);
			ret = -ENOTSUP;
			break;
		}

		/* Perform basic sanity checks */
		ret = nfp_flow_item_check(item, proc);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "nfp flow item %d check failed", item->type);
			ret = -EINVAL;
			break;
		}

		if (proc->merge == NULL) {
			PMD_DRV_LOG(ERR, "nfp flow item %d no proc function", item->type);
			ret = -ENOTSUP;
			break;
		}

		ret = proc->merge(app_fw_flower, nfp_flow, mbuf_off_exact, item,
				proc, false, is_outer_layer);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "nfp flow item %d exact merge failed", item->type);
			break;
		}

		ret = proc->merge(app_fw_flower, nfp_flow, mbuf_off_mask, item,
				proc, true, is_outer_layer);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "nfp flow item %d mask merge failed", item->type);
			break;
		}

		proc_list = proc;
	}

	return ret;
}

static int
nfp_flow_compile_items(struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		struct rte_flow *nfp_flow)
{
	int ret;
	char *mbuf_off_mask;
	char *mbuf_off_exact;
	bool is_tun_flow = false;
	bool is_outer_layer = true;
	struct nfp_flower_meta_tci *meta_tci;
	const struct rte_flow_item *loop_item;

	mbuf_off_exact = nfp_flow->payload.unmasked_data +
			sizeof(struct nfp_flower_meta_tci) +
			sizeof(struct nfp_flower_in_port);
	mbuf_off_mask  = nfp_flow->payload.mask_data +
			sizeof(struct nfp_flower_meta_tci) +
			sizeof(struct nfp_flower_in_port);

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_EXT_META) {
		mbuf_off_exact += sizeof(struct nfp_flower_ext_meta);
		mbuf_off_mask += sizeof(struct nfp_flower_ext_meta);
	}

	/* Check if this is a tunnel flow and get the inner item*/
	is_tun_flow = nfp_flow_inner_item_get(items, &loop_item);
	if (is_tun_flow)
		is_outer_layer = false;

	/* Go over items */
	ret = nfp_flow_compile_item_proc(representor, loop_item, nfp_flow,
			&mbuf_off_exact, &mbuf_off_mask, is_outer_layer);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "nfp flow item compile failed.");
		return -EINVAL;
	}

	/* Go over inner items */
	if (is_tun_flow) {
		ret = nfp_flow_compile_item_proc(representor, items, nfp_flow,
				&mbuf_off_exact, &mbuf_off_mask, true);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "nfp flow outer item compile failed.");
			return -EINVAL;
		}
	}

	return 0;
}

static int
nfp_flow_action_output(char *act_data,
		const struct rte_flow_action *action,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		uint32_t output_cnt)
{
	size_t act_size;
	struct rte_eth_dev *ethdev;
	struct nfp_fl_act_output *output;
	struct nfp_flower_representor *representor;
	const struct rte_flow_action_port_id *port_id;

	port_id = action->conf;
	if (port_id == NULL || port_id->id >= RTE_MAX_ETHPORTS)
		return -ERANGE;

	ethdev = &rte_eth_devices[port_id->id];
	representor = (struct nfp_flower_representor *)ethdev->data->dev_private;
	act_size = sizeof(struct nfp_fl_act_output);

	output = (struct nfp_fl_act_output *)act_data;
	output->head.jump_id = NFP_FL_ACTION_OPCODE_OUTPUT;
	output->head.len_lw  = act_size >> NFP_FL_LW_SIZ;
	output->port         = rte_cpu_to_be_32(representor->port_id);
	if (output_cnt == 0)
		output->flags = rte_cpu_to_be_16(NFP_FL_OUT_FLAGS_LAST);

	nfp_flow_meta->shortcut = rte_cpu_to_be_32(representor->port_id);

	return 0;
}

static void
nfp_flow_action_set_mac(char *act_data,
		const struct rte_flow_action *action,
		bool mac_src_flag,
		bool mac_set_flag)
{
	uint8_t i;
	size_t act_size;
	struct nfp_fl_act_set_eth *set_eth;
	const struct rte_flow_action_set_mac *set_mac;

	if (mac_set_flag)
		set_eth = (struct nfp_fl_act_set_eth *)act_data - 1;
	else
		set_eth = (struct nfp_fl_act_set_eth *)act_data;

	act_size = sizeof(struct nfp_fl_act_set_eth);
	set_eth->head.jump_id = NFP_FL_ACTION_OPCODE_SET_ETHERNET;
	set_eth->head.len_lw  = act_size >> NFP_FL_LW_SIZ;
	set_eth->reserved     = 0;

	set_mac = (const struct rte_flow_action_set_mac *)action->conf;
	if (mac_src_flag) {
		rte_memcpy(&set_eth->eth_addr[RTE_ETHER_ADDR_LEN],
				set_mac->mac_addr, RTE_ETHER_ADDR_LEN);
		for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
			set_eth->eth_addr_mask[RTE_ETHER_ADDR_LEN + i] = 0xff;
	} else {
		rte_memcpy(&set_eth->eth_addr[0],
				set_mac->mac_addr, RTE_ETHER_ADDR_LEN);
		for (i = 0; i < RTE_ETHER_ADDR_LEN; i++)
			set_eth->eth_addr_mask[i] = 0xff;
	}
}

static void
nfp_flow_action_pop_vlan(char *act_data,
		struct nfp_fl_rule_metadata *nfp_flow_meta)
{
	size_t act_size;
	struct nfp_fl_act_pop_vlan *pop_vlan;

	act_size = sizeof(struct nfp_fl_act_pop_vlan);
	pop_vlan = (struct nfp_fl_act_pop_vlan *)act_data;
	pop_vlan->head.jump_id = NFP_FL_ACTION_OPCODE_POP_VLAN;
	pop_vlan->head.len_lw  = act_size >> NFP_FL_LW_SIZ;
	pop_vlan->reserved     = 0;

	nfp_flow_meta->shortcut = rte_cpu_to_be_32(NFP_FL_SC_ACT_POPV);
}

static void
nfp_flow_action_set_ip(char *act_data,
		const struct rte_flow_action *action,
		bool ip_src_flag,
		bool ip_set_flag)
{
	size_t act_size;
	struct nfp_fl_act_set_ip4_addrs *set_ip;
	const struct rte_flow_action_set_ipv4 *set_ipv4;

	if (ip_set_flag)
		set_ip = (struct nfp_fl_act_set_ip4_addrs *)act_data - 1;
	else
		set_ip = (struct nfp_fl_act_set_ip4_addrs *)act_data;

	act_size = sizeof(struct nfp_fl_act_set_ip4_addrs);
	set_ip->head.jump_id = NFP_FL_ACTION_OPCODE_SET_IPV4_ADDRS;
	set_ip->head.len_lw  = act_size >> NFP_FL_LW_SIZ;
	set_ip->reserved     = 0;

	set_ipv4 = (const struct rte_flow_action_set_ipv4 *)action->conf;
	if (ip_src_flag) {
		set_ip->ipv4_src = set_ipv4->ipv4_addr;
		set_ip->ipv4_src_mask = RTE_BE32(0xffffffff);
	} else {
		set_ip->ipv4_dst = set_ipv4->ipv4_addr;
		set_ip->ipv4_dst_mask = RTE_BE32(0xffffffff);
	}
}

static void
nfp_flow_action_set_ipv6(char *act_data,
		const struct rte_flow_action *action,
		bool ip_src_flag)
{
	int i;
	size_t act_size;
	struct nfp_fl_act_set_ipv6_addr *set_ip;
	const struct rte_flow_action_set_ipv6 *set_ipv6;

	set_ip = (struct nfp_fl_act_set_ipv6_addr *)act_data;
	set_ipv6 = (const struct rte_flow_action_set_ipv6 *)action->conf;

	if (ip_src_flag)
		set_ip->head.jump_id = NFP_FL_ACTION_OPCODE_SET_IPV6_SRC;
	else
		set_ip->head.jump_id = NFP_FL_ACTION_OPCODE_SET_IPV6_DST;

	act_size = sizeof(struct nfp_fl_act_set_ipv6_addr);
	set_ip->head.len_lw = act_size >> NFP_FL_LW_SIZ;
	set_ip->reserved = 0;

	for (i = 0; i < 4; i++) {
		set_ip->ipv6[i].exact = set_ipv6->ipv6_addr[i * 4];
		set_ip->ipv6[i].mask = RTE_BE32(0xffffffff);
	}
}

static void
nfp_flow_action_set_tp(char *act_data,
		const struct rte_flow_action *action,
		bool tp_src_flag,
		bool tp_set_flag)
{
	size_t act_size;
	struct nfp_fl_act_set_tport *set_tp;
	const struct rte_flow_action_set_tp *set_tp_conf;

	if (tp_set_flag)
		set_tp = (struct nfp_fl_act_set_tport *)act_data - 1;
	else
		set_tp = (struct nfp_fl_act_set_tport *)act_data;

	act_size = sizeof(struct nfp_fl_act_set_tport);
	set_tp->head.jump_id = NFP_FL_ACTION_OPCODE_SET_TCP;
	set_tp->head.len_lw  = act_size >> NFP_FL_LW_SIZ;
	set_tp->reserved     = 0;

	set_tp_conf = (const struct rte_flow_action_set_tp *)action->conf;
	if (tp_src_flag) {
		set_tp->src_port = set_tp_conf->port;
		set_tp->src_port_mask = RTE_BE16(0xffff);
	} else {
		set_tp->dst_port = set_tp_conf->port;
		set_tp->dst_port_mask = RTE_BE16(0xffff);
	}
}

static int
nfp_flow_action_push_vlan(char *act_data,
		const struct rte_flow_action *action)
{
	size_t act_size;
	struct nfp_fl_act_push_vlan *push_vlan;
	const struct rte_flow_action_of_push_vlan *push_vlan_conf;
	const struct rte_flow_action_of_set_vlan_pcp *vlan_pcp_conf;
	const struct rte_flow_action_of_set_vlan_vid *vlan_vid_conf;

	if (((action + 1)->type != RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP) ||
			((action + 2)->type != RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID))
		return -EINVAL;

	act_size = sizeof(struct nfp_fl_act_push_vlan);
	push_vlan = (struct nfp_fl_act_push_vlan *)act_data;
	push_vlan->head.jump_id = NFP_FL_ACTION_OPCODE_PUSH_VLAN;
	push_vlan->head.len_lw  = act_size >> NFP_FL_LW_SIZ;
	push_vlan->reserved     = 0;

	push_vlan_conf = (const struct rte_flow_action_of_push_vlan *)
			action->conf;
	vlan_pcp_conf  = (const struct rte_flow_action_of_set_vlan_pcp *)
			(action + 1)->conf;
	vlan_vid_conf  = (const struct rte_flow_action_of_set_vlan_vid *)
			(action + 2)->conf;
	push_vlan->vlan_tpid = push_vlan_conf->ethertype;
	push_vlan->vlan_tci = ((vlan_pcp_conf->vlan_pcp & 0x07) << 13) |
			(vlan_vid_conf->vlan_vid & 0x0fff);

	return 0;
}

static void
nfp_flow_action_set_ttl(char *act_data,
		const struct rte_flow_action *action,
		bool ttl_tos_flag)
{
	size_t act_size;
	struct nfp_fl_act_set_ip4_ttl_tos *ttl_tos;
	const struct rte_flow_action_set_ttl *ttl_conf;

	if (ttl_tos_flag)
		ttl_tos = (struct nfp_fl_act_set_ip4_ttl_tos *)act_data - 1;
	else
		ttl_tos = (struct nfp_fl_act_set_ip4_ttl_tos *)act_data;

	act_size = sizeof(struct nfp_fl_act_set_ip4_ttl_tos);
	ttl_tos->head.jump_id = NFP_FL_ACTION_OPCODE_SET_IPV4_TTL_TOS;
	ttl_tos->head.len_lw = act_size >> NFP_FL_LW_SIZ;

	ttl_conf = (const struct rte_flow_action_set_ttl *)action->conf;
	ttl_tos->ipv4_ttl = ttl_conf->ttl_value;
	ttl_tos->ipv4_ttl_mask = 0xff;
	ttl_tos->reserved = 0;
}

static void
nfp_flow_action_set_hl(char *act_data,
		const struct rte_flow_action *action,
		bool tc_hl_flag)
{
	size_t act_size;
	struct nfp_fl_act_set_ipv6_tc_hl_fl *tc_hl;
	const struct rte_flow_action_set_ttl *ttl_conf;

	if (tc_hl_flag)
		tc_hl = (struct nfp_fl_act_set_ipv6_tc_hl_fl *)act_data - 1;
	else
		tc_hl = (struct nfp_fl_act_set_ipv6_tc_hl_fl *)act_data;

	act_size = sizeof(struct nfp_fl_act_set_ipv6_tc_hl_fl);
	tc_hl->head.jump_id = NFP_FL_ACTION_OPCODE_SET_IPV6_TC_HL_FL;
	tc_hl->head.len_lw = act_size >> NFP_FL_LW_SIZ;

	ttl_conf = (const struct rte_flow_action_set_ttl *)action->conf;
	tc_hl->ipv6_hop_limit = ttl_conf->ttl_value;
	tc_hl->ipv6_hop_limit_mask = 0xff;
	tc_hl->reserved = 0;
}

static void
nfp_flow_action_set_tos(char *act_data,
		const struct rte_flow_action *action,
		bool ttl_tos_flag)
{
	size_t act_size;
	struct nfp_fl_act_set_ip4_ttl_tos *ttl_tos;
	const struct rte_flow_action_set_dscp *tos_conf;

	if (ttl_tos_flag)
		ttl_tos = (struct nfp_fl_act_set_ip4_ttl_tos *)act_data - 1;
	else
		ttl_tos = (struct nfp_fl_act_set_ip4_ttl_tos *)act_data;

	act_size = sizeof(struct nfp_fl_act_set_ip4_ttl_tos);
	ttl_tos->head.jump_id = NFP_FL_ACTION_OPCODE_SET_IPV4_TTL_TOS;
	ttl_tos->head.len_lw = act_size >> NFP_FL_LW_SIZ;

	tos_conf = (const struct rte_flow_action_set_dscp *)action->conf;
	ttl_tos->ipv4_tos = tos_conf->dscp;
	ttl_tos->ipv4_tos_mask = 0xff;
	ttl_tos->reserved = 0;
}

static void
nfp_flow_action_set_tc(char *act_data,
		const struct rte_flow_action *action,
		bool tc_hl_flag)
{
	size_t act_size;
	struct nfp_fl_act_set_ipv6_tc_hl_fl *tc_hl;
	const struct rte_flow_action_set_dscp *tos_conf;

	if (tc_hl_flag)
		tc_hl = (struct nfp_fl_act_set_ipv6_tc_hl_fl *)act_data - 1;
	else
		tc_hl = (struct nfp_fl_act_set_ipv6_tc_hl_fl *)act_data;

	act_size = sizeof(struct nfp_fl_act_set_ipv6_tc_hl_fl);
	tc_hl->head.jump_id = NFP_FL_ACTION_OPCODE_SET_IPV6_TC_HL_FL;
	tc_hl->head.len_lw = act_size >> NFP_FL_LW_SIZ;

	tos_conf = (const struct rte_flow_action_set_dscp *)action->conf;
	tc_hl->ipv6_tc = tos_conf->dscp;
	tc_hl->ipv6_tc_mask = 0xff;
	tc_hl->reserved = 0;
}

static void
nfp_flow_pre_tun_v4_process(struct nfp_fl_act_pre_tun *pre_tun,
		rte_be32_t ipv4_dst)
{
	pre_tun->head.jump_id = NFP_FL_ACTION_OPCODE_PRE_TUNNEL;
	pre_tun->head.len_lw  = sizeof(struct nfp_fl_act_pre_tun) >> NFP_FL_LW_SIZ;
	pre_tun->ipv4_dst     = ipv4_dst;
}

static void
nfp_flow_pre_tun_v6_process(struct nfp_fl_act_pre_tun *pre_tun,
		const uint8_t ipv6_dst[])
{
	pre_tun->head.jump_id = NFP_FL_ACTION_OPCODE_PRE_TUNNEL;
	pre_tun->head.len_lw  = sizeof(struct nfp_fl_act_pre_tun) >> NFP_FL_LW_SIZ;
	pre_tun->flags        = rte_cpu_to_be_16(NFP_FL_PRE_TUN_IPV6);
	memcpy(pre_tun->ipv6_dst, ipv6_dst, sizeof(pre_tun->ipv6_dst));
}

static void
nfp_flow_set_tun_process(struct nfp_fl_act_set_tun *set_tun,
		enum nfp_flower_tun_type tun_type,
		uint64_t tun_id,
		uint8_t ttl,
		uint8_t tos)
{
	/* Currently only support one pre-tunnel, so index is always 0. */
	uint8_t pretun_idx = 0;
	uint32_t tun_type_index;

	tun_type_index = ((tun_type << 4) & 0xf0) | (pretun_idx & 0x07);

	set_tun->head.jump_id   = NFP_FL_ACTION_OPCODE_SET_TUNNEL;
	set_tun->head.len_lw    = sizeof(struct nfp_fl_act_set_tun) >> NFP_FL_LW_SIZ;
	set_tun->tun_type_index = rte_cpu_to_be_32(tun_type_index);
	set_tun->tun_id         = rte_cpu_to_be_64(tun_id);
	set_tun->ttl            = ttl;
	set_tun->tos            = tos;
}

static int
nfp_flower_add_tun_neigh_v4_encap(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun,
		const struct rte_ether_hdr *eth,
		const struct rte_flow_item_ipv4 *ipv4)
{
	struct nfp_fl_tun *tmp;
	struct nfp_flow_priv *priv;
	struct nfp_flower_in_port *port;
	struct nfp_flower_cmsg_tun_neigh_v4 payload;

	tun->payload.v6_flag = 0;
	tun->payload.dst.dst_ipv4 = ipv4->hdr.dst_addr;
	tun->payload.src.src_ipv4 = ipv4->hdr.src_addr;
	memcpy(tun->payload.dst_addr, eth->dst_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	memcpy(tun->payload.src_addr, eth->src_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

	tun->ref_cnt = 1;
	priv = app_fw_flower->flow_priv;
	LIST_FOREACH(tmp, &priv->nn_list, next) {
		if (memcmp(&tmp->payload, &tun->payload, sizeof(struct nfp_fl_tun_entry)) == 0) {
			tmp->ref_cnt++;
			return 0;
		}
	}

	LIST_INSERT_HEAD(&priv->nn_list, tun, next);

	port = (struct nfp_flower_in_port *)((char *)nfp_flow_meta +
			sizeof(struct nfp_fl_rule_metadata) +
			sizeof(struct nfp_flower_meta_tci));

	memset(&payload, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v4));
	payload.dst_ipv4 = ipv4->hdr.dst_addr;
	payload.src_ipv4 = ipv4->hdr.src_addr;
	memcpy(payload.common.dst_mac, eth->dst_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	memcpy(payload.common.src_mac, eth->src_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	payload.common.port_id = port->in_port;

	return nfp_flower_cmsg_tun_neigh_v4_rule(app_fw_flower, &payload);
}

static int
nfp_flower_add_tun_neigh_v4_decap(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct rte_flow *nfp_flow)
{
	bool exists = false;
	struct nfp_fl_tun *tmp;
	struct nfp_fl_tun *tun;
	struct nfp_flow_priv *priv;
	struct nfp_flower_ipv4 *ipv4;
	struct nfp_flower_mac_mpls *eth;
	struct nfp_flower_in_port *port;
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_flower_cmsg_tun_neigh_v4 payload;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	port = (struct nfp_flower_in_port *)(meta_tci + 1);
	eth = (struct nfp_flower_mac_mpls *)(port + 1);

	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_TP)
		ipv4 = (struct nfp_flower_ipv4 *)((char *)eth +
				sizeof(struct nfp_flower_mac_mpls) +
				sizeof(struct nfp_flower_tp_ports));
	else
		ipv4 = (struct nfp_flower_ipv4 *)((char *)eth +
				sizeof(struct nfp_flower_mac_mpls));

	tun = &nfp_flow->tun;
	tun->payload.v6_flag = 0;
	tun->payload.dst.dst_ipv4 = ipv4->ipv4_src;
	tun->payload.src.src_ipv4 = ipv4->ipv4_dst;
	memcpy(tun->payload.dst_addr, eth->mac_src, RTE_ETHER_ADDR_LEN);
	memcpy(tun->payload.src_addr, eth->mac_dst, RTE_ETHER_ADDR_LEN);

	tun->ref_cnt = 1;
	priv = app_fw_flower->flow_priv;
	LIST_FOREACH(tmp, &priv->nn_list, next) {
		if (memcmp(&tmp->payload, &tun->payload, sizeof(struct nfp_fl_tun_entry)) == 0) {
			tmp->ref_cnt++;
			exists = true;
			break;
		}
	}

	if (exists) {
		if (!nfp_flower_support_decap_v2(app_fw_flower))
			return 0;
	} else {
		LIST_INSERT_HEAD(&priv->nn_list, tun, next);
	}

	memset(&payload, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v4));
	payload.dst_ipv4 = ipv4->ipv4_src;
	payload.src_ipv4 = ipv4->ipv4_dst;
	memcpy(payload.common.dst_mac, eth->mac_src, RTE_ETHER_ADDR_LEN);
	memcpy(payload.common.src_mac, eth->mac_dst, RTE_ETHER_ADDR_LEN);
	payload.common.port_id = port->in_port;

	if (nfp_flower_support_decap_v2(app_fw_flower)) {
		if (meta_tci->tci != 0) {
			payload.ext.vlan_tci = meta_tci->tci;
			payload.ext.vlan_tpid = 0x88a8;
		} else {
			payload.ext.vlan_tci = 0xffff;
			payload.ext.vlan_tpid = 0xffff;
		}
		payload.ext.host_ctx = nfp_flow_meta->host_ctx_id;
	}

	return nfp_flower_cmsg_tun_neigh_v4_rule(app_fw_flower, &payload);
}

static int
nfp_flower_del_tun_neigh_v4(struct nfp_app_fw_flower *app_fw_flower,
		rte_be32_t ipv4)
{
	struct nfp_flower_cmsg_tun_neigh_v4 payload;

	memset(&payload, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v4));
	payload.dst_ipv4 = ipv4;

	return nfp_flower_cmsg_tun_neigh_v4_rule(app_fw_flower, &payload);
}

static int
nfp_flower_add_tun_neigh_v6_encap(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun,
		const struct rte_ether_hdr *eth,
		const struct rte_flow_item_ipv6 *ipv6)
{
	struct nfp_fl_tun *tmp;
	struct nfp_flow_priv *priv;
	struct nfp_flower_in_port *port;
	struct nfp_flower_cmsg_tun_neigh_v6 payload;

	tun->payload.v6_flag = 1;
	memcpy(tun->payload.dst.dst_ipv6, ipv6->hdr.dst_addr, sizeof(tun->payload.dst.dst_ipv6));
	memcpy(tun->payload.src.src_ipv6, ipv6->hdr.src_addr, sizeof(tun->payload.src.src_ipv6));
	memcpy(tun->payload.dst_addr, eth->dst_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	memcpy(tun->payload.src_addr, eth->src_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

	tun->ref_cnt = 1;
	priv = app_fw_flower->flow_priv;
	LIST_FOREACH(tmp, &priv->nn_list, next) {
		if (memcmp(&tmp->payload, &tun->payload, sizeof(struct nfp_fl_tun_entry)) == 0) {
			tmp->ref_cnt++;
			return 0;
		}
	}

	LIST_INSERT_HEAD(&priv->nn_list, tun, next);

	port = (struct nfp_flower_in_port *)((char *)nfp_flow_meta +
			sizeof(struct nfp_fl_rule_metadata) +
			sizeof(struct nfp_flower_meta_tci));

	memset(&payload, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v6));
	memcpy(payload.dst_ipv6, ipv6->hdr.dst_addr, sizeof(payload.dst_ipv6));
	memcpy(payload.src_ipv6, ipv6->hdr.src_addr, sizeof(payload.src_ipv6));
	memcpy(payload.common.dst_mac, eth->dst_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	memcpy(payload.common.src_mac, eth->src_addr.addr_bytes, RTE_ETHER_ADDR_LEN);
	payload.common.port_id = port->in_port;

	return nfp_flower_cmsg_tun_neigh_v6_rule(app_fw_flower, &payload);
}

static int
nfp_flower_add_tun_neigh_v6_decap(struct nfp_app_fw_flower *app_fw_flower,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct rte_flow *nfp_flow)
{
	bool exists = false;
	struct nfp_fl_tun *tmp;
	struct nfp_fl_tun *tun;
	struct nfp_flow_priv *priv;
	struct nfp_flower_ipv6 *ipv6;
	struct nfp_flower_mac_mpls *eth;
	struct nfp_flower_in_port *port;
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_flower_cmsg_tun_neigh_v6 payload;

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	port = (struct nfp_flower_in_port *)(meta_tci + 1);
	eth = (struct nfp_flower_mac_mpls *)(port + 1);

	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_TP)
		ipv6 = (struct nfp_flower_ipv6 *)((char *)eth +
				sizeof(struct nfp_flower_mac_mpls) +
				sizeof(struct nfp_flower_tp_ports));
	else
		ipv6 = (struct nfp_flower_ipv6 *)((char *)eth +
				sizeof(struct nfp_flower_mac_mpls));

	tun = &nfp_flow->tun;
	tun->payload.v6_flag = 1;
	memcpy(tun->payload.dst.dst_ipv6, ipv6->ipv6_src, sizeof(tun->payload.dst.dst_ipv6));
	memcpy(tun->payload.src.src_ipv6, ipv6->ipv6_dst, sizeof(tun->payload.src.src_ipv6));
	memcpy(tun->payload.dst_addr, eth->mac_src, RTE_ETHER_ADDR_LEN);
	memcpy(tun->payload.src_addr, eth->mac_dst, RTE_ETHER_ADDR_LEN);

	tun->ref_cnt = 1;
	priv = app_fw_flower->flow_priv;
	LIST_FOREACH(tmp, &priv->nn_list, next) {
		if (memcmp(&tmp->payload, &tun->payload, sizeof(struct nfp_fl_tun_entry)) == 0) {
			tmp->ref_cnt++;
			exists = true;
			break;
		}
	}

	if (exists) {
		if (!nfp_flower_support_decap_v2(app_fw_flower))
			return 0;
	} else {
		LIST_INSERT_HEAD(&priv->nn_list, tun, next);
	}

	memset(&payload, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v6));
	memcpy(payload.dst_ipv6, ipv6->ipv6_src, sizeof(payload.dst_ipv6));
	memcpy(payload.src_ipv6, ipv6->ipv6_dst, sizeof(payload.src_ipv6));
	memcpy(payload.common.dst_mac, eth->mac_src, RTE_ETHER_ADDR_LEN);
	memcpy(payload.common.src_mac, eth->mac_dst, RTE_ETHER_ADDR_LEN);
	payload.common.port_id = port->in_port;

	if (nfp_flower_support_decap_v2(app_fw_flower)) {
		if (meta_tci->tci != 0) {
			payload.ext.vlan_tci = meta_tci->tci;
			payload.ext.vlan_tpid = 0x88a8;
		} else {
			payload.ext.vlan_tci = 0xffff;
			payload.ext.vlan_tpid = 0xffff;
		}
		payload.ext.host_ctx = nfp_flow_meta->host_ctx_id;
	}

	return nfp_flower_cmsg_tun_neigh_v6_rule(app_fw_flower, &payload);
}

static int
nfp_flower_del_tun_neigh_v6(struct nfp_app_fw_flower *app_fw_flower,
		uint8_t *ipv6)
{
	struct nfp_flower_cmsg_tun_neigh_v6 payload;

	memset(&payload, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v6));
	memcpy(payload.dst_ipv6, ipv6, sizeof(payload.dst_ipv6));

	return nfp_flower_cmsg_tun_neigh_v6_rule(app_fw_flower, &payload);
}

static int
nfp_flower_del_tun_neigh(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_flow *nfp_flow,
		bool decap_flag)
{
	int ret;
	bool flag = false;
	struct nfp_fl_tun *tmp;
	struct nfp_fl_tun *tun;
	struct nfp_flower_in_port *port;

	tun = &nfp_flow->tun;
	LIST_FOREACH(tmp, &app_fw_flower->flow_priv->nn_list, next) {
		ret = memcmp(&tmp->payload, &tun->payload, sizeof(struct nfp_fl_tun_entry));
		if (ret == 0) {
			tmp->ref_cnt--;
			flag = true;
			break;
		}
	}

	if (!flag) {
		PMD_DRV_LOG(DEBUG, "Can't find nn entry in the nn list");
		return -EINVAL;
	}

	if (tmp->ref_cnt == 0) {
		LIST_REMOVE(tmp, next);
		if (tmp->payload.v6_flag != 0) {
			return nfp_flower_del_tun_neigh_v6(app_fw_flower,
					tmp->payload.dst.dst_ipv6);
		} else {
			return nfp_flower_del_tun_neigh_v4(app_fw_flower,
					tmp->payload.dst.dst_ipv4);
		}
	}

	if (!decap_flag)
		return 0;

	port = (struct nfp_flower_in_port *)(nfp_flow->payload.unmasked_data +
			sizeof(struct nfp_fl_rule_metadata) +
			sizeof(struct nfp_flower_meta_tci));

	if (tmp->payload.v6_flag != 0) {
		struct nfp_flower_cmsg_tun_neigh_v6 nn_v6;
		memset(&nn_v6, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v6));
		memcpy(nn_v6.dst_ipv6, tmp->payload.dst.dst_ipv6, sizeof(nn_v6.dst_ipv6));
		memcpy(nn_v6.src_ipv6, tmp->payload.src.src_ipv6, sizeof(nn_v6.src_ipv6));
		memcpy(nn_v6.common.dst_mac, tmp->payload.dst_addr, RTE_ETHER_ADDR_LEN);
		memcpy(nn_v6.common.src_mac, tmp->payload.src_addr, RTE_ETHER_ADDR_LEN);
		nn_v6.common.port_id = port->in_port;

		ret = nfp_flower_cmsg_tun_neigh_v6_rule(app_fw_flower, &nn_v6);
	} else {
		struct nfp_flower_cmsg_tun_neigh_v4 nn_v4;
		memset(&nn_v4, 0, sizeof(struct nfp_flower_cmsg_tun_neigh_v4));
		nn_v4.dst_ipv4 = tmp->payload.dst.dst_ipv4;
		nn_v4.src_ipv4 = tmp->payload.src.src_ipv4;
		memcpy(nn_v4.common.dst_mac, tmp->payload.dst_addr, RTE_ETHER_ADDR_LEN);
		memcpy(nn_v4.common.src_mac, tmp->payload.src_addr, RTE_ETHER_ADDR_LEN);
		nn_v4.common.port_id = port->in_port;

		ret = nfp_flower_cmsg_tun_neigh_v4_rule(app_fw_flower, &nn_v4);
	}

	if (ret != 0) {
		PMD_DRV_LOG(DEBUG, "Failed to send the nn entry");
		return -EINVAL;
	}

	return 0;
}

static int
nfp_flow_action_vxlan_encap_v4(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct vxlan_data *vxlan_data,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	uint64_t tun_id;
	struct nfp_fl_act_pre_tun *pre_tun;
	struct nfp_fl_act_set_tun *set_tun;
	const struct rte_flow_item_eth *eth;
	const struct rte_flow_item_ipv4 *ipv4;
	const struct rte_flow_item_vxlan *vxlan;
	size_t act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
	size_t act_set_size = sizeof(struct nfp_fl_act_set_tun);

	eth   = (const struct rte_flow_item_eth *)vxlan_data->items[0].spec;
	ipv4  = (const struct rte_flow_item_ipv4 *)vxlan_data->items[1].spec;
	vxlan = (const struct rte_flow_item_vxlan *)vxlan_data->items[3].spec;

	pre_tun = (struct nfp_fl_act_pre_tun *)actions;
	memset(pre_tun, 0, act_pre_size);
	nfp_flow_pre_tun_v4_process(pre_tun, ipv4->hdr.dst_addr);

	set_tun = (struct nfp_fl_act_set_tun *)(act_data + act_pre_size);
	memset(set_tun, 0, act_set_size);
	tun_id = rte_be_to_cpu_32(vxlan->hdr.vx_vni);
	nfp_flow_set_tun_process(set_tun, NFP_FL_TUN_VXLAN, tun_id,
			ipv4->hdr.time_to_live, ipv4->hdr.type_of_service);
	set_tun->tun_flags = vxlan->hdr.vx_flags;

	/* Send the tunnel neighbor cmsg to fw */
	return nfp_flower_add_tun_neigh_v4_encap(app_fw_flower, nfp_flow_meta,
			tun, &eth->hdr, ipv4);
}

static int
nfp_flow_action_vxlan_encap_v6(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct vxlan_data *vxlan_data,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	uint64_t tun_id;
	struct nfp_fl_act_pre_tun *pre_tun;
	struct nfp_fl_act_set_tun *set_tun;
	const struct rte_flow_item_eth *eth;
	const struct rte_flow_item_ipv6 *ipv6;
	const struct rte_flow_item_vxlan *vxlan;
	size_t act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
	size_t act_set_size = sizeof(struct nfp_fl_act_set_tun);

	eth   = (const struct rte_flow_item_eth *)vxlan_data->items[0].spec;
	ipv6  = (const struct rte_flow_item_ipv6 *)vxlan_data->items[1].spec;
	vxlan = (const struct rte_flow_item_vxlan *)vxlan_data->items[3].spec;

	pre_tun = (struct nfp_fl_act_pre_tun *)actions;
	memset(pre_tun, 0, act_pre_size);
	nfp_flow_pre_tun_v6_process(pre_tun, ipv6->hdr.dst_addr);

	set_tun = (struct nfp_fl_act_set_tun *)(act_data + act_pre_size);
	memset(set_tun, 0, act_set_size);
	tun_id = rte_be_to_cpu_32(vxlan->hdr.vx_vni);
	nfp_flow_set_tun_process(set_tun, NFP_FL_TUN_VXLAN, tun_id,
			ipv6->hdr.hop_limits,
			(ipv6->hdr.vtc_flow >> RTE_IPV6_HDR_TC_SHIFT) & 0xff);
	set_tun->tun_flags = vxlan->hdr.vx_flags;

	/* Send the tunnel neighbor cmsg to fw */
	return nfp_flower_add_tun_neigh_v6_encap(app_fw_flower, nfp_flow_meta,
			tun, &eth->hdr, ipv6);
}

static int
nfp_flow_action_vxlan_encap(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct rte_flow_action *action,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	size_t act_len;
	size_t act_pre_size;
	const struct vxlan_data *vxlan_data;

	vxlan_data = action->conf;
	if (vxlan_data->items[0].type != RTE_FLOW_ITEM_TYPE_ETH ||
			(vxlan_data->items[1].type != RTE_FLOW_ITEM_TYPE_IPV4 &&
			vxlan_data->items[1].type != RTE_FLOW_ITEM_TYPE_IPV6) ||
			vxlan_data->items[2].type != RTE_FLOW_ITEM_TYPE_UDP ||
			vxlan_data->items[3].type != RTE_FLOW_ITEM_TYPE_VXLAN ||
			vxlan_data->items[4].type != RTE_FLOW_ITEM_TYPE_END) {
		PMD_DRV_LOG(ERR, "Not an valid vxlan action conf.");
		return -EINVAL;
	}

	/*
	 * Pre_tunnel action must be the first on the action list.
	 * If other actions already exist, they need to be pushed forward.
	 */
	act_len = act_data - actions;
	if (act_len != 0) {
		act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
		memmove(actions + act_pre_size, actions, act_len);
	}

	if (vxlan_data->items[1].type == RTE_FLOW_ITEM_TYPE_IPV4)
		return nfp_flow_action_vxlan_encap_v4(app_fw_flower, act_data,
				actions, vxlan_data, nfp_flow_meta, tun);
	else
		return nfp_flow_action_vxlan_encap_v6(app_fw_flower, act_data,
				actions, vxlan_data, nfp_flow_meta, tun);
}

static struct nfp_pre_tun_entry *
nfp_pre_tun_table_search(struct nfp_flow_priv *priv,
		char *hash_data,
		uint32_t hash_len)
{
	int index;
	uint32_t hash_key;
	struct nfp_pre_tun_entry *mac_index;

	hash_key = rte_jhash(hash_data, hash_len, priv->hash_seed);
	index = rte_hash_lookup_data(priv->pre_tun_table, &hash_key, (void **)&mac_index);
	if (index < 0) {
		PMD_DRV_LOG(DEBUG, "Data NOT found in the hash table");
		return NULL;
	}

	return mac_index;
}

static bool
nfp_pre_tun_table_add(struct nfp_flow_priv *priv,
		char *hash_data,
		uint32_t hash_len)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(hash_data, hash_len, priv->hash_seed);
	ret = rte_hash_add_key_data(priv->pre_tun_table, &hash_key, hash_data);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Add to pre tunnel table failed");
		return false;
	}

	return true;
}

static bool
nfp_pre_tun_table_delete(struct nfp_flow_priv *priv,
		char *hash_data,
		uint32_t hash_len)
{
	int ret;
	uint32_t hash_key;

	hash_key = rte_jhash(hash_data, hash_len, priv->hash_seed);
	ret = rte_hash_del_key(priv->pre_tun_table, &hash_key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Delete from pre tunnel table failed");
		return false;
	}

	return true;
}

static int
nfp_pre_tun_table_check_add(struct nfp_flower_representor *repr,
		uint16_t *index)
{
	uint16_t i;
	uint32_t entry_size;
	uint16_t mac_index = 1;
	struct nfp_flow_priv *priv;
	struct nfp_pre_tun_entry *entry;
	struct nfp_pre_tun_entry *find_entry;

	priv = repr->app_fw_flower->flow_priv;
	if (priv->pre_tun_cnt >= NFP_TUN_PRE_TUN_RULE_LIMIT) {
		PMD_DRV_LOG(ERR, "Pre tunnel table has full");
		return -EINVAL;
	}

	entry_size = sizeof(struct nfp_pre_tun_entry);
	entry = rte_zmalloc("nfp_pre_tun", entry_size, 0);
	if (entry == NULL) {
		PMD_DRV_LOG(ERR, "Memory alloc failed for pre tunnel table");
		return -ENOMEM;
	}

	entry->ref_cnt = 1U;
	memcpy(entry->mac_addr, repr->mac_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

	/* 0 is considered a failed match */
	for (i = 1; i < NFP_TUN_PRE_TUN_RULE_LIMIT; i++) {
		if (priv->pre_tun_bitmap[i] == 0)
			continue;
		entry->mac_index = i;
		find_entry = nfp_pre_tun_table_search(priv, (char *)entry, entry_size);
		if (find_entry != NULL) {
			find_entry->ref_cnt++;
			*index = find_entry->mac_index;
			rte_free(entry);
			return 0;
		}
	}

	for (i = 1; i < NFP_TUN_PRE_TUN_RULE_LIMIT; i++) {
		if (priv->pre_tun_bitmap[i] == 0) {
			priv->pre_tun_bitmap[i] = 1U;
			mac_index = i;
			break;
		}
	}

	entry->mac_index = mac_index;
	if (!nfp_pre_tun_table_add(priv, (char *)entry, entry_size)) {
		rte_free(entry);
		return -EINVAL;
	}

	*index = entry->mac_index;
	priv->pre_tun_cnt++;
	return 0;
}

static int
nfp_pre_tun_table_check_del(struct nfp_flower_representor *repr,
		struct rte_flow *nfp_flow)
{
	uint16_t i;
	int ret = 0;
	uint32_t entry_size;
	uint16_t nfp_mac_idx;
	struct nfp_flow_priv *priv;
	struct nfp_pre_tun_entry *entry;
	struct nfp_pre_tun_entry *find_entry;
	struct nfp_fl_rule_metadata *nfp_flow_meta;

	priv = repr->app_fw_flower->flow_priv;
	if (priv->pre_tun_cnt == 1)
		return 0;

	entry_size = sizeof(struct nfp_pre_tun_entry);
	entry = rte_zmalloc("nfp_pre_tun", entry_size, 0);
	if (entry == NULL) {
		PMD_DRV_LOG(ERR, "Memory alloc failed for pre tunnel table");
		return -ENOMEM;
	}

	entry->ref_cnt = 1U;
	memcpy(entry->mac_addr, repr->mac_addr.addr_bytes, RTE_ETHER_ADDR_LEN);

	/* 0 is considered a failed match */
	for (i = 1; i < NFP_TUN_PRE_TUN_RULE_LIMIT; i++) {
		if (priv->pre_tun_bitmap[i] == 0)
			continue;
		entry->mac_index = i;
		find_entry = nfp_pre_tun_table_search(priv, (char *)entry, entry_size);
		if (find_entry != NULL) {
			find_entry->ref_cnt--;
			if (find_entry->ref_cnt != 0)
				goto free_entry;
			priv->pre_tun_bitmap[i] = 0;
			break;
		}
	}

	nfp_flow_meta = nfp_flow->payload.meta;
	nfp_mac_idx = (find_entry->mac_index << 8) |
			NFP_FLOWER_CMSG_PORT_TYPE_OTHER_PORT |
			NFP_TUN_PRE_TUN_IDX_BIT;
	if (nfp_flow->tun.payload.v6_flag != 0)
		nfp_mac_idx |= NFP_TUN_PRE_TUN_IPV6_BIT;

	ret = nfp_flower_cmsg_tun_mac_rule(repr->app_fw_flower, &repr->mac_addr,
			nfp_mac_idx, true);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Send tunnel mac rule failed");
		ret = -EINVAL;
		goto free_entry;
	}

	if (!nfp_flower_support_decap_v2(repr->app_fw_flower)) {
		ret = nfp_flower_cmsg_pre_tunnel_rule(repr->app_fw_flower, nfp_flow_meta,
				nfp_mac_idx, true);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Send pre tunnel rule failed");
			ret = -EINVAL;
			goto free_entry;
		}
	}

	find_entry->ref_cnt = 1U;
	if (!nfp_pre_tun_table_delete(priv, (char *)find_entry, entry_size)) {
		PMD_DRV_LOG(ERR, "Delete entry from pre tunnel table failed");
		ret = -EINVAL;
		goto free_entry;
	}

	rte_free(entry);
	rte_free(find_entry);
	priv->pre_tun_cnt--;

free_entry:
	rte_free(entry);

	return ret;
}

static int
nfp_flow_action_tunnel_decap(struct nfp_flower_representor *repr,
		const struct rte_flow_action *action,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct rte_flow *nfp_flow)
{
	int ret;
	uint16_t nfp_mac_idx = 0;
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_app_fw_flower *app_fw_flower;

	ret = nfp_pre_tun_table_check_add(repr, &nfp_mac_idx);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Pre tunnel table add failed");
		return -EINVAL;
	}

	nfp_mac_idx = (nfp_mac_idx << 8) |
			NFP_FLOWER_CMSG_PORT_TYPE_OTHER_PORT |
			NFP_TUN_PRE_TUN_IDX_BIT;
	if (action->conf != NULL)
		nfp_mac_idx |= NFP_TUN_PRE_TUN_IPV6_BIT;

	app_fw_flower = repr->app_fw_flower;
	ret = nfp_flower_cmsg_tun_mac_rule(app_fw_flower, &repr->mac_addr,
			nfp_mac_idx, false);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Send tunnel mac rule failed");
		return -EINVAL;
	}

	if (!nfp_flower_support_decap_v2(app_fw_flower)) {
		ret = nfp_flower_cmsg_pre_tunnel_rule(app_fw_flower, nfp_flow_meta,
				nfp_mac_idx, false);
		if (ret != 0) {
			PMD_DRV_LOG(ERR, "Send pre tunnel rule failed");
			return -EINVAL;
		}
	}

	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;
	if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_IPV4)
		return nfp_flower_add_tun_neigh_v4_decap(app_fw_flower, nfp_flow_meta, nfp_flow);
	else
		return nfp_flower_add_tun_neigh_v6_decap(app_fw_flower, nfp_flow_meta, nfp_flow);
}

static int
nfp_flow_action_geneve_encap_v4(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct rte_flow_action_raw_encap *raw_encap,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	uint64_t tun_id;
	const struct rte_ether_hdr *eth;
	const struct rte_flow_item_udp *udp;
	const struct rte_flow_item_ipv4 *ipv4;
	const struct rte_flow_item_geneve *geneve;
	struct nfp_fl_act_pre_tun *pre_tun;
	struct nfp_fl_act_set_tun *set_tun;
	size_t act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
	size_t act_set_size = sizeof(struct nfp_fl_act_set_tun);

	eth    = (const struct rte_ether_hdr *)raw_encap->data;
	ipv4   = (const struct rte_flow_item_ipv4 *)(eth + 1);
	udp    = (const struct rte_flow_item_udp *)(ipv4 + 1);
	geneve = (const struct rte_flow_item_geneve *)(udp + 1);

	pre_tun = (struct nfp_fl_act_pre_tun *)actions;
	memset(pre_tun, 0, act_pre_size);
	nfp_flow_pre_tun_v4_process(pre_tun, ipv4->hdr.dst_addr);

	set_tun = (struct nfp_fl_act_set_tun *)(act_data + act_pre_size);
	memset(set_tun, 0, act_set_size);
	tun_id = (geneve->vni[0] << 16) | (geneve->vni[1] << 8) | geneve->vni[2];
	nfp_flow_set_tun_process(set_tun, NFP_FL_TUN_GENEVE, tun_id,
			ipv4->hdr.time_to_live, ipv4->hdr.type_of_service);
	set_tun->tun_proto = geneve->protocol;

	/* Send the tunnel neighbor cmsg to fw */
	return nfp_flower_add_tun_neigh_v4_encap(app_fw_flower, nfp_flow_meta,
			tun, eth, ipv4);
}

static int
nfp_flow_action_geneve_encap_v6(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct rte_flow_action_raw_encap *raw_encap,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	uint8_t tos;
	uint64_t tun_id;
	const struct rte_ether_hdr *eth;
	const struct rte_flow_item_udp *udp;
	const struct rte_flow_item_ipv6 *ipv6;
	const struct rte_flow_item_geneve *geneve;
	struct nfp_fl_act_pre_tun *pre_tun;
	struct nfp_fl_act_set_tun *set_tun;
	size_t act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
	size_t act_set_size = sizeof(struct nfp_fl_act_set_tun);

	eth    = (const struct rte_ether_hdr *)raw_encap->data;
	ipv6   = (const struct rte_flow_item_ipv6 *)(eth + 1);
	udp    = (const struct rte_flow_item_udp *)(ipv6 + 1);
	geneve = (const struct rte_flow_item_geneve *)(udp + 1);

	pre_tun = (struct nfp_fl_act_pre_tun *)actions;
	memset(pre_tun, 0, act_pre_size);
	nfp_flow_pre_tun_v6_process(pre_tun, ipv6->hdr.dst_addr);

	set_tun = (struct nfp_fl_act_set_tun *)(act_data + act_pre_size);
	memset(set_tun, 0, act_set_size);
	tos = (ipv6->hdr.vtc_flow >> RTE_IPV6_HDR_TC_SHIFT) & 0xff;
	tun_id = (geneve->vni[0] << 16) | (geneve->vni[1] << 8) | geneve->vni[2];
	nfp_flow_set_tun_process(set_tun, NFP_FL_TUN_GENEVE, tun_id,
			ipv6->hdr.hop_limits, tos);
	set_tun->tun_proto = geneve->protocol;

	/* Send the tunnel neighbor cmsg to fw */
	return nfp_flower_add_tun_neigh_v6_encap(app_fw_flower, nfp_flow_meta,
			tun, eth, ipv6);
}

static int
nfp_flow_action_nvgre_encap_v4(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct rte_flow_action_raw_encap *raw_encap,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	const struct rte_ether_hdr *eth;
	const struct rte_flow_item_ipv4 *ipv4;
	const struct rte_flow_item_gre *gre;
	struct nfp_fl_act_pre_tun *pre_tun;
	struct nfp_fl_act_set_tun *set_tun;
	size_t act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
	size_t act_set_size = sizeof(struct nfp_fl_act_set_tun);

	eth  = (const struct rte_ether_hdr *)raw_encap->data;
	ipv4 = (const struct rte_flow_item_ipv4 *)(eth + 1);
	gre  = (const struct rte_flow_item_gre *)(ipv4 + 1);

	pre_tun = (struct nfp_fl_act_pre_tun *)actions;
	memset(pre_tun, 0, act_pre_size);
	nfp_flow_pre_tun_v4_process(pre_tun, ipv4->hdr.dst_addr);

	set_tun = (struct nfp_fl_act_set_tun *)(act_data + act_pre_size);
	memset(set_tun, 0, act_set_size);
	nfp_flow_set_tun_process(set_tun, NFP_FL_TUN_GRE, 0,
			ipv4->hdr.time_to_live, ipv4->hdr.type_of_service);
	set_tun->tun_proto = gre->protocol;

	/* Send the tunnel neighbor cmsg to fw */
	return nfp_flower_add_tun_neigh_v4_encap(app_fw_flower, nfp_flow_meta,
			tun, eth, ipv4);
}

static int
nfp_flow_action_nvgre_encap_v6(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct rte_flow_action_raw_encap *raw_encap,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	uint8_t tos;
	const struct rte_ether_hdr *eth;
	const struct rte_flow_item_ipv6 *ipv6;
	const struct rte_flow_item_gre *gre;
	struct nfp_fl_act_pre_tun *pre_tun;
	struct nfp_fl_act_set_tun *set_tun;
	size_t act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
	size_t act_set_size = sizeof(struct nfp_fl_act_set_tun);

	eth    = (const struct rte_ether_hdr *)raw_encap->data;
	ipv6   = (const struct rte_flow_item_ipv6 *)(eth + 1);
	gre    = (const struct rte_flow_item_gre *)(ipv6 + 1);

	pre_tun = (struct nfp_fl_act_pre_tun *)actions;
	memset(pre_tun, 0, act_pre_size);
	nfp_flow_pre_tun_v6_process(pre_tun, ipv6->hdr.dst_addr);

	set_tun = (struct nfp_fl_act_set_tun *)(act_data + act_pre_size);
	memset(set_tun, 0, act_set_size);
	tos = (ipv6->hdr.vtc_flow >> RTE_IPV6_HDR_TC_SHIFT) & 0xff;
	nfp_flow_set_tun_process(set_tun, NFP_FL_TUN_GRE, 0,
			ipv6->hdr.hop_limits, tos);
	set_tun->tun_proto = gre->protocol;

	/* Send the tunnel neighbor cmsg to fw */
	return nfp_flower_add_tun_neigh_v6_encap(app_fw_flower, nfp_flow_meta,
			tun, eth, ipv6);
}

static int
nfp_flow_action_raw_encap(struct nfp_app_fw_flower *app_fw_flower,
		char *act_data,
		char *actions,
		const struct rte_flow_action *action,
		struct nfp_fl_rule_metadata *nfp_flow_meta,
		struct nfp_fl_tun *tun)
{
	int ret;
	size_t act_len;
	size_t act_pre_size;
	const struct rte_flow_action_raw_encap *raw_encap;

	raw_encap = action->conf;
	if (raw_encap->data == NULL) {
		PMD_DRV_LOG(ERR, "The raw encap action conf is NULL.");
		return -EINVAL;
	}

	/* Pre_tunnel action must be the first on action list.
	 * If other actions already exist, they need to be
	 * pushed forward.
	 */
	act_len = act_data - actions;
	if (act_len != 0) {
		act_pre_size = sizeof(struct nfp_fl_act_pre_tun);
		memmove(actions + act_pre_size, actions, act_len);
	}

	switch (raw_encap->size) {
	case GENEVE_V4_LEN:
		ret = nfp_flow_action_geneve_encap_v4(app_fw_flower, act_data,
				actions, raw_encap, nfp_flow_meta, tun);
		break;
	case GENEVE_V6_LEN:
		ret = nfp_flow_action_geneve_encap_v6(app_fw_flower, act_data,
				actions, raw_encap, nfp_flow_meta, tun);
		break;
	case NVGRE_V4_LEN:
		ret = nfp_flow_action_nvgre_encap_v4(app_fw_flower, act_data,
				actions, raw_encap, nfp_flow_meta, tun);
		break;
	case NVGRE_V6_LEN:
		ret = nfp_flow_action_nvgre_encap_v6(app_fw_flower, act_data,
				actions, raw_encap, nfp_flow_meta, tun);
		break;
	default:
		PMD_DRV_LOG(ERR, "Not an valid raw encap action conf.");
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int
nfp_flow_action_meter(struct nfp_flower_representor *representor,
		const struct rte_flow_action *action,
		char *act_data,
		uint32_t *mtr_id)
{
	struct nfp_mtr *mtr;
	struct nfp_fl_act_meter *fl_meter;
	struct nfp_app_fw_flower *app_fw_flower;
	const struct rte_flow_action_meter *meter;
	size_t act_size = sizeof(struct nfp_fl_act_meter);

	meter = action->conf;
	fl_meter = (struct nfp_fl_act_meter *)act_data;
	app_fw_flower = representor->app_fw_flower;

	mtr = nfp_mtr_find_by_mtr_id(app_fw_flower->mtr_priv, meter->mtr_id);
	if (mtr == NULL) {
		PMD_DRV_LOG(ERR, "Meter id not exist");
		return -EINVAL;
	}

	if (!mtr->enable) {
		PMD_DRV_LOG(ERR, "Requested meter disable");
		return -EINVAL;
	}

	if (!mtr->shared && mtr->ref_cnt > 0) {
		PMD_DRV_LOG(ERR, "Can't use a used unshared meter");
		return -EINVAL;
	}

	*mtr_id = meter->mtr_id;

	fl_meter->head.jump_id = NFP_FL_ACTION_OPCODE_METER;
	fl_meter->head.len_lw = act_size >> NFP_FL_LW_SIZ;
	fl_meter->reserved = 0;
	fl_meter->profile_id = rte_cpu_to_be_32(mtr->mtr_profile->profile_id);

	return 0;
}

static uint32_t
nfp_flow_count_output(const struct rte_flow_action actions[])
{
	uint32_t count = 0;
	const struct rte_flow_action *action;

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; ++action) {
		if (action->type == RTE_FLOW_ACTION_TYPE_PORT_ID)
			count++;
	}

	return count;
}

static int
nfp_flow_compile_action(struct nfp_flower_representor *representor,
		const struct rte_flow_action actions[],
		struct rte_flow *nfp_flow)
{
	int ret = 0;
	uint32_t count;
	char *position;
	char *action_data;
	bool ttl_tos_flag = false;
	bool tc_hl_flag = false;
	bool drop_flag = false;
	bool ip_set_flag = false;
	bool tp_set_flag = false;
	bool mac_set_flag = false;
	uint32_t total_actions = 0;
	const struct rte_flow_action *action;
	struct nfp_flower_meta_tci *meta_tci;
	struct nfp_fl_rule_metadata *nfp_flow_meta;

	nfp_flow_meta = nfp_flow->payload.meta;
	action_data   = nfp_flow->payload.action_data;
	position      = action_data;
	meta_tci = (struct nfp_flower_meta_tci *)nfp_flow->payload.unmasked_data;

	count = nfp_flow_count_output(actions);

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; ++action) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_DROP");
			drop_flag = true;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_COUNT");
			break;
		case RTE_FLOW_ACTION_TYPE_JUMP:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_JUMP");
			break;
		case RTE_FLOW_ACTION_TYPE_PORT_ID:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_PORT_ID");
			count--;
			ret = nfp_flow_action_output(position, action, nfp_flow_meta, count);
			if (ret != 0) {
				PMD_DRV_LOG(ERR, "Failed when process"
						" RTE_FLOW_ACTION_TYPE_PORT_ID");
				return ret;
			}

			position += sizeof(struct nfp_fl_act_output);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_SRC:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_MAC_SRC");
			nfp_flow_action_set_mac(position, action, true, mac_set_flag);
			if (!mac_set_flag) {
				position += sizeof(struct nfp_fl_act_set_eth);
				mac_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_MAC_DST:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_MAC_DST");
			nfp_flow_action_set_mac(position, action, false, mac_set_flag);
			if (!mac_set_flag) {
				position += sizeof(struct nfp_fl_act_set_eth);
				mac_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_OF_POP_VLAN:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_OF_POP_VLAN");
			nfp_flow_action_pop_vlan(position, nfp_flow_meta);
			position += sizeof(struct nfp_fl_act_pop_vlan);
			break;
		case RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN");
			ret = nfp_flow_action_push_vlan(position, action);
			if (ret != 0) {
				PMD_DRV_LOG(ERR, "Failed when process"
						" RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN");
				return ret;
			}

			/*
			 * RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP and
			 * RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID
			 * have also been processed.
			 */
			action += 2;
			position += sizeof(struct nfp_fl_act_push_vlan);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_IPV4_SRC");
			nfp_flow_action_set_ip(position, action, true, ip_set_flag);
			if (!ip_set_flag) {
				position += sizeof(struct nfp_fl_act_set_ip4_addrs);
				ip_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DST:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_IPV4_DST");
			nfp_flow_action_set_ip(position, action, false, ip_set_flag);
			if (!ip_set_flag) {
				position += sizeof(struct nfp_fl_act_set_ip4_addrs);
				ip_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_IPV6_SRC");
			nfp_flow_action_set_ipv6(position, action, true);
			position += sizeof(struct nfp_fl_act_set_ipv6_addr);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DST:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_IPV6_DST");
			nfp_flow_action_set_ipv6(position, action, false);
			position += sizeof(struct nfp_fl_act_set_ipv6_addr);
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_SRC:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_TP_SRC");
			nfp_flow_action_set_tp(position, action, true, tp_set_flag);
			if (!tp_set_flag) {
				position += sizeof(struct nfp_fl_act_set_tport);
				tp_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TP_DST:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_TP_DST");
			nfp_flow_action_set_tp(position, action, false, tp_set_flag);
			if (!tp_set_flag) {
				position += sizeof(struct nfp_fl_act_set_tport);
				tp_set_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_TTL:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_TTL");
			if (meta_tci->nfp_flow_key_layer & NFP_FLOWER_LAYER_IPV4) {
				nfp_flow_action_set_ttl(position, action, ttl_tos_flag);
				if (!ttl_tos_flag) {
					position += sizeof(struct nfp_fl_act_set_ip4_ttl_tos);
					ttl_tos_flag = true;
				}
			} else {
				nfp_flow_action_set_hl(position, action, ttl_tos_flag);
				if (!tc_hl_flag) {
					position += sizeof(struct nfp_fl_act_set_ipv6_tc_hl_fl);
					tc_hl_flag = true;
				}
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_IPV4_DSCP");
			nfp_flow_action_set_tos(position, action, ttl_tos_flag);
			if (!ttl_tos_flag) {
				position += sizeof(struct nfp_fl_act_set_ip4_ttl_tos);
				ttl_tos_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_SET_IPV6_DSCP");
			nfp_flow_action_set_tc(position, action, ttl_tos_flag);
			if (!tc_hl_flag) {
				position += sizeof(struct nfp_fl_act_set_ipv6_tc_hl_fl);
				tc_hl_flag = true;
			}
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP");
			ret = nfp_flow_action_vxlan_encap(representor->app_fw_flower,
					position, action_data, action, nfp_flow_meta,
					&nfp_flow->tun);
			if (ret != 0) {
				PMD_DRV_LOG(ERR, "Failed when process"
						" RTE_FLOW_ACTION_TYPE_VXLAN_ENCAP");
				return ret;
			}
			position += sizeof(struct nfp_fl_act_pre_tun);
			position += sizeof(struct nfp_fl_act_set_tun);
			nfp_flow->type = NFP_FLOW_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_RAW_ENCAP:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_RAW_ENCAP");
			ret = nfp_flow_action_raw_encap(representor->app_fw_flower,
					position, action_data, action, nfp_flow_meta,
					&nfp_flow->tun);
			if (ret != 0) {
				PMD_DRV_LOG(ERR, "Failed when process"
						" RTE_FLOW_ACTION_TYPE_RAW_ENCAP");
				return ret;
			}
			position += sizeof(struct nfp_fl_act_pre_tun);
			position += sizeof(struct nfp_fl_act_set_tun);
			nfp_flow->type = NFP_FLOW_ENCAP;
			break;
		case RTE_FLOW_ACTION_TYPE_VXLAN_DECAP:
		case RTE_FLOW_ACTION_TYPE_RAW_DECAP:
			PMD_DRV_LOG(DEBUG, "process action tunnel decap");
			ret = nfp_flow_action_tunnel_decap(representor, action,
					nfp_flow_meta, nfp_flow);
			if (ret != 0) {
				PMD_DRV_LOG(ERR, "Failed when process tunnel decap");
				return ret;
			}
			nfp_flow->type = NFP_FLOW_DECAP;
			nfp_flow->install_flag = false;
			if (action->conf != NULL)
				nfp_flow->tun.payload.v6_flag = 1;
			break;
		case RTE_FLOW_ACTION_TYPE_METER:
			PMD_DRV_LOG(DEBUG, "Process RTE_FLOW_ACTION_TYPE_METER");
			ret = nfp_flow_action_meter(representor, action,
					position, &nfp_flow->mtr_id);
			if (ret != 0)
				return -EINVAL;
			position += sizeof(struct nfp_fl_act_meter);
			break;
		default:
			PMD_DRV_LOG(ERR, "Unsupported action type: %d", action->type);
			return -ENOTSUP;
		}
		total_actions++;
	}

	if (drop_flag)
		nfp_flow_meta->shortcut = rte_cpu_to_be_32(NFP_FL_SC_ACT_DROP);
	else if (total_actions > 1)
		nfp_flow_meta->shortcut = rte_cpu_to_be_32(NFP_FL_SC_ACT_NULL);

	return 0;
}

static struct rte_flow *
nfp_flow_process(struct nfp_flower_representor *representor,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		bool validate_flag)
{
	int ret;
	char *hash_data;
	char *mask_data;
	uint32_t mask_len;
	uint32_t stats_ctx = 0;
	uint8_t new_mask_id = 0;
	struct rte_flow *nfp_flow;
	struct rte_flow *flow_find;
	struct nfp_flow_priv *priv;
	struct nfp_fl_key_ls key_layer;
	struct nfp_fl_rule_metadata *nfp_flow_meta;

	ret = nfp_flow_key_layers_calculate(items, actions, &key_layer);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Key layers calculate failed.");
		return NULL;
	}

	if (key_layer.port == (uint32_t)~0)
		key_layer.port = representor->port_id;

	priv = representor->app_fw_flower->flow_priv;
	ret = nfp_stats_id_alloc(priv, &stats_ctx);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "nfp stats id alloc failed.");
		return NULL;
	}

	nfp_flow = nfp_flow_alloc(&key_layer, representor->port_id);
	if (nfp_flow == NULL) {
		PMD_DRV_LOG(ERR, "Alloc nfp flow failed.");
		goto free_stats;
	}

	nfp_flow->install_flag = true;

	nfp_flow_compile_metadata(priv, nfp_flow, &key_layer, stats_ctx);

	ret = nfp_flow_compile_items(representor, items, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "nfp flow item process failed.");
		goto free_flow;
	}

	ret = nfp_flow_compile_action(representor, actions, nfp_flow);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "nfp flow action process failed.");
		goto free_flow;
	}

	nfp_flow_meta = nfp_flow->payload.meta;
	mask_data = nfp_flow->payload.mask_data;
	mask_len = key_layer.key_size;
	if (!nfp_check_mask_add(priv, mask_data, mask_len,
			&nfp_flow_meta->flags, &new_mask_id)) {
		PMD_DRV_LOG(ERR, "nfp mask add check failed.");
		goto free_flow;
	}

	/* Once we have a mask_id, update the meta tci */
	nfp_flower_update_meta_tci(nfp_flow->payload.unmasked_data, new_mask_id);

	/* Calculate and store the hash_key for later use */
	hash_data = (char *)(nfp_flow->payload.unmasked_data);
	nfp_flow->hash_key = rte_jhash(hash_data, nfp_flow->length, priv->hash_seed);

	/* Find the flow in hash table */
	flow_find = nfp_flow_table_search(priv, nfp_flow);
	if (flow_find != NULL) {
		PMD_DRV_LOG(ERR, "This flow is already exist.");
		if (!nfp_check_mask_remove(priv, mask_data, mask_len,
				&nfp_flow_meta->flags)) {
			PMD_DRV_LOG(ERR, "nfp mask del check failed.");
		}
		goto free_flow;
	}

	/* Flow validate should not update the flower version */
	if (!validate_flag)
		priv->flower_version++;

	return nfp_flow;

free_flow:
	nfp_flow_free(nfp_flow);
free_stats:
	nfp_stats_id_free(priv, stats_ctx);

	return NULL;
}

static struct rte_flow *
nfp_flow_setup(struct nfp_flower_representor *representor,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		__rte_unused struct rte_flow_error *error,
		bool validate_flag)
{
	if (attr->group != 0)
		PMD_DRV_LOG(INFO, "Pretend we support group attribute.");

	if (attr->priority != 0)
		PMD_DRV_LOG(INFO, "Pretend we support priority attribute.");

	if (attr->transfer != 0)
		PMD_DRV_LOG(INFO, "Pretend we support transfer attribute.");

	return nfp_flow_process(representor, items, actions, validate_flag);
}

static int
nfp_flow_teardown(struct nfp_flow_priv *priv,
		struct rte_flow *nfp_flow,
		bool validate_flag)
{
	char *mask_data;
	uint32_t mask_len;
	uint32_t stats_ctx;
	struct nfp_fl_rule_metadata *nfp_flow_meta;

	nfp_flow_meta = nfp_flow->payload.meta;
	mask_data = nfp_flow->payload.mask_data;
	mask_len = nfp_flow_meta->mask_len << NFP_FL_LW_SIZ;
	nfp_flow_meta->flags &= ~NFP_FL_META_FLAG_MANAGE_MASK;
	if (!nfp_check_mask_remove(priv, mask_data, mask_len,
			&nfp_flow_meta->flags)) {
		PMD_DRV_LOG(ERR, "nfp mask del check failed.");
		return -EINVAL;
	}

	nfp_flow_meta->flow_version = rte_cpu_to_be_64(priv->flower_version);

	/* Flow validate should not update the flower version */
	if (!validate_flag)
		priv->flower_version++;

	stats_ctx = rte_be_to_cpu_32(nfp_flow_meta->host_ctx_id);
	return nfp_stats_id_free(priv, stats_ctx);
}

static int
nfp_flow_validate(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	int ret;
	struct rte_flow *nfp_flow;
	struct nfp_flow_priv *priv;
	struct nfp_flower_representor *representor;

	representor = (struct nfp_flower_representor *)dev->data->dev_private;
	priv = representor->app_fw_flower->flow_priv;

	nfp_flow = nfp_flow_setup(representor, attr, items, actions, error, true);
	if (nfp_flow == NULL) {
		return rte_flow_error_set(error, ENOTSUP,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "This flow can not be offloaded.");
	}

	ret = nfp_flow_teardown(priv, nfp_flow, true);
	if (ret != 0) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow resource free failed.");
	}

	nfp_flow_free(nfp_flow);

	return 0;
}

static struct rte_flow *
nfp_flow_create(struct rte_eth_dev *dev,
		const struct rte_flow_attr *attr,
		const struct rte_flow_item items[],
		const struct rte_flow_action actions[],
		struct rte_flow_error *error)
{
	int ret;
	struct rte_flow *nfp_flow;
	struct nfp_flow_priv *priv;
	struct nfp_app_fw_flower *app_fw_flower;
	struct nfp_flower_representor *representor;

	representor = (struct nfp_flower_representor *)dev->data->dev_private;
	app_fw_flower = representor->app_fw_flower;
	priv = app_fw_flower->flow_priv;

	nfp_flow = nfp_flow_setup(representor, attr, items, actions, error, false);
	if (nfp_flow == NULL) {
		rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "This flow can not be offloaded.");
		return NULL;
	}

	/* Add the flow to hardware */
	if (nfp_flow->install_flag) {
		ret = nfp_flower_cmsg_flow_add(app_fw_flower, nfp_flow);
		if (ret != 0) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "Add flow to firmware failed.");
			goto flow_teardown;
		}
	}

	/* Add the flow to flow hash table */
	ret = nfp_flow_table_add(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Add flow to the flow table failed.");
		goto flow_teardown;
	}

	/* Update meter object ref count */
	if (nfp_flow->mtr_id != NFP_MAX_MTR_CNT) {
		ret = nfp_mtr_update_ref_cnt(app_fw_flower->mtr_priv,
				nfp_flow->mtr_id, true);
		if (ret != 0) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "Update meter ref_cnt failed.");
			goto flow_teardown;
		}
	}

	return nfp_flow;

flow_teardown:
	nfp_flow_teardown(priv, nfp_flow, false);
	nfp_flow_free(nfp_flow);

	return NULL;
}

static int
nfp_flow_destroy(struct rte_eth_dev *dev,
		struct rte_flow *nfp_flow,
		struct rte_flow_error *error)
{
	int ret;
	struct rte_flow *flow_find;
	struct nfp_flow_priv *priv;
	struct nfp_app_fw_flower *app_fw_flower;
	struct nfp_flower_representor *representor;

	representor = (struct nfp_flower_representor *)dev->data->dev_private;
	app_fw_flower = representor->app_fw_flower;
	priv = app_fw_flower->flow_priv;

	/* Find the flow in flow hash table */
	flow_find = nfp_flow_table_search(priv, nfp_flow);
	if (flow_find == NULL) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow does not exist.");
		ret = -EINVAL;
		goto exit;
	}

	/* Update flow */
	ret = nfp_flow_teardown(priv, nfp_flow, false);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Flow teardown failed.");
		ret = -EINVAL;
		goto exit;
	}

	switch (nfp_flow->type) {
	case NFP_FLOW_COMMON:
		break;
	case NFP_FLOW_ENCAP:
		/* Delete the entry from nn table */
		ret = nfp_flower_del_tun_neigh(app_fw_flower, nfp_flow, false);
		break;
	case NFP_FLOW_DECAP:
		/* Delete the entry from nn table */
		ret = nfp_flower_del_tun_neigh(app_fw_flower, nfp_flow, true);
		if (ret != 0)
			goto exit;

		/* Delete the entry in pre tunnel table */
		ret = nfp_pre_tun_table_check_del(representor, nfp_flow);
		break;
	default:
		PMD_DRV_LOG(ERR, "Invalid nfp flow type %d.", nfp_flow->type);
		ret = -EINVAL;
		break;
	}

	if (ret != 0)
		goto exit;

	/* Delete the ip off */
	if (nfp_flow_is_tunnel(nfp_flow))
		nfp_tun_check_ip_off_del(representor, nfp_flow);

	/* Delete the flow from hardware */
	if (nfp_flow->install_flag) {
		ret = nfp_flower_cmsg_flow_delete(app_fw_flower, nfp_flow);
		if (ret != 0) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "Delete flow from firmware failed.");
			ret = -EINVAL;
			goto exit;
		}
	}

	/* Delete the flow from flow hash table */
	ret = nfp_flow_table_delete(priv, nfp_flow);
	if (ret != 0) {
		rte_flow_error_set(error, EINVAL, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
				NULL, "Delete flow from the flow table failed.");
		ret = -EINVAL;
		goto exit;
	}

	/* Update meter object ref count */
	if (nfp_flow->mtr_id != NFP_MAX_MTR_CNT) {
		ret = nfp_mtr_update_ref_cnt(app_fw_flower->mtr_priv,
				nfp_flow->mtr_id, false);
		if (ret != 0) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "Update meter ref_cnt failed.");
		}
	}

exit:
	nfp_flow_free(nfp_flow);

	return ret;
}

static int
nfp_flow_flush(struct rte_eth_dev *dev,
		struct rte_flow_error *error)
{
	int ret = 0;
	void *next_data;
	uint32_t iter = 0;
	const void *next_key;
	struct rte_flow *nfp_flow;
	struct nfp_flow_priv *priv;
	struct nfp_flower_representor *representor;

	representor = dev->data->dev_private;

	priv = nfp_flow_dev_to_priv(dev);

	while (rte_hash_iterate(priv->flow_table, &next_key, &next_data, &iter) >= 0) {
		nfp_flow = next_data;
		if (nfp_flow->port_id == representor->port_id) {
			ret = nfp_flow_destroy(dev, nfp_flow, error);
			if (ret != 0)
				break;
		}
	}

	return ret;
}

static void
nfp_flow_stats_get(struct rte_eth_dev *dev,
		struct rte_flow *nfp_flow,
		void *data)
{
	bool reset;
	uint32_t ctx_id;
	struct rte_flow *flow;
	struct nfp_flow_priv *priv;
	struct nfp_fl_stats *stats;
	struct rte_flow_query_count *query;

	priv = nfp_flow_dev_to_priv(dev);
	flow = nfp_flow_table_search(priv, nfp_flow);
	if (flow == NULL) {
		PMD_DRV_LOG(ERR, "Can not find statistics for this flow.");
		return;
	}

	query = (struct rte_flow_query_count *)data;
	reset = query->reset;
	memset(query, 0, sizeof(*query));

	ctx_id = rte_be_to_cpu_32(nfp_flow->payload.meta->host_ctx_id);
	stats = &priv->stats[ctx_id];

	rte_spinlock_lock(&priv->stats_lock);
	if (stats->pkts != 0 && stats->bytes != 0) {
		query->hits = stats->pkts;
		query->bytes = stats->bytes;
		query->hits_set = 1;
		query->bytes_set = 1;
		if (reset) {
			stats->pkts = 0;
			stats->bytes = 0;
		}
	}
	rte_spinlock_unlock(&priv->stats_lock);
}

static int
nfp_flow_query(struct rte_eth_dev *dev,
		struct rte_flow *nfp_flow,
		const struct rte_flow_action *actions,
		void *data,
		struct rte_flow_error *error)
{
	const struct rte_flow_action *action;

	for (action = actions; action->type != RTE_FLOW_ACTION_TYPE_END; ++action) {
		switch (action->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			nfp_flow_stats_get(dev, nfp_flow, data);
			break;
		default:
			rte_flow_error_set(error, ENOTSUP, RTE_FLOW_ERROR_TYPE_UNSPECIFIED,
					NULL, "Unsupported action type for flow query.");
			return -ENOTSUP;
		}
	}

	return 0;
}

static int
nfp_flow_tunnel_match(__rte_unused struct rte_eth_dev *dev,
		__rte_unused struct rte_flow_tunnel *tunnel,
		__rte_unused struct rte_flow_item **pmd_items,
		uint32_t *num_of_items,
		__rte_unused struct rte_flow_error *err)
{
	*num_of_items = 0;

	return 0;
}

static int
nfp_flow_tunnel_item_release(__rte_unused struct rte_eth_dev *dev,
		__rte_unused struct rte_flow_item *pmd_items,
		__rte_unused uint32_t num_of_items,
		__rte_unused struct rte_flow_error *err)
{
	return 0;
}

static int
nfp_flow_tunnel_decap_set(__rte_unused struct rte_eth_dev *dev,
		struct rte_flow_tunnel *tunnel,
		struct rte_flow_action **pmd_actions,
		uint32_t *num_of_actions,
		__rte_unused struct rte_flow_error *err)
{
	struct rte_flow_action *nfp_action;

	nfp_action = rte_zmalloc("nfp_tun_action", sizeof(struct rte_flow_action), 0);
	if (nfp_action == NULL) {
		PMD_DRV_LOG(ERR, "Alloc memory for nfp tunnel action failed.");
		return -ENOMEM;
	}

	if (tunnel->is_ipv6)
		nfp_action->conf = (void *)~0;

	switch (tunnel->type) {
	case RTE_FLOW_ITEM_TYPE_VXLAN:
		nfp_action->type = RTE_FLOW_ACTION_TYPE_VXLAN_DECAP;
		*pmd_actions = nfp_action;
		*num_of_actions = 1;
		break;
	case RTE_FLOW_ITEM_TYPE_GENEVE:
	case RTE_FLOW_ITEM_TYPE_GRE:
		nfp_action->type = RTE_FLOW_ACTION_TYPE_RAW_DECAP;
		*pmd_actions = nfp_action;
		*num_of_actions = 1;
		break;
	default:
		*pmd_actions = NULL;
		*num_of_actions = 0;
		rte_free(nfp_action);
		break;
	}

	return 0;
}

static int
nfp_flow_tunnel_action_decap_release(__rte_unused struct rte_eth_dev *dev,
		struct rte_flow_action *pmd_actions,
		uint32_t num_of_actions,
		__rte_unused struct rte_flow_error *err)
{
	uint32_t i;
	struct rte_flow_action *nfp_action;

	for (i = 0; i < num_of_actions; i++) {
		nfp_action = &pmd_actions[i];
		nfp_action->conf = NULL;
		rte_free(nfp_action);
	}

	return 0;
}

static const struct rte_flow_ops nfp_flow_ops = {
	.validate                    = nfp_flow_validate,
	.create                      = nfp_flow_create,
	.destroy                     = nfp_flow_destroy,
	.flush                       = nfp_flow_flush,
	.query                       = nfp_flow_query,
	.tunnel_match                = nfp_flow_tunnel_match,
	.tunnel_item_release         = nfp_flow_tunnel_item_release,
	.tunnel_decap_set            = nfp_flow_tunnel_decap_set,
	.tunnel_action_decap_release = nfp_flow_tunnel_action_decap_release,
};

int
nfp_net_flow_ops_get(struct rte_eth_dev *dev,
		const struct rte_flow_ops **ops)
{
	if ((dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR) == 0) {
		*ops = NULL;
		PMD_DRV_LOG(ERR, "Port is not a representor.");
		return -EINVAL;
	}

	*ops = &nfp_flow_ops;

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

	struct rte_hash_parameters pre_tun_hash_params = {
		.name       = "pre_tunnel_table",
		.entries    = 32,
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

	/* pre tunnel table */
	priv->pre_tun_cnt = 1;
	pre_tun_hash_params.hash_func_init_val = priv->hash_seed;
	priv->pre_tun_table = rte_hash_create(&pre_tun_hash_params);
	if (priv->pre_tun_table == NULL) {
		PMD_INIT_LOG(ERR, "Pre tunnel table creation failed");
		ret = -ENOMEM;
		goto free_flow_table;
	}

	/* ipv4 off list */
	rte_spinlock_init(&priv->ipv4_off_lock);
	LIST_INIT(&priv->ipv4_off_list);

	/* ipv6 off list */
	rte_spinlock_init(&priv->ipv6_off_lock);
	LIST_INIT(&priv->ipv6_off_list);

	/* neighbor next list */
	LIST_INIT(&priv->nn_list);

	return 0;

free_flow_table:
	rte_hash_free(priv->flow_table);
free_mask_table:
	rte_hash_free(priv->mask_table);
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

	rte_hash_free(priv->pre_tun_table);
	rte_hash_free(priv->flow_table);
	rte_hash_free(priv->mask_table);
	rte_free(priv->stats);
	rte_free(priv->stats_ids.free_list.buf);
	rte_free(priv->mask_ids.free_list.buf);
	rte_free(priv);
}
