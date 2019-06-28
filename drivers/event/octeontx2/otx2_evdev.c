/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <inttypes.h>

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_eal.h>
#include <rte_eventdev_pmd_pci.h>
#include <rte_kvargs.h>
#include <rte_mbuf_pool_ops.h>
#include <rte_pci.h>

#include "otx2_evdev.h"

static void
otx2_sso_info_get(struct rte_eventdev *event_dev,
		  struct rte_event_dev_info *dev_info)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_OCTEONTX2_PMD);
	dev_info->min_dequeue_timeout_ns = dev->min_dequeue_timeout_ns;
	dev_info->max_dequeue_timeout_ns = dev->max_dequeue_timeout_ns;
	dev_info->max_event_queues = dev->max_event_queues;
	dev_info->max_event_queue_flows = (1ULL << 20);
	dev_info->max_event_queue_priority_levels = 8;
	dev_info->max_event_priority_levels = 1;
	dev_info->max_event_ports = dev->max_event_ports;
	dev_info->max_event_port_dequeue_depth = 1;
	dev_info->max_event_port_enqueue_depth = 1;
	dev_info->max_num_events =  dev->max_num_events;
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_QUEUE_QOS |
					RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
					RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES |
					RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
					RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
					RTE_EVENT_DEV_CAP_NONSEQ_MODE;
}

static int
sso_hw_lf_cfg(struct otx2_mbox *mbox, enum otx2_sso_lf_type type,
	      uint16_t nb_lf, uint8_t attach)
{
	if (attach) {
		struct rsrc_attach_req *req;

		req = otx2_mbox_alloc_msg_attach_resources(mbox);
		switch (type) {
		case SSO_LF_GGRP:
			req->sso = nb_lf;
			break;
		case SSO_LF_GWS:
			req->ssow = nb_lf;
			break;
		default:
			return -EINVAL;
		}
		req->modify = true;
		if (otx2_mbox_process(mbox) < 0)
			return -EIO;
	} else {
		struct rsrc_detach_req *req;

		req = otx2_mbox_alloc_msg_detach_resources(mbox);
		switch (type) {
		case SSO_LF_GGRP:
			req->sso = true;
			break;
		case SSO_LF_GWS:
			req->ssow = true;
			break;
		default:
			return -EINVAL;
		}
		req->partial = true;
		if (otx2_mbox_process(mbox) < 0)
			return -EIO;
	}

	return 0;
}

static int
sso_lf_cfg(struct otx2_sso_evdev *dev, struct otx2_mbox *mbox,
	   enum otx2_sso_lf_type type, uint16_t nb_lf, uint8_t alloc)
{
	void *rsp;
	int rc;

	if (alloc) {
		switch (type) {
		case SSO_LF_GGRP:
			{
			struct sso_lf_alloc_req *req_ggrp;
			req_ggrp = otx2_mbox_alloc_msg_sso_lf_alloc(mbox);
			req_ggrp->hwgrps = nb_lf;
			}
			break;
		case SSO_LF_GWS:
			{
			struct ssow_lf_alloc_req *req_hws;
			req_hws = otx2_mbox_alloc_msg_ssow_lf_alloc(mbox);
			req_hws->hws = nb_lf;
			}
			break;
		default:
			return -EINVAL;
		}
	} else {
		switch (type) {
		case SSO_LF_GGRP:
			{
			struct sso_lf_free_req *req_ggrp;
			req_ggrp = otx2_mbox_alloc_msg_sso_lf_free(mbox);
			req_ggrp->hwgrps = nb_lf;
			}
			break;
		case SSO_LF_GWS:
			{
			struct ssow_lf_free_req *req_hws;
			req_hws = otx2_mbox_alloc_msg_ssow_lf_free(mbox);
			req_hws->hws = nb_lf;
			}
			break;
		default:
			return -EINVAL;
		}
	}

	rc = otx2_mbox_process_msg_tmo(mbox, (void **)&rsp, ~0);
	if (rc < 0)
		return rc;

