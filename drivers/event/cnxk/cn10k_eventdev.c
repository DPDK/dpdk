/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_eventdev.h"

static void
cn10k_init_hws_ops(struct cn10k_sso_hws *ws, uintptr_t base)
{
	ws->tag_wqe_op = base + SSOW_LF_GWS_WQE0;
	ws->getwrk_op = base + SSOW_LF_GWS_OP_GET_WORK0;
	ws->updt_wqe_op = base + SSOW_LF_GWS_OP_UPD_WQP_GRP1;
	ws->swtag_norm_op = base + SSOW_LF_GWS_OP_SWTAG_NORM;
	ws->swtag_untag_op = base + SSOW_LF_GWS_OP_SWTAG_UNTAG;
	ws->swtag_flush_op = base + SSOW_LF_GWS_OP_SWTAG_FLUSH;
	ws->swtag_desched_op = base + SSOW_LF_GWS_OP_SWTAG_DESCHED;
}

static uint32_t
cn10k_sso_gw_mode_wdata(struct cnxk_sso_evdev *dev)
{
	uint32_t wdata = BIT(16) | 1;

	switch (dev->gw_mode) {
	case CN10K_GW_MODE_NONE:
	default:
		break;
	case CN10K_GW_MODE_PREF:
		wdata |= BIT(19);
		break;
	case CN10K_GW_MODE_PREF_WFE:
		wdata |= BIT(20) | BIT(19);
		break;
	}

	return wdata;
}

static void *
cn10k_sso_init_hws_mem(void *arg, uint8_t port_id)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws;

	/* Allocate event port memory */
	ws = rte_zmalloc("cn10k_ws",
			 sizeof(struct cn10k_sso_hws) + RTE_CACHE_LINE_SIZE,
			 RTE_CACHE_LINE_SIZE);
	if (ws == NULL) {
		plt_err("Failed to alloc memory for port=%d", port_id);
		return NULL;
	}

	/* First cache line is reserved for cookie */
	ws = (struct cn10k_sso_hws *)((uint8_t *)ws + RTE_CACHE_LINE_SIZE);
	ws->base = roc_sso_hws_base_get(&dev->sso, port_id);
	cn10k_init_hws_ops(ws, ws->base);
	ws->hws_id = port_id;
	ws->swtag_req = 0;
	ws->gw_wdata = cn10k_sso_gw_mode_wdata(dev);
	ws->lmt_base = dev->sso.lmt_base;

	return ws;
}

