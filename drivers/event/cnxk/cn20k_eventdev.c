/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "roc_api.h"

#include "cn20k_ethdev.h"
#include "cn20k_eventdev.h"
#include "cn20k_worker.h"
#include "cnxk_common.h"
#include "cnxk_eventdev.h"
#include "cnxk_worker.h"

static void *
cn20k_sso_init_hws_mem(void *arg, uint8_t port_id)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws;

	/* Allocate event port memory */
	ws = rte_zmalloc("cn20k_ws", sizeof(struct cn20k_sso_hws) + RTE_CACHE_LINE_SIZE,
			 RTE_CACHE_LINE_SIZE);
	if (ws == NULL) {
		plt_err("Failed to alloc memory for port=%d", port_id);
		return NULL;
	}

	/* First cache line is reserved for cookie */
	ws = (struct cn20k_sso_hws *)((uint8_t *)ws + RTE_CACHE_LINE_SIZE);
	ws->base = roc_sso_hws_base_get(&dev->sso, port_id);
	ws->hws_id = port_id;
	ws->swtag_req = 0;
	ws->gw_wdata = cnxk_sso_hws_prf_wdata(dev);
	ws->gw_rdata = SSO_TT_EMPTY << 32;
	ws->xae_waes = dev->sso.feat.xaq_wq_entries;

	return ws;
}

static int
cn20k_sso_hws_link(void *arg, void *port, uint16_t *map, uint16_t nb_link, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = port;

	return roc_sso_hws_link(&dev->sso, ws->hws_id, map, nb_link, profile, 0);
}

static int
cn20k_sso_hws_unlink(void *arg, void *port, uint16_t *map, uint16_t nb_link, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = port;

	return roc_sso_hws_unlink(&dev->sso, ws->hws_id, map, nb_link, profile, 0);
}

static void
cn20k_sso_hws_setup(void *arg, void *hws, uintptr_t grp_base)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = hws;
	uint64_t val;

	ws->grp_base = grp_base;
	ws->fc_mem = (int64_t __rte_atomic *)dev->fc_iova;
	ws->xaq_lmt = dev->xaq_lmt;
	ws->fc_cache_space = (int64_t __rte_atomic *)dev->fc_cache_space;
	ws->aw_lmt = dev->sso.lmt_base;
	ws->gw_wdata = cnxk_sso_hws_prf_wdata(dev);

	/* Set get_work timeout for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns);
	val = val ? val - 1 : 0;
	plt_write64(val, ws->base + SSOW_LF_GWS_NW_TIM);
}

static void
cn20k_sso_hws_release(void *arg, void *hws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn20k_sso_hws *ws = hws;
	uint16_t i, j;

	for (i = 0; i < CNXK_SSO_MAX_PROFILES; i++)
		for (j = 0; j < dev->nb_event_queues; j++)
			roc_sso_hws_unlink(&dev->sso, ws->hws_id, &j, 1, i, 0);
	memset(ws, 0, sizeof(*ws));
}

static int
cn20k_sso_hws_flush_events(void *hws, uint8_t queue_id, uintptr_t base, cnxk_handle_event_t fn,
			   void *arg)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(arg);
	uint64_t retry = CNXK_SSO_FLUSH_RETRY_MAX;
	struct cn20k_sso_hws *ws = hws;
	uint64_t cq_ds_cnt = 1;
	uint64_t aq_cnt = 1;
	uint64_t ds_cnt = 1;
	struct rte_event ev;
	uint64_t val, req;

	plt_write64(0, base + SSO_LF_GGRP_QCTL);

	roc_sso_hws_gwc_invalidate(&dev->sso, &ws->hws_id, 1);
	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
	req = queue_id;	    /* GGRP ID */
	req |= BIT_ULL(18); /* Grouped */
	req |= BIT_ULL(16); /* WAIT */

	aq_cnt = plt_read64(base + SSO_LF_GGRP_AQ_CNT);
	ds_cnt = plt_read64(base + SSO_LF_GGRP_MISC_CNT);
	cq_ds_cnt = plt_read64(base + SSO_LF_GGRP_INT_CNT);
	cq_ds_cnt &= 0x3FFF3FFF0000;

	while (aq_cnt || cq_ds_cnt || ds_cnt) {
		plt_write64(req, ws->base + SSOW_LF_GWS_OP_GET_WORK0);
		cn20k_sso_hws_get_work_empty(ws, &ev, 0);
		if (fn != NULL && ev.u64 != 0)
			fn(arg, ev);
		if (ev.sched_type != SSO_TT_EMPTY)
			cnxk_sso_hws_swtag_flush(ws->base);
		else if (retry-- == 0)
			break;
		do {
			val = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
		} while (val & BIT_ULL(56));
		aq_cnt = plt_read64(base + SSO_LF_GGRP_AQ_CNT);
		ds_cnt = plt_read64(base + SSO_LF_GGRP_MISC_CNT);
		cq_ds_cnt = plt_read64(base + SSO_LF_GGRP_INT_CNT);
		/* Extract cq and ds count */
		cq_ds_cnt &= 0x3FFF3FFF0000;
	}

	if (aq_cnt || cq_ds_cnt || ds_cnt)
		return -EAGAIN;

	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
	roc_sso_hws_gwc_invalidate(&dev->sso, &ws->hws_id, 1);
	rte_mb();

	return 0;
}