	if (alloc && type == SSO_LF_GGRP) {
		struct sso_lf_alloc_rsp *rsp_ggrp = rsp;

		dev->xaq_buf_size = rsp_ggrp->xaq_buf_size;
		dev->xae_waes = rsp_ggrp->xaq_wq_entries;
		dev->iue = rsp_ggrp->in_unit_entries;
	}

	return 0;
}

static void
otx2_sso_port_release(void *port)
{
	rte_free(port);
}

static void
otx2_sso_queue_release(struct rte_eventdev *event_dev, uint8_t queue_id)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(queue_id);
}

static void
sso_set_port_ops(struct otx2_ssogws *ws, uintptr_t base)
{
	ws->tag_op		= base + SSOW_LF_GWS_TAG;
	ws->wqp_op		= base + SSOW_LF_GWS_WQP;
	ws->getwrk_op		= base + SSOW_LF_GWS_OP_GET_WORK;
	ws->swtp_op		= base + SSOW_LF_GWS_SWTP;
	ws->swtag_norm_op	= base + SSOW_LF_GWS_OP_SWTAG_NORM;
	ws->swtag_desched_op	= base + SSOW_LF_GWS_OP_SWTAG_DESCHED;
}

static int
sso_configure_ports(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_mbox *mbox = dev->mbox;
	uint8_t nb_lf;
	int i, rc;

	otx2_sso_dbg("Configuring event ports %d", dev->nb_event_ports);

	nb_lf = dev->nb_event_ports;
	/* Ask AF to attach required LFs. */
	rc = sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, true);
	if (rc < 0) {
		otx2_err("Failed to attach SSO GWS LF");
		return -ENODEV;
	}

	if (sso_lf_cfg(dev, mbox, SSO_LF_GWS, nb_lf, true) < 0) {
		sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, false);
		otx2_err("Failed to init SSO GWS LF");
		return -ENODEV;
	}

	for (i = 0; i < nb_lf; i++) {
		struct otx2_ssogws *ws;
		uintptr_t base;

		/* Free memory prior to re-allocation if needed */
		if (event_dev->data->ports[i] != NULL) {
			ws = event_dev->data->ports[i];
			rte_free(ws);
			ws = NULL;
		}

		/* Allocate event port memory */
		ws = rte_zmalloc_socket("otx2_sso_ws",
					sizeof(struct otx2_ssogws),
					RTE_CACHE_LINE_SIZE,
					event_dev->data->socket_id);
		if (ws == NULL) {
			otx2_err("Failed to alloc memory for port=%d", i);
			rc = -ENOMEM;
			break;
		}

		ws->port = i;
		base = dev->bar2 + (RVU_BLOCK_ADDR_SSOW << 20 | i << 12);
		sso_set_port_ops(ws, base);

		event_dev->data->ports[i] = ws;
	}

	if (rc < 0) {
		sso_lf_cfg(dev, mbox, SSO_LF_GWS, nb_lf, false);
		sso_hw_lf_cfg(mbox, SSO_LF_GWS, nb_lf, false);
	}

	return rc;
}

static int
sso_configure_queues(const struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_mbox *mbox = dev->mbox;
	uint8_t nb_lf;
	int rc;

	otx2_sso_dbg("Configuring event queues %d", dev->nb_event_queues);

	nb_lf = dev->nb_event_queues;
	/* Ask AF to attach required LFs. */
	rc = sso_hw_lf_cfg(mbox, SSO_LF_GGRP, nb_lf, true);
	if (rc < 0) {
		otx2_err("Failed to attach SSO GGRP LF");
		return -ENODEV;
	}

	if (sso_lf_cfg(dev, mbox, SSO_LF_GGRP, nb_lf, true) < 0) {
		sso_hw_lf_cfg(mbox, SSO_LF_GGRP, nb_lf, false);
		otx2_err("Failed to init SSO GGRP LF");
		return -ENODEV;
	}

	return rc;
}

