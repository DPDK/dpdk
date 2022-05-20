/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include "cnxk_cryptodev_ops.h"
#include "cnxk_eventdev.h"

static int
crypto_adapter_qp_setup(const struct rte_cryptodev *cdev,
			struct cnxk_cpt_qp *qp)
{
	char name[RTE_MEMPOOL_NAMESIZE];
	uint32_t cache_size, nb_req;
	unsigned int req_size;

	snprintf(name, RTE_MEMPOOL_NAMESIZE, "cnxk_ca_req_%u:%u",
		 cdev->data->dev_id, qp->lf.lf_id);
	req_size = sizeof(struct cpt_inflight_req);
	cache_size = RTE_MIN(RTE_MEMPOOL_CACHE_MAX_SIZE, qp->lf.nb_desc / 1.5);
	nb_req = RTE_MAX(qp->lf.nb_desc, cache_size * rte_lcore_count());
	qp->ca.req_mp = rte_mempool_create(name, nb_req, req_size, cache_size,
					   0, NULL, NULL, NULL, NULL,
					   rte_socket_id(), 0);
	if (qp->ca.req_mp == NULL)
		return -ENOMEM;

	qp->ca.enabled = true;

	return 0;
}

int
cnxk_crypto_adapter_qp_add(const struct rte_eventdev *event_dev,
			   const struct rte_cryptodev *cdev,
			   int32_t queue_pair_id)
{
	struct cnxk_sso_evdev *sso_evdev = cnxk_sso_pmd_priv(event_dev);
	uint32_t adptr_xae_cnt = 0;
	struct cnxk_cpt_qp *qp;
	int ret;

	if (queue_pair_id == -1) {
		uint16_t qp_id;

		for (qp_id = 0; qp_id < cdev->data->nb_queue_pairs; qp_id++) {
			qp = cdev->data->queue_pairs[qp_id];
			ret = crypto_adapter_qp_setup(cdev, qp);
			if (ret) {
				cnxk_crypto_adapter_qp_del(cdev, -1);
				return ret;
			}
			adptr_xae_cnt += qp->ca.req_mp->size;
		}
	} else {
		qp = cdev->data->queue_pairs[queue_pair_id];
		ret = crypto_adapter_qp_setup(cdev, qp);
		if (ret)
			return ret;
		adptr_xae_cnt = qp->ca.req_mp->size;
	}

	/* Update crypto adapter XAE count */
	sso_evdev->adptr_xae_cnt += adptr_xae_cnt;
	cnxk_sso_xae_reconfigure((struct rte_eventdev *)(uintptr_t)event_dev);

	return 0;
}

static int
crypto_adapter_qp_free(struct cnxk_cpt_qp *qp)
{
	rte_mempool_free(qp->ca.req_mp);
	qp->ca.enabled = false;

	return 0;
}

int
cnxk_crypto_adapter_qp_del(const struct rte_cryptodev *cdev,
			   int32_t queue_pair_id)
{
	struct cnxk_cpt_qp *qp;

	if (queue_pair_id == -1) {
		uint16_t qp_id;

		for (qp_id = 0; qp_id < cdev->data->nb_queue_pairs; qp_id++) {
			qp = cdev->data->queue_pairs[qp_id];
			if (qp->ca.enabled)
				crypto_adapter_qp_free(qp);
		}
	} else {
		qp = cdev->data->queue_pairs[queue_pair_id];
		if (qp->ca.enabled)
			crypto_adapter_qp_free(qp);
	}

	return 0;
}

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
				  RTE_EVENT_DEV_CAP_CARRY_FLOW_ID |
				  RTE_EVENT_DEV_CAP_MAINTENANCE_FREE;
}