static void
cn20k_sso_set_rsrc(void *arg)
{
	struct cnxk_sso_evdev *dev = arg;

	dev->max_event_ports = dev->sso.max_hws;
	dev->max_event_queues = dev->sso.max_hwgrp > RTE_EVENT_MAX_QUEUES_PER_DEV ?
					RTE_EVENT_MAX_QUEUES_PER_DEV :
					dev->sso.max_hwgrp;
}

static int
cn20k_sso_rsrc_init(void *arg, uint8_t hws, uint8_t hwgrp)
{
	struct cnxk_tim_evdev *tim_dev = cnxk_tim_priv_get();
	struct cnxk_sso_evdev *dev = arg;
	uint16_t nb_tim_lfs;

	nb_tim_lfs = tim_dev ? tim_dev->nb_rings : 0;
	return roc_sso_rsrc_init(&dev->sso, hws, hwgrp, nb_tim_lfs);
}


static void
cn20k_sso_fp_fns_set(struct rte_eventdev *event_dev)
{
#if defined(RTE_ARCH_ARM64)
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	event_dev->enqueue_burst = cn20k_sso_hws_enq_burst;
	event_dev->enqueue_new_burst = cn20k_sso_hws_enq_new_burst;
	event_dev->enqueue_forward_burst = cn20k_sso_hws_enq_fwd_burst;

	event_dev->dequeue_burst = cn20k_sso_hws_deq_burst;
	if (dev->deq_tmo_ns)
		event_dev->dequeue_burst = cn20k_sso_hws_tmo_deq_burst;

	event_dev->profile_switch = cn20k_sso_hws_profile_switch;
	event_dev->preschedule_modify = cn20k_sso_hws_preschedule_modify;
	event_dev->preschedule = cn20k_sso_hws_preschedule;
#else
	RTE_SET_USED(event_dev);
#endif
}

static void
cn20k_sso_info_get(struct rte_eventdev *event_dev, struct rte_event_dev_info *dev_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_CN20K_PMD);
	cnxk_sso_info_get(dev, dev_info);
	dev_info->max_event_port_enqueue_depth = UINT32_MAX;
}

static int
cn20k_sso_dev_configure(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = cnxk_sso_dev_validate(event_dev, 1, UINT32_MAX);
	if (rc < 0) {
		plt_err("Invalid event device configuration");
		return -EINVAL;
	}

	rc = cn20k_sso_rsrc_init(dev, dev->nb_event_ports, dev->nb_event_queues);
	if (rc < 0) {
		plt_err("Failed to initialize SSO resources");
		return -ENODEV;
	}

	rc = cnxk_sso_xaq_allocate(dev);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	dev->gw_mode = cnxk_sso_hws_preschedule_get(event_dev->data->dev_conf.preschedule_type);

	rc = cnxk_setup_event_ports(event_dev, cn20k_sso_init_hws_mem, cn20k_sso_hws_setup);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	/* Restore any prior port-queue mapping. */
	cnxk_sso_restore_links(event_dev, cn20k_sso_hws_link);

	dev->configured = 1;
	rte_mb();

	return 0;
cnxk_rsrc_fini:
	roc_sso_rsrc_fini(&dev->sso);
	dev->nb_event_ports = 0;
	return rc;
}

static int
cn20k_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		     const struct rte_event_port_conf *port_conf)
{

	RTE_SET_USED(port_conf);
	return cnxk_sso_port_setup(event_dev, port_id, cn20k_sso_hws_setup);
}