static int
sso_xaq_allocate(struct otx2_sso_evdev *dev)
{
	const struct rte_memzone *mz;
	struct npa_aura_s *aura;
	static int reconfig_cnt;
	char pool_name[RTE_MEMZONE_NAMESIZE];
	uint32_t xaq_cnt;
	int rc;

	if (dev->xaq_pool)
		rte_mempool_free(dev->xaq_pool);

	/*
	 * Allocate memory for Add work backpressure.
	 */
	mz = rte_memzone_lookup(OTX2_SSO_FC_NAME);
	if (mz == NULL)
		mz = rte_memzone_reserve_aligned(OTX2_SSO_FC_NAME,
						 OTX2_ALIGN +
						 sizeof(struct npa_aura_s),
						 rte_socket_id(),
						 RTE_MEMZONE_IOVA_CONTIG,
						 OTX2_ALIGN);
	if (mz == NULL) {
		otx2_err("Failed to allocate mem for fcmem");
		return -ENOMEM;
	}

	dev->fc_iova = mz->iova;
	dev->fc_mem = mz->addr;

	aura = (struct npa_aura_s *)((uintptr_t)dev->fc_mem + OTX2_ALIGN);
	memset(aura, 0, sizeof(struct npa_aura_s));

	aura->fc_ena = 1;
	aura->fc_addr = dev->fc_iova;
	aura->fc_hyst_bits = 0; /* Store count on all updates */

	/* Taken from HRM 14.3.3(4) */
	xaq_cnt = dev->nb_event_queues * OTX2_SSO_XAQ_CACHE_CNT;
	if (dev->xae_cnt)
		xaq_cnt += dev->xae_cnt / dev->xae_waes;
	else
		xaq_cnt += (dev->iue / dev->xae_waes) +
			(OTX2_SSO_XAQ_SLACK * dev->nb_event_queues);

	otx2_sso_dbg("Configuring %d xaq buffers", xaq_cnt);
	/* Setup XAQ based on number of nb queues. */
	snprintf(pool_name, 30, "otx2_xaq_buf_pool_%d", reconfig_cnt);
	dev->xaq_pool = (void *)rte_mempool_create_empty(pool_name,
			xaq_cnt, dev->xaq_buf_size, 0, 0,
			rte_socket_id(), 0);

	if (dev->xaq_pool == NULL) {
		otx2_err("Unable to create empty mempool.");
		rte_memzone_free(mz);
		return -ENOMEM;
	}

	rc = rte_mempool_set_ops_byname(dev->xaq_pool,
					rte_mbuf_platform_mempool_ops(), aura);
	if (rc != 0) {
		otx2_err("Unable to set xaqpool ops.");
		goto alloc_fail;
	}

	rc = rte_mempool_populate_default(dev->xaq_pool);
	if (rc < 0) {
		otx2_err("Unable to set populate xaqpool.");
		goto alloc_fail;
	}
	reconfig_cnt++;
	/* When SW does addwork (enqueue) check if there is space in XAQ by
	 * comparing fc_addr above against the xaq_lmt calculated below.
	 * There should be a minimum headroom (OTX2_SSO_XAQ_SLACK / 2) for SSO
	 * to request XAQ to cache them even before enqueue is called.
	 */
	dev->xaq_lmt = xaq_cnt - (OTX2_SSO_XAQ_SLACK / 2 *
				  dev->nb_event_queues);
	dev->nb_xaq_cfg = xaq_cnt;

	return 0;
alloc_fail:
	rte_mempool_free(dev->xaq_pool);
	rte_memzone_free(mz);
	return rc;
}

static int
sso_ggrp_alloc_xaq(struct otx2_sso_evdev *dev)
{
	struct otx2_mbox *mbox = dev->mbox;
	struct sso_hw_setconfig *req;

	otx2_sso_dbg("Configuring XAQ for GGRPs");
	req = otx2_mbox_alloc_msg_sso_hw_setconfig(mbox);
	req->npa_pf_func = otx2_npa_pf_func_get();
	req->npa_aura_id = npa_lf_aura_handle_to_aura(dev->xaq_pool->pool_id);
	req->hwgrps = dev->nb_event_queues;

	return otx2_mbox_process(mbox);
}

