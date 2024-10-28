/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#include "roc_api.h"

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
			      CNXK_SSO_STASH "=<string>");