static void
cn20k_sso_port_release(void *port)
{
	struct cnxk_sso_hws_cookie *gws_cookie = cnxk_sso_hws_get_cookie(port);
	struct cnxk_sso_evdev *dev;

	if (port == NULL)
		return;

	dev = cnxk_sso_pmd_priv(gws_cookie->event_dev);
	if (!gws_cookie->configured)
		goto free;

	cn20k_sso_hws_release(dev, port);
	memset(gws_cookie, 0, sizeof(*gws_cookie));
free:
	rte_free(gws_cookie);
}

static void
cn20k_sso_port_quiesce(struct rte_eventdev *event_dev, void *port,
		       rte_eventdev_port_flush_t flush_cb, void *args)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct cn20k_sso_hws *ws = port;
	struct rte_event ev;
	uint64_t ptag;
	bool is_pend;

	is_pend = false;
	/* Work in WQE0 is always consumed, unless its a SWTAG. */
	ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
	if (ptag & (BIT_ULL(62) | BIT_ULL(54)) || ws->swtag_req)
		is_pend = true;
	do {
		ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
	} while (ptag & (BIT_ULL(62) | BIT_ULL(58) | BIT_ULL(56) | BIT_ULL(54)));

	cn20k_sso_hws_get_work_empty(ws, &ev, 0);
	if (is_pend && ev.u64)
		if (flush_cb)
			flush_cb(event_dev->data->dev_id, ev, args);
	ptag = (plt_read64(ws->base + SSOW_LF_GWS_TAG) >> 32) & SSO_TT_EMPTY;
	if (ptag != SSO_TT_EMPTY)
		cnxk_sso_hws_swtag_flush(ws->base);

	do {
		ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
	} while (ptag & BIT_ULL(56));

	/* Check if we have work in PRF_WQE0, if so extract it. */
	switch (dev->gw_mode) {
	case CNXK_GW_MODE_PREF:
	case CNXK_GW_MODE_PREF_WFE:
		while (plt_read64(ws->base + SSOW_LF_GWS_PRF_WQE0) & BIT_ULL(63))
			;
		break;
	case CNXK_GW_MODE_NONE:
	default:
		break;
	}

	if (CNXK_TT_FROM_TAG(plt_read64(ws->base + SSOW_LF_GWS_PRF_WQE0)) != SSO_TT_EMPTY) {
		plt_write64(BIT_ULL(16) | 1, ws->base + SSOW_LF_GWS_OP_GET_WORK0);
		cn20k_sso_hws_get_work_empty(ws, &ev, 0);
		if (ev.u64) {
			if (flush_cb)
				flush_cb(event_dev->data->dev_id, ev, args);
		}
		cnxk_sso_hws_swtag_flush(ws->base);
		do {
			ptag = plt_read64(ws->base + SSOW_LF_GWS_PENDSTATE);
		} while (ptag & BIT_ULL(56));
	}
	ws->swtag_req = 0;
	plt_write64(0, ws->base + SSOW_LF_GWS_OP_GWC_INVAL);
}

static int
cn20k_sso_port_link_profile(struct rte_eventdev *event_dev, void *port, const uint8_t queues[],
			    const uint8_t priorities[], uint16_t nb_links, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_links];
	uint16_t link;

	RTE_SET_USED(priorities);
	for (link = 0; link < nb_links; link++)
		hwgrp_ids[link] = queues[link];
	nb_links = cn20k_sso_hws_link(dev, port, hwgrp_ids, nb_links, profile);

	return (int)nb_links;
}

static int
cn20k_sso_port_unlink_profile(struct rte_eventdev *event_dev, void *port, uint8_t queues[],
			      uint16_t nb_unlinks, uint8_t profile)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t hwgrp_ids[nb_unlinks];
	uint16_t unlink;

	for (unlink = 0; unlink < nb_unlinks; unlink++)
		hwgrp_ids[unlink] = queues[unlink];
	nb_unlinks = cn20k_sso_hws_unlink(dev, port, hwgrp_ids, nb_unlinks, profile);

	return (int)nb_unlinks;
}

static int
cn20k_sso_port_link(struct rte_eventdev *event_dev, void *port, const uint8_t queues[],
		    const uint8_t priorities[], uint16_t nb_links)
{
	return cn20k_sso_port_link_profile(event_dev, port, queues, priorities, nb_links, 0);
}

static int
cn20k_sso_port_unlink(struct rte_eventdev *event_dev, void *port, uint8_t queues[],
		      uint16_t nb_unlinks)
{
	return cn20k_sso_port_unlink_profile(event_dev, port, queues, nb_unlinks, 0);
}