static void
sso_lf_teardown(struct otx2_sso_evdev *dev,
		enum otx2_sso_lf_type lf_type)
{
	uint8_t nb_lf;

	switch (lf_type) {
	case SSO_LF_GGRP:
		nb_lf = dev->nb_event_queues;
		break;
	case SSO_LF_GWS:
		nb_lf = dev->nb_event_ports;
		break;
	default:
		return;
	}

	sso_lf_cfg(dev, dev->mbox, lf_type, nb_lf, false);
	sso_hw_lf_cfg(dev->mbox, lf_type, nb_lf, false);
}

static int
otx2_sso_configure(const struct rte_eventdev *event_dev)
{
	struct rte_event_dev_config *conf = &event_dev->data->dev_conf;
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uint32_t deq_tmo_ns;
	int rc;

	sso_func_trace();
	deq_tmo_ns = conf->dequeue_timeout_ns;

	if (deq_tmo_ns == 0)
		deq_tmo_ns = dev->min_dequeue_timeout_ns;

	if (deq_tmo_ns < dev->min_dequeue_timeout_ns ||
	    deq_tmo_ns > dev->max_dequeue_timeout_ns) {
		otx2_err("Unsupported dequeue timeout requested");
		return -EINVAL;
	}

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		dev->is_timeout_deq = 1;

	dev->deq_tmo_ns = deq_tmo_ns;

	if (conf->nb_event_ports > dev->max_event_ports ||
	    conf->nb_event_queues > dev->max_event_queues) {
		otx2_err("Unsupported event queues/ports requested");
		return -EINVAL;
	}

	if (conf->nb_event_port_dequeue_depth > 1) {
		otx2_err("Unsupported event port deq depth requested");
		return -EINVAL;
	}

	if (conf->nb_event_port_enqueue_depth > 1) {
		otx2_err("Unsupported event port enq depth requested");
		return -EINVAL;
	}

	if (dev->nb_event_queues) {
		/* Finit any previous queues. */
		sso_lf_teardown(dev, SSO_LF_GGRP);
	}
	if (dev->nb_event_ports) {
		/* Finit any previous ports. */
		sso_lf_teardown(dev, SSO_LF_GWS);
	}

	dev->nb_event_queues = conf->nb_event_queues;
	dev->nb_event_ports = conf->nb_event_ports;

	if (sso_configure_ports(event_dev)) {
		otx2_err("Failed to configure event ports");
		return -ENODEV;
	}

	if (sso_configure_queues(event_dev) < 0) {
		otx2_err("Failed to configure event queues");
		rc = -ENODEV;
		goto teardown_hws;
	}

	if (sso_xaq_allocate(dev) < 0) {
		rc = -ENOMEM;
		goto teardown_hwggrp;
	}

	rc = sso_ggrp_alloc_xaq(dev);
	if (rc < 0) {
		otx2_err("Failed to alloc xaq to ggrp %d", rc);
		goto teardown_hwggrp;
	}

	dev->configured = 1;
	rte_mb();

	return 0;
teardown_hwggrp:
	sso_lf_teardown(dev, SSO_LF_GGRP);
teardown_hws:
	sso_lf_teardown(dev, SSO_LF_GWS);
	dev->nb_event_queues = 0;
	dev->nb_event_ports = 0;
	dev->configured = 0;
	return rc;
}

static void
otx2_sso_queue_def_conf(struct rte_eventdev *event_dev, uint8_t queue_id,
			struct rte_event_queue_conf *queue_conf)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(queue_id);

	queue_conf->nb_atomic_flows = (1ULL << 20);
	queue_conf->nb_atomic_order_sequences = (1ULL << 20);
	queue_conf->event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
}

