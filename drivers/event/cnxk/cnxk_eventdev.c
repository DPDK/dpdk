/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_eventdev.h"

void
cnxk_sso_info_get(struct cnxk_sso_evdev *dev,
		  struct rte_event_dev_info *dev_info)
{

	dev_info->min_dequeue_timeout_ns = dev->min_dequeue_timeout_ns;
	dev_info->max_dequeue_timeout_ns = dev->max_dequeue_timeout_ns;
	dev_info->max_event_queues = dev->max_event_queues;
	dev_info->max_event_queue_flows = (1ULL << 20);
	dev_info->max_event_queue_priority_levels = 8;
	dev_info->max_event_priority_levels = 1;
	dev_info->max_event_ports = dev->max_event_ports;
	dev_info->max_event_port_dequeue_depth = 1;
	dev_info->max_event_port_enqueue_depth = 1;
	dev_info->max_num_events = dev->max_num_events;
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_QUEUE_QOS |
				  RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
				  RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES |
				  RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
				  RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
				  RTE_EVENT_DEV_CAP_NONSEQ_MODE |
				  RTE_EVENT_DEV_CAP_CARRY_FLOW_ID;
}

int
cnxk_sso_xaq_allocate(struct cnxk_sso_evdev *dev)
{
	char pool_name[RTE_MEMZONE_NAMESIZE];
	uint32_t xaq_cnt, npa_aura_id;
	const struct rte_memzone *mz;
	struct npa_aura_s *aura;
	static int reconfig_cnt;
	int rc;

	if (dev->xaq_pool) {
		rc = roc_sso_hwgrp_release_xaq(&dev->sso, dev->nb_event_queues);
		if (rc < 0) {
			plt_err("Failed to release XAQ %d", rc);
			return rc;
		}
		rte_mempool_free(dev->xaq_pool);
		dev->xaq_pool = NULL;
	}

	/*
	 * Allocate memory for Add work backpressure.
	 */
	mz = rte_memzone_lookup(CNXK_SSO_FC_NAME);
	if (mz == NULL)
		mz = rte_memzone_reserve_aligned(CNXK_SSO_FC_NAME,
						 sizeof(struct npa_aura_s) +
							 RTE_CACHE_LINE_SIZE,
						 0, 0, RTE_CACHE_LINE_SIZE);
	if (mz == NULL) {
		plt_err("Failed to allocate mem for fcmem");
		return -ENOMEM;
	}

	dev->fc_iova = mz->iova;
	dev->fc_mem = mz->addr;

	aura = (struct npa_aura_s *)((uintptr_t)dev->fc_mem +
				     RTE_CACHE_LINE_SIZE);
	memset(aura, 0, sizeof(struct npa_aura_s));

	aura->fc_ena = 1;
	aura->fc_addr = dev->fc_iova;
	aura->fc_hyst_bits = 0; /* Store count on all updates */

	/* Taken from HRM 14.3.3(4) */
	xaq_cnt = dev->nb_event_queues * CNXK_SSO_XAQ_CACHE_CNT;
	if (dev->xae_cnt)
		xaq_cnt += dev->xae_cnt / dev->sso.xae_waes;
	else
		xaq_cnt += (dev->sso.iue / dev->sso.xae_waes) +
			   (CNXK_SSO_XAQ_SLACK * dev->nb_event_queues);

	plt_sso_dbg("Configuring %d xaq buffers", xaq_cnt);
	/* Setup XAQ based on number of nb queues. */
	snprintf(pool_name, 30, "cnxk_xaq_buf_pool_%d", reconfig_cnt);
	dev->xaq_pool = (void *)rte_mempool_create_empty(
		pool_name, xaq_cnt, dev->sso.xaq_buf_size, 0, 0,
		rte_socket_id(), 0);

	if (dev->xaq_pool == NULL) {
		plt_err("Unable to create empty mempool.");
		rte_memzone_free(mz);
		return -ENOMEM;
	}

	rc = rte_mempool_set_ops_byname(dev->xaq_pool,
					rte_mbuf_platform_mempool_ops(), aura);
	if (rc != 0) {
		plt_err("Unable to set xaqpool ops.");
		goto alloc_fail;
	}

	rc = rte_mempool_populate_default(dev->xaq_pool);
	if (rc < 0) {
		plt_err("Unable to set populate xaqpool.");
		goto alloc_fail;
	}
	reconfig_cnt++;
	/* When SW does addwork (enqueue) check if there is space in XAQ by
	 * comparing fc_addr above against the xaq_lmt calculated below.
	 * There should be a minimum headroom (CNXK_SSO_XAQ_SLACK / 2) for SSO
	 * to request XAQ to cache them even before enqueue is called.
	 */
	dev->xaq_lmt =
		xaq_cnt - (CNXK_SSO_XAQ_SLACK / 2 * dev->nb_event_queues);
	dev->nb_xaq_cfg = xaq_cnt;

	npa_aura_id = roc_npa_aura_handle_to_aura(dev->xaq_pool->pool_id);
	return roc_sso_hwgrp_alloc_xaq(&dev->sso, npa_aura_id,
				       dev->nb_event_queues);
alloc_fail:
	rte_mempool_free(dev->xaq_pool);
	rte_memzone_free(mz);
	return rc;
}