int
cnxk_sso_xaq_allocate(struct cnxk_sso_evdev *dev)
{
	uint32_t xae_cnt;
	int rc;

	xae_cnt = dev->sso.iue;
	if (dev->xae_cnt)
		xae_cnt += dev->xae_cnt;
	if (dev->adptr_xae_cnt)
		xae_cnt += (dev->adptr_xae_cnt);

	plt_sso_dbg("Configuring %d xae buffers", xae_cnt);
	rc = roc_sso_hwgrp_init_xaq_aura(&dev->sso, xae_cnt);
	if (rc < 0) {
		plt_err("Failed to configure XAQ aura");
		return rc;
	}
	dev->xaq_lmt = dev->sso.xaq.xaq_lmt;
	dev->fc_iova = (uint64_t)dev->sso.xaq.fc;

	return roc_sso_hwgrp_alloc_xaq(
		&dev->sso,
		roc_npa_aura_handle_to_aura(dev->sso.xaq.aura_handle),
		dev->nb_event_queues);
}

int
cnxk_sso_xae_reconfigure(struct rte_eventdev *event_dev)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int rc = 0;

	if (event_dev->data->dev_started)
		event_dev->dev_ops->dev_stop(event_dev);

	rc = cnxk_sso_xaq_allocate(dev);
	if (rc < 0) {
		plt_err("Failed to alloc XAQ %d", rc);
		return rc;
	}

	rte_mb();
	if (event_dev->data->dev_started)
		event_dev->dev_ops->dev_start(event_dev);

	return 0;
}

int
cnxk_setup_event_ports(const struct rte_eventdev *event_dev,
		       cnxk_sso_init_hws_mem_t init_hws_fn,
		       cnxk_sso_hws_setup_t setup_hws_fn)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	int i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		struct cnxk_sso_hws_cookie *ws_cookie;
		void *ws;

		/* Free memory prior to re-allocation if needed */
		if (event_dev->data->ports[i] != NULL)
			ws = event_dev->data->ports[i];
		else
			ws = init_hws_fn(dev, i);
		if (ws == NULL)
			goto hws_fini;
		ws_cookie = cnxk_sso_hws_get_cookie(ws);
		ws_cookie->event_dev = event_dev;
		ws_cookie->configured = 1;
		event_dev->data->ports[i] = ws;
		cnxk_sso_port_setup((struct rte_eventdev *)(uintptr_t)event_dev,
				    i, setup_hws_fn);
	}

	return 0;
hws_fini:
	for (i = i - 1; i >= 0; i--) {
		event_dev->data->ports[i] = NULL;
		rte_free(cnxk_sso_hws_get_cookie(event_dev->data->ports[i]));
	}
	return -ENOMEM;
}

void
cnxk_sso_restore_links(const struct rte_eventdev *event_dev,
		       cnxk_sso_link_t link_fn)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t *links_map, hwgrp[CNXK_SSO_MAX_HWGRP];
	int i, j;

	for (i = 0; i < dev->nb_event_ports; i++) {
		uint16_t nb_hwgrp = 0;

		links_map = event_dev->data->links_map;
		/* Point links_map to this port specific area */
		links_map += (i * RTE_EVENT_MAX_QUEUES_PER_DEV);

		for (j = 0; j < dev->nb_event_queues; j++) {
			if (links_map[j] == 0xdead)
				continue;
			hwgrp[nb_hwgrp] = j;
			nb_hwgrp++;
		}

		link_fn(dev, event_dev->data->ports[i], hwgrp, nb_hwgrp);
	}
}

int
cnxk_sso_dev_validate(const struct rte_eventdev *event_dev)
{
	struct rte_event_dev_config *conf = &event_dev->data->dev_conf;
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint32_t deq_tmo_ns;

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

	roc_sso_rsrc_fini(&dev->sso);
	roc_sso_hwgrp_free_xaq_aura(&dev->sso, dev->sso.nb_hwgrp);

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

int
cnxk_sso_port_setup(struct rte_eventdev *event_dev, uint8_t port_id,
		    cnxk_sso_hws_setup_t hws_setup_fn)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uintptr_t grp_base = 0;

	plt_sso_dbg("Port=%d", port_id);
	if (event_dev->data->ports[port_id] == NULL) {
		plt_err("Invalid port Id %d", port_id);
		return -EINVAL;
	}

	grp_base = roc_sso_hwgrp_base_get(&dev->sso, 0);
	if (grp_base == 0) {
		plt_err("Failed to get grp base addr");
		return -EINVAL;
	}

	hws_setup_fn(dev, event_dev->data->ports[port_id], grp_base);
	plt_sso_dbg("Port=%d ws=%p", port_id, event_dev->data->ports[port_id]);
	rte_mb();

	return 0;
}