static int
otx2_sso_queue_setup(struct rte_eventdev *event_dev, uint8_t queue_id,
		     const struct rte_event_queue_conf *queue_conf)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct otx2_mbox *mbox = dev->mbox;
	struct sso_grp_priority *req;
	int rc;

	sso_func_trace("Queue=%d prio=%d", queue_id, queue_conf->priority);

	req = otx2_mbox_alloc_msg_sso_grp_set_priority(dev->mbox);
	req->grp = queue_id;
	req->weight = 0xFF;
	req->affinity = 0xFF;
	/* Normalize <0-255> to <0-7> */
	req->priority = queue_conf->priority / 32;

	rc = otx2_mbox_process(mbox);
	if (rc < 0) {
		otx2_err("Failed to set priority queue=%d", queue_id);
		return rc;
	}

	return 0;
}

static void
otx2_sso_port_def_conf(struct rte_eventdev *event_dev, uint8_t port_id,
		       struct rte_event_port_conf *port_conf)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);

	RTE_SET_USED(port_id);
	port_conf->new_event_threshold = dev->max_num_events;
	port_conf->dequeue_depth = 1;
	port_conf->enqueue_depth = 1;
}

static int
otx2_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		    const struct rte_event_port_conf *port_conf)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	uintptr_t grps_base[OTX2_SSO_MAX_VHGRP] = {0};
	uint64_t val;
	uint16_t q;

	sso_func_trace("Port=%d", port_id);
	RTE_SET_USED(port_conf);

	if (event_dev->data->ports[port_id] == NULL) {
		otx2_err("Invalid port Id %d", port_id);
		return -EINVAL;
	}

	for (q = 0; q < dev->nb_event_queues; q++) {
		grps_base[q] = dev->bar2 + (RVU_BLOCK_ADDR_SSO << 20 | q << 12);
		if (grps_base[q] == 0) {
			otx2_err("Failed to get grp[%d] base addr", q);
			return -EINVAL;
		}
	}

	/* Set get_work timeout for HWS */
	val = NSEC2USEC(dev->deq_tmo_ns) - 1;

	struct otx2_ssogws *ws = event_dev->data->ports[port_id];
	uintptr_t base = OTX2_SSOW_GET_BASE_ADDR(ws->getwrk_op);

	rte_memcpy(ws->grps_base, grps_base,
		   sizeof(uintptr_t) * OTX2_SSO_MAX_VHGRP);
	ws->fc_mem = dev->fc_mem;
	ws->xaq_lmt = dev->xaq_lmt;
	otx2_write64(val, base + SSOW_LF_GWS_NW_TIM);

	otx2_sso_dbg("Port=%d ws=%p", port_id, event_dev->data->ports[port_id]);

	return 0;
}

/* Initialize and register event driver with DPDK Application */
static struct rte_eventdev_ops otx2_sso_ops = {
	.dev_infos_get    = otx2_sso_info_get,
	.dev_configure    = otx2_sso_configure,
	.queue_def_conf   = otx2_sso_queue_def_conf,
	.queue_setup      = otx2_sso_queue_setup,
	.queue_release    = otx2_sso_queue_release,
	.port_def_conf    = otx2_sso_port_def_conf,
	.port_setup       = otx2_sso_port_setup,
	.port_release     = otx2_sso_port_release,
};

#define OTX2_SSO_XAE_CNT	"xae_cnt"

static void
sso_parse_devargs(struct otx2_sso_evdev *dev, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;

	if (devargs == NULL)
		return;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	rte_kvargs_process(kvlist, OTX2_SSO_XAE_CNT, &parse_kvargs_value,
			   &dev->xae_cnt);

	rte_kvargs_free(kvlist);
}

static int
otx2_sso_probe(struct rte_pci_driver *pci_drv, struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_probe(pci_drv, pci_dev,
				       sizeof(struct otx2_sso_evdev),
				       otx2_sso_init);
}

static int
otx2_sso_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, otx2_sso_fini);
}

static const struct rte_pci_id pci_sso_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_RVU_SSO_TIM_PF)
	},
	{
		.vendor_id = 0,
	},
};

