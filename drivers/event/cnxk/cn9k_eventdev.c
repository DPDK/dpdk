/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_eventdev.h"

#define CN9K_DUAL_WS_NB_WS	    2
#define CN9K_DUAL_WS_PAIR_ID(x, id) (((x)*CN9K_DUAL_WS_NB_WS) + id)

static void
cn9k_init_hws_ops(struct cn9k_sso_hws_state *ws, uintptr_t base)
{
	ws->tag_op = base + SSOW_LF_GWS_TAG;
	ws->wqp_op = base + SSOW_LF_GWS_WQP;
	ws->getwrk_op = base + SSOW_LF_GWS_OP_GET_WORK0;
	ws->swtag_flush_op = base + SSOW_LF_GWS_OP_SWTAG_FLUSH;
	ws->swtag_norm_op = base + SSOW_LF_GWS_OP_SWTAG_NORM;
	ws->swtag_desched_op = base + SSOW_LF_GWS_OP_SWTAG_DESCHED;
}

static void
cn9k_sso_hws_setup(void *arg, void *hws, uintptr_t *grps_base)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	uint64_t val;

	/* Set get_work tmo for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns) - 1;
	if (dev->dual_ws) {
		dws = hws;
		rte_memcpy(dws->grps_base, grps_base,
			   sizeof(uintptr_t) * CNXK_SSO_MAX_HWGRP);
		dws->fc_mem = dev->fc_mem;
		dws->xaq_lmt = dev->xaq_lmt;

		plt_write64(val, dws->base[0] + SSOW_LF_GWS_NW_TIM);
		plt_write64(val, dws->base[1] + SSOW_LF_GWS_NW_TIM);
	} else {
		ws = hws;
		rte_memcpy(ws->grps_base, grps_base,
			   sizeof(uintptr_t) * CNXK_SSO_MAX_HWGRP);
		ws->fc_mem = dev->fc_mem;
		ws->xaq_lmt = dev->xaq_lmt;

		plt_write64(val, ws->base + SSOW_LF_GWS_NW_TIM);
	}
}

static void
cn9k_sso_hws_release(void *arg, void *hws)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;

	if (dev->dual_ws) {
		dws = hws;
		memset(dws, 0, sizeof(*dws));
	} else {
		ws = hws;
		memset(ws, 0, sizeof(*ws));
	}
}

static void
cn9k_sso_set_rsrc(void *arg)
{
	struct cnxk_sso_evdev *dev = arg;

	if (dev->dual_ws)
		dev->max_event_ports = dev->sso.max_hws / CN9K_DUAL_WS_NB_WS;
	else
		dev->max_event_ports = dev->sso.max_hws;
	dev->max_event_queues =
		dev->sso.max_hwgrp > RTE_EVENT_MAX_QUEUES_PER_DEV ?
			      RTE_EVENT_MAX_QUEUES_PER_DEV :
			      dev->sso.max_hwgrp;
}

static int
cn9k_sso_rsrc_init(void *arg, uint8_t hws, uint8_t hwgrp)
{
	struct cnxk_sso_evdev *dev = arg;

	if (dev->dual_ws)
		hws = hws * CN9K_DUAL_WS_NB_WS;

	return roc_sso_rsrc_init(&dev->sso, hws, hwgrp);
}

static void *
cn9k_sso_init_hws_mem(void *arg, uint8_t port_id)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn9k_sso_hws_dual *dws;
	struct cn9k_sso_hws *ws;
	void *data;

	if (dev->dual_ws) {
		dws = rte_zmalloc("cn9k_dual_ws",
				  sizeof(struct cn9k_sso_hws_dual) +
					  RTE_CACHE_LINE_SIZE,
				  RTE_CACHE_LINE_SIZE);
		if (dws == NULL) {
			plt_err("Failed to alloc memory for port=%d", port_id);
			return NULL;
		}

		dws = RTE_PTR_ADD(dws, sizeof(struct cnxk_sso_hws_cookie));
		dws->base[0] = roc_sso_hws_base_get(
			&dev->sso, CN9K_DUAL_WS_PAIR_ID(port_id, 0));
		dws->base[1] = roc_sso_hws_base_get(
			&dev->sso, CN9K_DUAL_WS_PAIR_ID(port_id, 1));
		cn9k_init_hws_ops(&dws->ws_state[0], dws->base[0]);
		cn9k_init_hws_ops(&dws->ws_state[1], dws->base[1]);
		dws->hws_id = port_id;
		dws->swtag_req = 0;
		dws->vws = 0;

		data = dws;
	} else {
		/* Allocate event port memory */
		ws = rte_zmalloc("cn9k_ws",
				 sizeof(struct cn9k_sso_hws) +
					 RTE_CACHE_LINE_SIZE,
				 RTE_CACHE_LINE_SIZE);
		if (ws == NULL) {
			plt_err("Failed to alloc memory for port=%d", port_id);
			return NULL;
		}

		/* First cache line is reserved for cookie */
		ws = RTE_PTR_ADD(ws, sizeof(struct cnxk_sso_hws_cookie));
		ws->base = roc_sso_hws_base_get(&dev->sso, port_id);
		cn9k_init_hws_ops((struct cn9k_sso_hws_state *)ws, ws->base);
		ws->hws_id = port_id;
		ws->swtag_req = 0;

		data = ws;
	}

	return data;
}

