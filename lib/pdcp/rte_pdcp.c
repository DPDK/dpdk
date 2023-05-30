/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_errno.h>
#include <rte_pdcp.h>
#include <rte_malloc.h>

#include "pdcp_cnt.h"
#include "pdcp_crypto.h"
#include "pdcp_ctrl_pdu.h"
#include "pdcp_entity.h"
#include "pdcp_process.h"

#define RTE_PDCP_DYNFIELD_NAME "rte_pdcp_dynfield"

struct entity_layout {
	size_t bitmap_offset;
	size_t bitmap_size;

	size_t reorder_buf_offset;
	size_t reorder_buf_size;

	size_t total_size;
};

int rte_pdcp_dynfield_offset = -1;

static int
pdcp_dynfield_register(void)
{
	const struct rte_mbuf_dynfield dynfield_desc = {
		.name = RTE_PDCP_DYNFIELD_NAME,
		.size = sizeof(rte_pdcp_dynfield_t),
		.align = __alignof__(rte_pdcp_dynfield_t),
	};

	if (rte_pdcp_dynfield_offset != -1)
		return rte_pdcp_dynfield_offset;

	rte_pdcp_dynfield_offset = rte_mbuf_dynfield_register(&dynfield_desc);
	return rte_pdcp_dynfield_offset;
}

static int
pdcp_entity_layout_get(const struct rte_pdcp_entity_conf *conf, struct entity_layout *layout)
{
	size_t size;
	const uint32_t window_size = pdcp_window_size_get(conf->pdcp_xfrm.sn_size);

	size = sizeof(struct rte_pdcp_entity) + sizeof(struct entity_priv);

	if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK) {
		size += sizeof(struct entity_priv_dl_part);
		/* Bitmap require memory to be cache aligned */
		size = RTE_CACHE_LINE_ROUNDUP(size);
		layout->bitmap_offset = size;
		layout->bitmap_size = pdcp_cnt_bitmap_get_memory_footprint(conf);
		size += layout->bitmap_size;
		layout->reorder_buf_offset = size;
		layout->reorder_buf_size = pdcp_reorder_memory_footprint_get(window_size);
		size += layout->reorder_buf_size;
	} else if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)
		size += sizeof(struct entity_priv_ul_part);
	else
		return -EINVAL;

	layout->total_size = size;

	return 0;
}

static int
pdcp_dl_establish(struct rte_pdcp_entity *entity, const struct rte_pdcp_entity_conf *conf,
		  const struct entity_layout *layout)
{
	const uint32_t window_size = pdcp_window_size_get(conf->pdcp_xfrm.sn_size);
	struct entity_priv_dl_part *dl = entity_dl_part_get(entity);
	void *memory;
	int ret;

	entity->max_pkt_cache = RTE_MAX(entity->max_pkt_cache, window_size);
	dl->t_reorder.handle = conf->t_reordering;

	memory = RTE_PTR_ADD(entity, layout->reorder_buf_offset);
	ret = pdcp_reorder_create(&dl->reorder, window_size, memory, layout->reorder_buf_size);
	if (ret)
		return ret;

	memory = RTE_PTR_ADD(entity, layout->bitmap_offset);
	ret = pdcp_cnt_bitmap_create(dl, window_size, memory, layout->bitmap_size);
	if (ret)
		return ret;

	return 0;
}