static int
cn20k_sso_start(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint8_t hws[RTE_EVENT_MAX_PORTS_PER_DEV];
	int rc, i;

	cnxk_sso_configure_queue_stash(event_dev);
	rc = cnxk_sso_start(event_dev, cnxk_sso_hws_reset, cn20k_sso_hws_flush_events);
	if (rc < 0)
		return rc;
	cn20k_sso_fp_fns_set(event_dev);
	for (i = 0; i < event_dev->data->nb_ports; i++)
		hws[i] = i;
	roc_sso_hws_gwc_invalidate(&dev->sso, hws, event_dev->data->nb_ports);

	return rc;
}

static void
cn20k_sso_stop(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint8_t hws[RTE_EVENT_MAX_PORTS_PER_DEV];
	int i;

	for (i = 0; i < event_dev->data->nb_ports; i++)
		hws[i] = i;
	roc_sso_hws_gwc_invalidate(&dev->sso, hws, event_dev->data->nb_ports);
	cnxk_sso_stop(event_dev, cnxk_sso_hws_reset, cn20k_sso_hws_flush_events);
}

static int
cn20k_sso_close(struct rte_eventdev *event_dev)
{
	return cnxk_sso_close(event_dev, cn20k_sso_hws_unlink);
}

static int
cn20k_sso_selftest(void)
{
	return cnxk_sso_selftest(RTE_STR(event_cn20k));
}

static int
cn20k_sso_rx_adapter_caps_get(const struct rte_eventdev *event_dev,
			      const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int rc;

	RTE_SET_USED(event_dev);
	rc = strncmp(eth_dev->device->driver->name, "net_cn20k", 9);
	if (rc)
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	else
		*caps = RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_MULTI_EVENTQ |
			RTE_EVENT_ETH_RX_ADAPTER_CAP_OVERRIDE_FLOW_ID;

	return 0;
}

static void
cn20k_sso_set_priv_mem(const struct rte_eventdev *event_dev, void *lookup_mem)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		struct cn20k_sso_hws *ws = event_dev->data->ports[i];
		ws->xaq_lmt = dev->xaq_lmt;
		ws->fc_mem = (int64_t __rte_atomic *)dev->fc_iova;
		ws->tstamp = dev->tstamp;
		if (lookup_mem)
			ws->lookup_mem = lookup_mem;
	}
}

static void
eventdev_fops_tstamp_update(struct rte_eventdev *event_dev)
{
	struct rte_event_fp_ops *fp_op = rte_event_fp_ops + event_dev->data->dev_id;

	fp_op->dequeue_burst = event_dev->dequeue_burst;
}

static void
cn20k_sso_tstamp_hdl_update(uint16_t port_id, uint16_t flags, bool ptp_en)
{
	struct rte_eth_dev *dev = &rte_eth_devices[port_id];
	struct cnxk_eth_dev *cnxk_eth_dev = dev->data->dev_private;
	struct rte_eventdev *event_dev = cnxk_eth_dev->evdev_priv;
	struct cnxk_sso_evdev *evdev = cnxk_sso_pmd_priv(event_dev);

	evdev->rx_offloads |= flags;
	if (ptp_en)
		evdev->tstamp[port_id] = &cnxk_eth_dev->tstamp;
	else
		evdev->tstamp[port_id] = NULL;
	cn20k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	eventdev_fops_tstamp_update(event_dev);
}

static int
cn20k_sso_rx_adapter_queue_add(const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev, int32_t rx_queue_id,
			       const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	struct cnxk_eth_dev *cnxk_eth_dev = eth_dev->data->dev_private;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_sso_hwgrp_stash stash;
	struct cn20k_eth_rxq *rxq;
	void *lookup_mem;
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (rc)
		return -EINVAL;

	rc = cnxk_sso_rx_adapter_queue_add(event_dev, eth_dev, rx_queue_id, queue_conf);
	if (rc)
		return -EINVAL;

	cnxk_eth_dev->cnxk_sso_ptp_tstamp_cb = cn20k_sso_tstamp_hdl_update;
	cnxk_eth_dev->evdev_priv = (struct rte_eventdev *)(uintptr_t)event_dev;

	rxq = eth_dev->data->rx_queues[0];
	lookup_mem = rxq->lookup_mem;
	cn20k_sso_set_priv_mem(event_dev, lookup_mem);
	cn20k_sso_fp_fns_set((struct rte_eventdev *)(uintptr_t)event_dev);
	if (roc_feature_sso_has_stash() && dev->nb_event_ports > 1) {
		stash.hwgrp = queue_conf->ev.queue_id;
		stash.stash_offset = CN20K_SSO_DEFAULT_STASH_OFFSET;
		stash.stash_count = CN20K_SSO_DEFAULT_STASH_LENGTH;
		rc = roc_sso_hwgrp_stash_config(&dev->sso, &stash, 1);
		if (rc < 0)
			plt_warn("failed to configure HWGRP WQE stashing rc = %d", rc);
	}

	return 0;
}