int
cnxk_sso_timeout_ticks(struct rte_eventdev *event_dev, uint64_t ns,
		       uint64_t *tmo_ticks)
{
	RTE_SET_USED(event_dev);
	*tmo_ticks = NSEC2TICK(ns, rte_get_timer_hz());

	return 0;
}

void
cnxk_sso_dump(struct rte_eventdev *event_dev, FILE *f)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);

	roc_sso_dump(&dev->sso, dev->sso.nb_hws, dev->sso.nb_hwgrp, f);
}

static void
cnxk_handle_event(void *arg, struct rte_event event)
{
	struct rte_eventdev *event_dev = arg;

	if (event_dev->dev_ops->dev_stop_flush != NULL)
		event_dev->dev_ops->dev_stop_flush(
			event_dev->data->dev_id, event,
			event_dev->data->dev_stop_flush_arg);
}

static void
cnxk_sso_cleanup(struct rte_eventdev *event_dev, cnxk_sso_hws_reset_t reset_fn,
		 cnxk_sso_hws_flush_t flush_fn, uint8_t enable)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uintptr_t hwgrp_base;
	uint16_t i;
	void *ws;

	for (i = 0; i < dev->nb_event_ports; i++) {
		ws = event_dev->data->ports[i];
		reset_fn(dev, ws);
	}

	rte_mb();
	ws = event_dev->data->ports[0];

	for (i = 0; i < dev->nb_event_queues; i++) {
		/* Consume all the events through HWS0 */
		hwgrp_base = roc_sso_hwgrp_base_get(&dev->sso, i);
		flush_fn(ws, i, hwgrp_base, cnxk_handle_event, event_dev);
		/* Enable/Disable SSO GGRP */
		plt_write64(enable, hwgrp_base + SSO_LF_GGRP_QCTL);
	}
}

int
cnxk_sso_start(struct rte_eventdev *event_dev, cnxk_sso_hws_reset_t reset_fn,
	       cnxk_sso_hws_flush_t flush_fn)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	struct roc_sso_hwgrp_qos qos[dev->qos_queue_cnt];
	int i, rc;

	plt_sso_dbg();
	for (i = 0; i < dev->qos_queue_cnt; i++) {
		qos[i].hwgrp = dev->qos_parse_data[i].queue;
		qos[i].iaq_prcnt = dev->qos_parse_data[i].iaq_prcnt;
		qos[i].taq_prcnt = dev->qos_parse_data[i].taq_prcnt;
		qos[i].xaq_prcnt = dev->qos_parse_data[i].xaq_prcnt;
	}
	rc = roc_sso_hwgrp_qos_config(&dev->sso, qos, dev->qos_queue_cnt,
				      dev->xae_cnt);
	if (rc < 0) {
		plt_sso_dbg("failed to configure HWGRP QoS rc = %d", rc);
		return -EINVAL;
	}
	cnxk_sso_cleanup(event_dev, reset_fn, flush_fn, true);
	rte_mb();

	return 0;
}

void
cnxk_sso_stop(struct rte_eventdev *event_dev, cnxk_sso_hws_reset_t reset_fn,
	      cnxk_sso_hws_flush_t flush_fn)
{
	plt_sso_dbg();
	cnxk_sso_cleanup(event_dev, reset_fn, flush_fn, false);
	rte_mb();
}