static void
cn10k_sso_hws_setup(void *arg, void *hws, uintptr_t *grps_base)
{
	struct cnxk_sso_evdev *dev = arg;
	struct cn10k_sso_hws *ws = hws;
	uint64_t val;

	rte_memcpy(ws->grps_base, grps_base,
		   sizeof(uintptr_t) * CNXK_SSO_MAX_HWGRP);
	ws->fc_mem = dev->fc_mem;
	ws->xaq_lmt = dev->xaq_lmt;

	/* Set get_work timeout for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns) - 1;
	plt_write64(val, ws->base + SSOW_LF_GWS_NW_TIM);
}

static void
cn10k_sso_hws_release(void *arg, void *hws)
{
	struct cn10k_sso_hws *ws = hws;

	RTE_SET_USED(arg);
	memset(ws, 0, sizeof(*ws));
}

static void
cn10k_sso_set_rsrc(void *arg)
{
	struct cnxk_sso_evdev *dev = arg;

	dev->max_event_ports = dev->sso.max_hws;
	dev->max_event_queues =
		dev->sso.max_hwgrp > RTE_EVENT_MAX_QUEUES_PER_DEV ?
			      RTE_EVENT_MAX_QUEUES_PER_DEV :
			      dev->sso.max_hwgrp;
}

static int
cn10k_sso_rsrc_init(void *arg, uint8_t hws, uint8_t hwgrp)
{
	struct cnxk_sso_evdev *dev = arg;

	return roc_sso_rsrc_init(&dev->sso, hws, hwgrp);
}

static void
cn10k_sso_info_get(struct rte_eventdev *event_dev,
		   struct rte_event_dev_info *dev_info)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_CN10K_PMD);
	cnxk_sso_info_get(dev, dev_info);
}

static int
cn10k_sso_dev_configure(const struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	rc = cnxk_sso_dev_validate(event_dev);
	if (rc < 0) {
		plt_err("Invalid event device configuration");
		return -EINVAL;
	}

	roc_sso_rsrc_fini(&dev->sso);

	rc = cn10k_sso_rsrc_init(dev, dev->nb_event_ports,
				 dev->nb_event_queues);
	if (rc < 0) {
		plt_err("Failed to initialize SSO resources");
		return -ENODEV;
	}

	rc = cnxk_sso_xaq_allocate(dev);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	rc = cnxk_setup_event_ports(event_dev, cn10k_sso_init_hws_mem,
				    cn10k_sso_hws_setup);
	if (rc < 0)
		goto cnxk_rsrc_fini;

	return 0;
cnxk_rsrc_fini:
	roc_sso_rsrc_fini(&dev->sso);
	dev->nb_event_ports = 0;
	return rc;
}

static int
cn10k_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		     const struct rte_event_port_conf *port_conf)
{

	RTE_SET_USED(port_conf);
	return cnxk_sso_port_setup(event_dev, port_id, cn10k_sso_hws_setup);
}

static void
cn10k_sso_port_release(void *port)
{
	struct cnxk_sso_hws_cookie *gws_cookie = cnxk_sso_hws_get_cookie(port);
	struct cnxk_sso_evdev *dev;

	if (port == NULL)
		return;

	dev = cnxk_sso_pmd_priv(gws_cookie->event_dev);
	if (!gws_cookie->configured)
		goto free;

	cn10k_sso_hws_release(dev, port);
	memset(gws_cookie, 0, sizeof(*gws_cookie));
free:
	rte_free(gws_cookie);
}

static struct rte_eventdev_ops cn10k_sso_dev_ops = {
	.dev_infos_get = cn10k_sso_info_get,
	.dev_configure = cn10k_sso_dev_configure,
	.queue_def_conf = cnxk_sso_queue_def_conf,
	.queue_setup = cnxk_sso_queue_setup,
	.queue_release = cnxk_sso_queue_release,
	.port_def_conf = cnxk_sso_port_def_conf,
	.port_setup = cn10k_sso_port_setup,
	.port_release = cn10k_sso_port_release,
};

static int
cn10k_sso_init(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc;

	if (RTE_CACHE_LINE_SIZE != 64) {
		plt_err("Driver not compiled for CN9K");
		return -EFAULT;
	}

	rc = roc_plt_init();
	if (rc < 0) {
		plt_err("Failed to initialize platform model");
		return rc;
	}

	event_dev->dev_ops = &cn10k_sso_dev_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rc = cnxk_sso_init(event_dev);
	if (rc < 0)
		return rc;

	cn10k_sso_set_rsrc(cnxk_sso_pmd_priv(event_dev));
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
cn10k_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev,
				       sizeof(struct cnxk_sso_evdev),
				       cn10k_sso_init);
}

static const struct rte_pci_id cn10k_pci_sso_map[] = {
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_SSO_TIM_PF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KA, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	CNXK_PCI_ID(PCI_SUBSYSTEM_DEVID_CN10KAS, PCI_DEVID_CNXK_RVU_SSO_TIM_VF),
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver cn10k_pci_sso = {
	.id_table = cn10k_pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA,
	.probe = cn10k_sso_probe,
	.remove = cnxk_sso_remove,
};

RTE_PMD_REGISTER_PCI(event_cn10k, cn10k_pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_cn10k, cn10k_pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_cn10k, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_cn10k, CNXK_SSO_XAE_CNT "=<int>"
			      CNXK_SSO_GGRP_QOS "=<string>");