struct rte_pdcp_entity *
rte_pdcp_entity_establish(const struct rte_pdcp_entity_conf *conf)
{
	struct entity_layout entity_layout = { 0 };
	struct rte_pdcp_entity *entity = NULL;
	struct entity_priv *en_priv;
	uint32_t count;
	int ret;

	if (pdcp_dynfield_register() < 0)
		return NULL;

	if (conf == NULL || conf->cop_pool == NULL || conf->ctrl_pdu_pool == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	if (conf->pdcp_xfrm.en_ordering || conf->pdcp_xfrm.remove_duplicates || conf->is_slrb ||
	    conf->en_sec_offload) {
		rte_errno = ENOTSUP;
		return NULL;
	}

	/*
	 * 6.3.2 PDCP SN
	 * Length: 12 or 18 bits as indicated in table 6.3.2-1. The length of the PDCP SN is
	 * configured by upper layers (pdcp-SN-SizeUL, pdcp-SN-SizeDL, or sl-PDCP-SN-Size in
	 * TS 38.331 [3])
	 */
	if ((conf->pdcp_xfrm.sn_size != RTE_SECURITY_PDCP_SN_SIZE_12) &&
	    (conf->pdcp_xfrm.sn_size != RTE_SECURITY_PDCP_SN_SIZE_18)) {
		rte_errno = ENOTSUP;
		return NULL;
	}

	if (conf->pdcp_xfrm.hfn_threshold) {
		rte_errno = EINVAL;
		return NULL;
	}

	ret = pdcp_entity_layout_get(conf, &entity_layout);
	if (ret < 0) {
		rte_errno = EINVAL;
		return NULL;
	}

	entity = rte_zmalloc_socket("pdcp_entity", entity_layout.total_size, RTE_CACHE_LINE_SIZE,
				    SOCKET_ID_ANY);
	if (entity == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	en_priv = entity_priv_get(entity);

	count = pdcp_count_from_hfn_sn_get(conf->pdcp_xfrm.hfn, conf->sn, conf->pdcp_xfrm.sn_size);

	en_priv->state.rx_deliv = count;
	en_priv->state.tx_next = count;
	en_priv->cop_pool = conf->cop_pool;
	en_priv->ctrl_pdu_pool = conf->ctrl_pdu_pool;

	/* Setup crypto session */
	ret = pdcp_crypto_sess_create(entity, conf);
	if (ret)
		goto entity_free;

	ret = pdcp_process_func_set(entity, conf);
	if (ret)
		goto crypto_sess_destroy;

	if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK) {
		ret = pdcp_dl_establish(entity, conf, &entity_layout);
		if (ret)
			goto crypto_sess_destroy;
	}

	return entity;

crypto_sess_destroy:
	pdcp_crypto_sess_destroy(entity);
entity_free:
	rte_free(entity);
	rte_errno = -ret;
	return NULL;
}

static int
pdcp_dl_release(struct rte_pdcp_entity *entity, struct rte_mbuf *out_mb[])
{
	struct entity_priv_dl_part *dl = entity_dl_part_get(entity);
	struct entity_priv *en_priv = entity_priv_get(entity);
	int nb_out;

	nb_out = pdcp_reorder_up_to_get(&dl->reorder, out_mb, entity->max_pkt_cache,
			en_priv->state.rx_next);

	return nb_out;
}

int
rte_pdcp_entity_release(struct rte_pdcp_entity *pdcp_entity, struct rte_mbuf *out_mb[])
{
	struct entity_priv *en_priv;
	int nb_out = 0;

	if (pdcp_entity == NULL)
		return -EINVAL;

	en_priv = entity_priv_get(pdcp_entity);

	if (!en_priv->flags.is_ul_entity)
		nb_out = pdcp_dl_release(pdcp_entity, out_mb);

	/* Teardown crypto sessions */
	pdcp_crypto_sess_destroy(pdcp_entity);

	rte_free(pdcp_entity);

	return nb_out;
}

int
rte_pdcp_entity_suspend(struct rte_pdcp_entity *pdcp_entity,
			struct rte_mbuf *out_mb[])
{
	struct entity_priv_dl_part *dl;
	struct entity_priv *en_priv;
	int nb_out = 0;

	if (pdcp_entity == NULL)
		return -EINVAL;

	en_priv = entity_priv_get(pdcp_entity);

	if (en_priv->flags.is_ul_entity) {
		en_priv->state.tx_next = 0;
	} else {
		dl = entity_dl_part_get(pdcp_entity);
		nb_out = pdcp_reorder_up_to_get(&dl->reorder, out_mb, pdcp_entity->max_pkt_cache,
				en_priv->state.rx_next);
		pdcp_reorder_stop(&dl->reorder);
		en_priv->state.rx_next = 0;
		en_priv->state.rx_deliv = 0;
	}

	return nb_out;
}

struct rte_mbuf *
rte_pdcp_control_pdu_create(struct rte_pdcp_entity *pdcp_entity,
			    enum rte_pdcp_ctrl_pdu_type type)
{
	struct entity_priv_dl_part *dl;
	struct entity_priv *en_priv;
	struct rte_mbuf *m;
	int ret;

	if (pdcp_entity == NULL) {
		rte_errno = EINVAL;
		return NULL;
	}

	en_priv = entity_priv_get(pdcp_entity);
	dl = entity_dl_part_get(pdcp_entity);

	m = rte_pktmbuf_alloc(en_priv->ctrl_pdu_pool);
	if (m == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	switch (type) {
	case RTE_PDCP_CTRL_PDU_TYPE_STATUS_REPORT:
		ret = pdcp_ctrl_pdu_status_gen(en_priv, dl, m);
		break;
	default:
		ret = -ENOTSUP;
	}

	if (ret) {
		rte_pktmbuf_free(m);
		rte_errno = -ret;
		return NULL;
	}

	return m;
}

uint16_t
rte_pdcp_t_reordering_expiry_handle(const struct rte_pdcp_entity *entity, struct rte_mbuf *out_mb[])
{
	struct entity_priv_dl_part *dl = entity_dl_part_get(entity);
	struct entity_priv *en_priv = entity_priv_get(entity);
	uint16_t capacity = entity->max_pkt_cache;
	uint16_t nb_out, nb_seq;

	/* 5.2.2.2 Actions when a t-Reordering expires */

	/*
	 * - deliver to upper layers in ascending order of the associated COUNT value after
	 *   performing header decompression, if not decompressed before:
	 */

	/*   - all stored PDCP SDU(s) with associated COUNT value(s) < RX_REORD; */
	nb_out = pdcp_reorder_up_to_get(&dl->reorder, out_mb, capacity, en_priv->state.rx_reord);
	capacity -= nb_out;
	out_mb = &out_mb[nb_out];

	/*
	 *   - all stored PDCP SDU(s) with consecutively associated COUNT value(s) starting from
	 *     RX_REORD;
	 */
	nb_seq = pdcp_reorder_get_sequential(&dl->reorder, out_mb, capacity);
	nb_out += nb_seq;

	/*
	 * - update RX_DELIV to the COUNT value of the first PDCP SDU which has not been delivered
	 *   to upper layers, with COUNT value >= RX_REORD;
	 */
	pdcp_rx_deliv_set(entity, en_priv->state.rx_reord + nb_seq);

	/*
	 * - if RX_DELIV < RX_NEXT:
	 *   - update RX_REORD to RX_NEXT;
	 *   - start t-Reordering.
	 */
	if (en_priv->state.rx_deliv < en_priv->state.rx_next) {
		en_priv->state.rx_reord = en_priv->state.rx_next;
		dl->t_reorder.state = TIMER_RUNNING;
		dl->t_reorder.handle.start(dl->t_reorder.handle.timer, dl->t_reorder.handle.args);
	} else {
		dl->t_reorder.state = TIMER_EXPIRED;
	}

	return nb_out;
}