static struct rte_pci_driver pci_sso = {
	.id_table = pci_sso_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = otx2_sso_probe,
	.remove = otx2_sso_remove,
};

int
otx2_sso_init(struct rte_eventdev *event_dev)
{
	struct free_rsrcs_rsp *rsrc_cnt;
	struct rte_pci_device *pci_dev;
	struct otx2_sso_evdev *dev;
	int rc;

	event_dev->dev_ops = &otx2_sso_ops;
	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dev = sso_pmd_priv(event_dev);

	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);

	/* Initialize the base otx2_dev object */
	rc = otx2_dev_init(pci_dev, dev);
	if (rc < 0) {
		otx2_err("Failed to initialize otx2_dev rc=%d", rc);
		goto error;
	}

	/* Get SSO and SSOW MSIX rsrc cnt */
	otx2_mbox_alloc_msg_free_rsrc_cnt(dev->mbox);
	rc = otx2_mbox_process_msg(dev->mbox, (void *)&rsrc_cnt);
	if (rc < 0) {
		otx2_err("Unable to get free rsrc count");
		goto otx2_dev_uninit;
	}
	otx2_sso_dbg("SSO %d SSOW %d NPA %d provisioned", rsrc_cnt->sso,
		     rsrc_cnt->ssow, rsrc_cnt->npa);

	dev->max_event_ports = RTE_MIN(rsrc_cnt->ssow, OTX2_SSO_MAX_VHWS);
	dev->max_event_queues = RTE_MIN(rsrc_cnt->sso, OTX2_SSO_MAX_VHGRP);
	/* Grab the NPA LF if required */
	rc = otx2_npa_lf_init(pci_dev, dev);
	if (rc < 0) {
		otx2_err("Unable to init NPA lf. It might not be provisioned");
		goto otx2_dev_uninit;
	}

	dev->drv_inited = true;
	dev->is_timeout_deq = 0;
	dev->min_dequeue_timeout_ns = USEC2NSEC(1);
	dev->max_dequeue_timeout_ns = USEC2NSEC(0x3FF);
	dev->max_num_events = -1;
	dev->nb_event_queues = 0;
	dev->nb_event_ports = 0;

	if (!dev->max_event_ports || !dev->max_event_queues) {
		otx2_err("Not enough eventdev resource queues=%d ports=%d",
			 dev->max_event_queues, dev->max_event_ports);
		rc = -ENODEV;
		goto otx2_npa_lf_uninit;
	}

	sso_parse_devargs(dev, pci_dev->device.devargs);

	otx2_sso_pf_func_set(dev->pf_func);
	otx2_sso_dbg("Initializing %s max_queues=%d max_ports=%d",
		     event_dev->data->name, dev->max_event_queues,
		     dev->max_event_ports);


	return 0;

otx2_npa_lf_uninit:
	otx2_npa_lf_fini();
otx2_dev_uninit:
	otx2_dev_fini(pci_dev, dev);
error:
	return rc;
}

int
otx2_sso_fini(struct rte_eventdev *event_dev)
{
	struct otx2_sso_evdev *dev = sso_pmd_priv(event_dev);
	struct rte_pci_device *pci_dev;

	/* For secondary processes, nothing to be done */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);

	if (!dev->drv_inited)
		goto dev_fini;

	dev->drv_inited = false;
	otx2_npa_lf_fini();

dev_fini:
	if (otx2_npa_lf_active(dev)) {
		otx2_info("Common resource in use by other devices");
		return -EAGAIN;
	}

	otx2_dev_fini(pci_dev, dev);

	return 0;
}

RTE_PMD_REGISTER_PCI(event_octeontx2, pci_sso);
RTE_PMD_REGISTER_PCI_TABLE(event_octeontx2, pci_sso_map);
RTE_PMD_REGISTER_KMOD_DEP(event_octeontx2, "vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(event_octeontx2, OTX2_SSO_XAE_CNT "=<int>");