int
cnxk_sso_dev_validate(const struct rte_eventdev *event_dev)
{
	struct rte_event_dev_config *conf = &event_dev->data->dev_conf;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint32_t deq_tmo_ns;
	int rc;

	deq_tmo_ns = conf->dequeue_timeout_ns;

	if (deq_tmo_ns == 0)
		deq_tmo_ns = dev->min_dequeue_timeout_ns;
	if (deq_tmo_ns < dev->min_dequeue_timeout_ns ||
	    deq_tmo_ns > dev->max_dequeue_timeout_ns) {
		plt_err("Unsupported dequeue timeout requested");
		return -EINVAL;
	}

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT)
		dev->is_timeout_deq = 1;

	dev->deq_tmo_ns = deq_tmo_ns;

	if (!conf->nb_event_queues || !conf->nb_event_ports ||
	    conf->nb_event_ports > dev->max_event_ports ||
	    conf->nb_event_queues > dev->max_event_queues) {
		plt_err("Unsupported event queues/ports requested");
		return -EINVAL;
	}

	if (conf->nb_event_port_dequeue_depth > 1) {
		plt_err("Unsupported event port deq depth requested");
		return -EINVAL;
	}

	if (conf->nb_event_port_enqueue_depth > 1) {
		plt_err("Unsupported event port enq depth requested");
		return -EINVAL;
	}

	if (dev->xaq_pool) {
		rc = roc_sso_hwgrp_release_xaq(&dev->sso, dev->nb_event_queues);
		if (rc < 0) {
			plt_err("Failed to release XAQ %d", rc);
			return rc;
		}
		rte_mempool_free(dev->xaq_pool);
		dev->xaq_pool = NULL;
	}

	dev->nb_event_queues = conf->nb_event_queues;
	dev->nb_event_ports = conf->nb_event_ports;

	return 0;
}

void
cnxk_sso_queue_def_conf(struct rte_eventdev *event_dev, uint8_t queue_id,
			struct rte_event_queue_conf *queue_conf)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(queue_id);

	queue_conf->nb_atomic_flows = (1ULL << 20);
	queue_conf->nb_atomic_order_sequences = (1ULL << 20);
	queue_conf->event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
}

int
cnxk_sso_queue_setup(struct rte_eventdev *event_dev, uint8_t queue_id,
		     const struct rte_event_queue_conf *queue_conf)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	plt_sso_dbg("Queue=%d prio=%d", queue_id, queue_conf->priority);
	/* Normalize <0-255> to <0-7> */
	return roc_sso_hwgrp_set_priority(&dev->sso, queue_id, 0xFF, 0xFF,
					  queue_conf->priority / 32);
}

void
cnxk_sso_queue_release(struct rte_eventdev *event_dev, uint8_t queue_id)
{
	RTE_SET_USED(event_dev);
	RTE_SET_USED(queue_id);
}

void
cnxk_sso_port_def_conf(struct rte_eventdev *event_dev, uint8_t port_id,
		       struct rte_event_port_conf *port_conf)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	RTE_SET_USED(port_id);
	port_conf->new_event_threshold = dev->max_num_events;
	port_conf->dequeue_depth = 1;
	port_conf->enqueue_depth = 1;
}

static void
cnxk_sso_parse_devargs(struct cnxk_sso_evdev *dev, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;

	if (devargs == NULL)
		return;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	rte_kvargs_process(kvlist, CNXK_SSO_XAE_CNT, &parse_kvargs_value,
			   &dev->xae_cnt);
	rte_kvargs_free(kvlist);
}

int
cnxk_sso_init(struct rte_eventdev *event_dev)
{
	const struct rte_memzone *mz = NULL;
	struct rte_pci_device *pci_dev;
	struct cnxk_sso_evdev *dev;
	int rc;

	mz = rte_memzone_reserve(CNXK_SSO_MZ_NAME, sizeof(uint64_t),
				 SOCKET_ID_ANY, 0);
	if (mz == NULL) {
		plt_err("Failed to create eventdev memzone");
		return -ENOMEM;
	}

	dev = cnxk_sso_pmd_priv(event_dev);
	pci_dev = container_of(event_dev->dev, struct rte_pci_device, device);
	dev->sso.pci_dev = pci_dev;

	*(uint64_t *)mz->addr = (uint64_t)dev;
	cnxk_sso_parse_devargs(dev, pci_dev->device.devargs);

	/* Initialize the base cnxk_dev object */
	rc = roc_sso_dev_init(&dev->sso);
	if (rc < 0) {
		plt_err("Failed to initialize RoC SSO rc=%d", rc);
		goto error;
	}

	dev->is_timeout_deq = 0;
	dev->min_dequeue_timeout_ns = USEC2NSEC(1);
	dev->max_dequeue_timeout_ns = USEC2NSEC(0x3FF);
	dev->max_num_events = -1;
	dev->nb_event_queues = 0;
	dev->nb_event_ports = 0;

	return 0;

error:
	rte_memzone_free(mz);
	return rc;
}

int
cnxk_sso_fini(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	/* For secondary processes, nothing to be done */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	roc_sso_rsrc_fini(&dev->sso);
	roc_sso_dev_fini(&dev->sso);

	return 0;
}

int
cnxk_sso_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, cnxk_sso_fini);
}