static void
cn9k_sso_info_get(struct rte_eventdev *event_dev,
		  struct rte_event_dev_info *dev_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_CN9K_PMD);
	cnxk_sso_info_get(dev, dev_info);
}

static int
cn9k_sso_dev_configure(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = cnxk_sso_dev_validate(event_dev);
	if (rc < 0) {
		plt_err("Invalid event device configuration");
		return -EINVAL;
	}

	roc_sso_rsrc_fini(&dev->sso);

	rc = cn9k_sso_rsrc_init(dev, dev->nb_event_ports, dev->nb_event_queues);
	if (rc < 0) {
		plt_err("Failed to initialize SSO resources");
		return -ENODEV;
	}

	rc = cnxk_sso_xaq_allocate(dev);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	rc = cnxk_setup_event_ports(event_dev, cn9k_sso_init_hws_mem,
				    cn9k_sso_hws_setup);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	return 0;
cnxk_rsrc_fini:
	roc_sso_rsrc_fini(&dev->sso);
	dev->nb_event_ports = 0;
	return rc;
}

static int
cn9k_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		    const struct rte_event_port_conf *port_conf)
{

	RTE_SET_USED(port_conf);
	return cnxk_sso_port_setup(event_dev, port_id, cn9k_sso_hws_setup);
}

static void
cn9k_sso_port_release(void *port)
{
	struct cnxk_sso_hws_cookie *gws_cookie = cnxk_sso_hws_get_cookie(port);
	struct cnxk_sso_evdev *dev;

	if (port == NULL)
		return;

	dev = cnxk_sso_pmd_priv(gws_cookie->event_dev);
	if (!gws_cookie->configured)
		goto free;

	cn9k_sso_hws_release(dev, port);
	memset(gws_cookie, 0, sizeof(*gws_cookie));
free:
	rte_free(gws_cookie);
}

static struct rte_eventdev_ops cn9k_sso_dev_ops = {
	.dev_infos_get = cn9k_sso_info_get,
	.dev_configure = cn9k_sso_dev_configure,
	.queue_def_conf = cnxk_sso_queue_def_conf,
	.queue_setup = cnxk_sso_queue_setup,
	.queue_release = cnxk_sso_queue_release,
	.port_def_conf = cnxk_sso_port_def_conf,
	.port_setup = cn9k_sso_port_setup,
	.port_release = cn9k_sso_port_release,
};

static int
cn9k_sso_init(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	if (RTE_CACHE_LINE_SIZE != 128) {
		plt_err("Driver not compiled for CN9K");
		return -EFAULT;
	}

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	event_dev->dev_ops = &cn9k_sso_dev_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rc = cnxk_sso_init(event_dev);
	if (rc < 0)
		return rc;

	cn9k_sso_set_rsrc(cnxk_sso_pmd_priv(event_dev));
	if (!dev->max_event_ports || !dev->max_event_queues) {
		plt_err("Not enough eventdev resource queues=%d ports=%d",
			dev->max_event_queues, dev->max_event_ports);
		cnxk_sso_fini(event_dev);
		return -ENODEV;
	}

	plt_sso_dbg("Initializing %s max_queues=%d max_ports=%d",
		    event_dev->data->name, dev->max_event_queues,
		    dev->max_event_ports);

	return 0;
}

static int
cn9k_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(
		pci_drv, pci_dev, sizeof(struct cnxk_sso_evdev), cn9k_sso_init);
}

static const struct rte_pci_id cn9k_pci_sso_map[] = {
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn9k_pci_sso = {
	.id_table = cn9k_pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn9k_sso_probe,
	.remove = cnxk_sso_remove,
};

RTE_PMD_REGISTER_PCI(event_cn9k, cn9k_pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_cn9k, cn9k_pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_cn9k, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_cn9k, CNXK_SSO_XAE_CNT "=<int>"
			      CNXK_SSO_GGRP_QOS "=<string>");
