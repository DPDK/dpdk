/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include <rte_errno.h>
#include <rte_pdcp.h>
#include <rte_malloc.h>

#include "pdcp_crypto.h"
#include "pdcp_entity.h"
#include "pdcp_process.h"

#define RTE_PDCP_DYNFIELD_NAME "rte_pdcp_dynfield"

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
pdcp_entity_size_get(const struct rte_pdcp_entity_conf *conf)
{
	int size;

	size = sizeof(struct rte_pdcp_entity) + sizeof(struct entity_priv);

	if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_DOWNLINK)
		size += sizeof(struct entity_priv_dl_part);
	else if (conf->pdcp_xfrm.pkt_dir == RTE_SECURITY_PDCP_UPLINK)
		size += sizeof(struct entity_priv_ul_part);
	else
		return -EINVAL;

	return RTE_ALIGN_CEIL(size, RTE_CACHE_LINE_SIZE);
}

struct rte_pdcp_entity *
rte_pdcp_entity_establish(const struct rte_pdcp_entity_conf *conf)
{
	struct rte_pdcp_entity *entity = NULL;
	struct entity_priv *en_priv;
	int ret, entity_size;
	uint32_t count;

	if (pdcp_dynfield_register() < 0)
		return NULL;

	if (conf == NULL || conf->cop_pool == NULL) {
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

	entity_size = pdcp_entity_size_get(conf);
	if (entity_size < 0) {
		rte_errno = EINVAL;
		return NULL;
	}

	entity = rte_zmalloc_socket("pdcp_entity", entity_size, RTE_CACHE_LINE_SIZE, SOCKET_ID_ANY);
	if (entity == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}

	en_priv = entity_priv_get(entity);

	count = pdcp_count_from_hfn_sn_get(conf->pdcp_xfrm.hfn, conf->sn, conf->pdcp_xfrm.sn_size);

	en_priv->state.rx_deliv = count;
	en_priv->state.tx_next = count;
	en_priv->cop_pool = conf->cop_pool;

	/* Setup crypto session */
	ret = pdcp_crypto_sess_create(entity, conf);
	if (ret)
		goto entity_free;

	ret = pdcp_process_func_set(entity, conf);
	if (ret)
		goto crypto_sess_destroy;

	return entity;

crypto_sess_destroy:
	pdcp_crypto_sess_destroy(entity);
entity_free:
	rte_free(entity);
	rte_errno = -ret;
	return NULL;
}

int
rte_pdcp_entity_release(struct rte_pdcp_entity *pdcp_entity, struct rte_mbuf *out_mb[])
{
	if (pdcp_entity == NULL)
		return -EINVAL;

	/* Teardown crypto sessions */
	pdcp_crypto_sess_destroy(pdcp_entity);

	rte_free(pdcp_entity);

	RTE_SET_USED(out_mb);
	return 0;
}

int
rte_pdcp_entity_suspend(struct rte_pdcp_entity *pdcp_entity,
			struct rte_mbuf *out_mb[])
{
	struct entity_priv *en_priv;

	if (pdcp_entity == NULL)
		return -EINVAL;

	en_priv = entity_priv_get(pdcp_entity);

	if (en_priv->flags.is_ul_entity) {
		en_priv->state.tx_next = 0;
	} else {
		en_priv->state.rx_next = 0;
		en_priv->state.rx_deliv = 0;
	}

	RTE_SET_USED(out_mb);

	return 0;
}