int
cnxk_sso_close(struct rte_eventdev *event_dev, cnxk_sso_unlink_t unlink_fn)
{
	struct cnxk_sso_evdev *dev = cnxk_sso_pmd_priv(event_dev);
	uint16_t all_queues[CNXK_SSO_MAX_HWGRP];
	uint16_t i;
	void *ws;

	if (!dev->configured)
		return 0;

	for (i = 0; i < dev->nb_event_queues; i++)
		all_queues[i] = i;

	for (i = 0; i < dev->nb_event_ports; i++) {
		ws = event_dev->data->ports[i];
		unlink_fn(dev, ws, all_queues, dev->nb_event_queues);
		rte_free(cnxk_sso_hws_get_cookie(ws));
		event_dev->data->ports[i] = NULL;
	}

	roc_sso_rsrc_fini(&dev->sso);

	dev->fc_iova = 0;
	dev->configured = false;
	dev->is_timeout_deq = 0;
	dev->nb_event_ports = 0;
	dev->max_num_events = -1;
	dev->nb_event_queues = 0;
	dev->min_dequeue_timeout_ns = USEC2NSEC(1);
	dev->max_dequeue_timeout_ns = USEC2NSEC(0x3FF);

	return 0;
}

static void
parse_queue_param(char *value, void *opaque)
{
	struct cnxk_sso_qos queue_qos = {0};
	uint16_t *val = (uint16_t *)&queue_qos;
	struct cnxk_sso_evdev *dev = opaque;
	char *tok = strtok(value, "-");
	struct cnxk_sso_qos *old_ptr;

	if (!strlen(value))
		return;

	while (tok != NULL) {
		*val = atoi(tok);
		tok = strtok(NULL, "-");
		val++;
	}

	if (val != (&queue_qos.iaq_prcnt + 1)) {
		plt_err("Invalid QoS parameter expected [Qx-XAQ-TAQ-IAQ]");
		return;
	}

	dev->qos_queue_cnt++;
	old_ptr = dev->qos_parse_data;
	dev->qos_parse_data = rte_realloc(
		dev->qos_parse_data,
		sizeof(struct cnxk_sso_qos) * dev->qos_queue_cnt, 0);
	if (dev->qos_parse_data == NULL) {
		dev->qos_parse_data = old_ptr;
		dev->qos_queue_cnt--;
		return;
	}
	dev->qos_parse_data[dev->qos_queue_cnt - 1] = queue_qos;
}

static void
parse_qos_list(const char *value, void *opaque)
{
	char *s = strdup(value);
	char *start = NULL;
	char *end = NULL;
	char *f = s;

	while (*s) {
		if (*s == '[')
			start = s;
		else if (*s == ']')
			end = s;

		if (start && start < end) {
			*end = 0;
			parse_queue_param(start + 1, opaque);
			s = end;
			start = end;
		}
		s++;
	}

	free(f);
}

static int
parse_sso_kvargs_dict(const char *key, const char *value, void *opaque)
{
	RTE_SET_USED(key);

	/* Dict format [Qx-XAQ-TAQ-IAQ][Qz-XAQ-TAQ-IAQ] use '-' cause ','
	 * isn't allowed. Everything is expressed in percentages, 0 represents
	 * default.
	 */
	parse_qos_list(value, opaque);

	return 0;
}

static void
cnxk_sso_parse_devargs(struct cnxk_sso_evdev *dev, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	uint8_t single_ws = 0;

	if (devargs == NULL)
		return;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	rte_kvargs_process(kvlist, CNXK_SSO_XAE_CNT, &parse_kvargs_value,
			   &dev->xae_cnt);
	rte_kvargs_process(kvlist, CNXK_SSO_GGRP_QOS, &parse_sso_kvargs_dict,
			   dev);
	rte_kvargs_process(kvlist, CNXK_SSO_FORCE_BP, &parse_kvargs_flag,
			   &dev->force_ena_bp);
	rte_kvargs_process(kvlist, CN9K_SSO_SINGLE_WS, &parse_kvargs_flag,
			   &single_ws);
	rte_kvargs_process(kvlist, CN10K_SSO_GW_MODE, &parse_kvargs_flag,
			   &dev->gw_mode);
	dev->dual_ws = !single_ws;
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

	cnxk_tim_init(&dev->sso);

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

	cnxk_tim_fini();
	roc_sso_rsrc_fini(&dev->sso);
	roc_sso_dev_fini(&dev->sso);

	return 0;
}

int
cnxk_sso_remove(struct rte_pci_device *pci_dev)
{
	return rte_event_pmd_pci_remove(pci_dev, cnxk_sso_fini);
}
