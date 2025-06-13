/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2025 Marvell.
 */
#include <event_vector_adapter_pmd.h>
#include <eventdev_pmd.h>

#include "roc_api.h"

#include "cnxk_eventdev.h"
#include "cnxk_vector_adptr.h"
#include "cnxk_worker.h"

static int
cnxk_sso_vector_adapter_create(struct rte_event_vector_adapter *adapter)
{
	struct rte_event_vector_adapter_conf *conf;
	struct cnxk_event_vector_adapter *adptr;
	uint32_t agq, tag_mask, stag_mask;
	struct roc_sso_agq_data data;
	struct cnxk_sso_evdev *dev;
	uint8_t queue_id;
	int rc;

	adptr = rte_zmalloc("cnxk_event_vector_adapter", sizeof(struct cnxk_event_vector_adapter),
			    RTE_CACHE_LINE_SIZE);
	if (adptr == NULL) {
		plt_err("Failed to allocate memory for vector adapter");
		return -ENOMEM;
	}

	conf = &adapter->data->conf;

	queue_id = adapter->data->conf.ev.queue_id;
	dev = cnxk_sso_pmd_priv(&rte_eventdevs[conf->event_dev_id]);
	adapter->data->adapter_priv = adptr;

	tag_mask = conf->ev.event & 0xFFFFFFFF;
	stag_mask = (conf->ev_fallback.event & 0xFFFFFFFF) >> 20;

	memset(&data, 0, sizeof(struct roc_sso_agq_data));
	data.tag = tag_mask;
	data.tt = conf->ev.sched_type;
	data.stag = stag_mask;
	data.vwqe_aura = roc_npa_aura_handle_to_aura(conf->vector_mp->pool_id);
	data.vwqe_max_sz_exp = rte_log2_u32(conf->vector_sz);
	data.vwqe_wait_tmo = conf->vector_timeout_ns / ((SSO_AGGR_DEF_TMO + 1) * 100);
	data.xqe_type = 0;

	agq = UINT32_MAX;
	rc = roc_sso_hwgrp_agq_alloc(&dev->sso, queue_id, &data, &agq);
	if (rc < 0 || agq == UINT32_MAX) {
		plt_err("Failed to allocate aggregation queue for queue_id=%d", queue_id);
		adapter->data->adapter_priv = NULL;
		rte_free(adptr);
		return rc;
	}

	adptr->agq = agq;
	adptr->tt = SSO_TT_AGG;
	adptr->base = roc_sso_hwgrp_base_get(&dev->sso, queue_id);

	return 0;
}

static int
cnxk_sso_vector_adapter_destroy(struct rte_event_vector_adapter *adapter)
{
	struct cnxk_event_vector_adapter *adptr;
	struct cnxk_sso_evdev *dev;
	uint16_t queue_id;

	if (adapter == NULL || adapter->data == NULL)
		return -EINVAL;

	adptr = adapter->data->adapter_priv;
	if (adptr == NULL)
		return -EINVAL;

	queue_id = adapter->data->conf.ev.queue_id;
	dev = cnxk_sso_pmd_priv(&rte_eventdevs[adapter->data->conf.event_dev_id]);

	roc_sso_hwgrp_agq_free(&dev->sso, queue_id, adptr->agq);
	rte_free(adptr);
	adapter->data->adapter_priv = NULL;

	return 0;
}

static int
cnxk_sso_vector_adapter_enqueue(struct rte_event_vector_adapter *adapter, uint64_t objs[],
				uint16_t num_elem, uint64_t flags)
{
	struct cnxk_event_vector_adapter *adptr;
	uint16_t n = num_elem;
	uint64_t add_work0;

	adptr = adapter->data->adapter_priv;
	plt_wmb();

	add_work0 = adptr->agq | ((uint64_t)(adptr->tt) << 32);
	roc_store_pair(add_work0 | ((uint64_t)!!(flags & RTE_EVENT_VECTOR_ENQ_SOV) << 34),
		       objs[num_elem - n], adptr->base);
	while (--n > 1)
		roc_store_pair(add_work0, objs[num_elem - n], adptr->base);
	if (n)
		roc_store_pair(add_work0 | ((uint64_t)!!(flags & RTE_EVENT_VECTOR_ENQ_EOV) << 35),
			       objs[num_elem - n], adptr->base);

	return num_elem;
}

static struct event_vector_adapter_ops vector_ops = {
	.create = cnxk_sso_vector_adapter_create,
	.destroy = cnxk_sso_vector_adapter_destroy,
	.enqueue = cnxk_sso_vector_adapter_enqueue,
};

int
cnxk_vector_caps_get(const struct rte_eventdev *evdev, uint32_t *caps,
		     const struct event_vector_adapter_ops **ops)
{
	RTE_SET_USED(evdev);

	*caps = RTE_EVENT_VECTOR_ADAPTER_CAP_INTERNAL_PORT;
	*ops = &vector_ops;

	return 0;
}

int
cnxk_vector_info_get(const struct rte_eventdev *evdev, struct rte_event_vector_adapter_info *info)
{
	RTE_SET_USED(evdev);
	if (info == NULL)
		return -EINVAL;

	info->max_vector_adapters_per_event_queue =
		SSO_AGGR_MAX_CTX > UINT8_MAX ? UINT8_MAX : SSO_AGGR_MAX_CTX;
	info->log2_sz = true;
	info->min_vector_sz = RTE_BIT32(ROC_NIX_VWQE_MIN_SIZE_LOG2);
	info->max_vector_sz = RTE_BIT32(ROC_NIX_VWQE_MAX_SIZE_LOG2);
	info->min_vector_timeout_ns = (SSO_AGGR_DEF_TMO + 1) * 100;
	info->max_vector_timeout_ns = (BITMASK_ULL(11, 0) + 1) * info->min_vector_timeout_ns;

	info->log2_sz = true;

	return 0;
}