static int
cn20k_sso_rx_adapter_queue_del(const struct rte_eventdev *event_dev,
			       const struct rte_eth_dev *eth_dev, int32_t rx_queue_id)
{
	int rc;

	rc = strncmp(eth_dev->device->driver->name, "net_cn20k", 8);
	if (rc)
		return -EINVAL;

	return cnxk_sso_rx_adapter_queue_del(event_dev, eth_dev, rx_queue_id);
}

static struct eventdev_ops cn20k_sso_dev_ops = {
	.dev_infos_get = cn20k_sso_info_get,
	.dev_configure = cn20k_sso_dev_configure,

	.queue_def_conf = cnxk_sso_queue_def_conf,
	.queue_setup = cnxk_sso_queue_setup,
	.queue_release = cnxk_sso_queue_release,
	.queue_attr_set = cnxk_sso_queue_attribute_set,

	.port_def_conf = cnxk_sso_port_def_conf,
	.port_setup = cn20k_sso_port_setup,
	.port_release = cn20k_sso_port_release,
	.port_quiesce = cn20k_sso_port_quiesce,
	.port_link = cn20k_sso_port_link,
	.port_unlink = cn20k_sso_port_unlink,
	.port_link_profile = cn20k_sso_port_link_profile,
	.port_unlink_profile = cn20k_sso_port_unlink_profile,
	.timeout_ticks = cnxk_sso_timeout_ticks,

	.eth_rx_adapter_caps_get = cn20k_sso_rx_adapter_caps_get,
	.eth_rx_adapter_queue_add = cn20k_sso_rx_adapter_queue_add,
	.eth_rx_adapter_queue_del = cn20k_sso_rx_adapter_queue_del,
	.eth_rx_adapter_start = cnxk_sso_rx_adapter_start,
	.eth_rx_adapter_stop = cnxk_sso_rx_adapter_stop,

	.xstats_get = cnxk_sso_xstats_get,
	.xstats_reset = cnxk_sso_xstats_reset,
	.xstats_get_names = cnxk_sso_xstats_get_names,

	.dump = cnxk_sso_dump,
	.dev_start = cn20k_sso_start,
	.dev_stop = cn20k_sso_stop,
	.dev_close = cn20k_sso_close,
	.dev_selftest = cn20k_sso_selftest,
};

static int
cn20k_sso_init(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	event_dev->dev_ops = &cn20k_sso_dev_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		cn20k_sso_fp_fns_set(event_dev);
		return 0;
	}

	rc = cnxk_sso_init(event_dev);
	if (rc < 0)
		return rc;

	cn20k_sso_set_rsrc(cnxk_sso_pmd_priv(event_dev));
	if (!dev->max_event_ports || !dev->max_event_queues) {
		plt_err("Not enough eventdev resource queues=%d ports=%d", dev->max_event_queues,
			dev->max_event_ports);
		cnxk_sso_fini(event_dev);
		return -ENODEV;
	}

	plt_sso_dbg("Initializing %s max_queues=%d max_ports=%d", event_dev->data->name,
		    dev->max_event_queues, dev->max_event_ports);

	return 0;
}

static int
cn20k_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev, sizeof(struct cnxk_sso_evdev),
				       cn20k_sso_init);
}

static const struct rte_pci_id cn20k_pci_sso_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN20KA, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN20KA, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn20k_pci_sso = {
	.id_table = cn20k_pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn20k_sso_probe,
	.remove = cnxk_sso_remove,
};

RTE_PMD_REGISTER_PCI(event_cn20k, cn20k_pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_cn20k, cn20k_pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_cn20k, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_cn20k,
			      CNXK_SSO_XAE_CNT "=<int>"
			      CNXK_SSO_GGRP_QOS "=<string>"
			      CNXK_SSO_STASH "=<string>"
			      CNXK_SSO_FORCE_BP "=1");
