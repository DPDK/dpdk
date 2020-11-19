/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <inttypes.h>

#include <rte_common.h>
#include <rte_debug.h>
#include <rte_dev.h>
#include <rte_eal.h>
#include <rte_ethdev_driver.h>
#include <rte_event_eth_rx_adapter.h>
#include <rte_kvargs.h>
#include <rte_lcore.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_memory.h>
#include <rte_bus_vdev.h>

#include "ssovf_evdev.h"
#include "timvf_evdev.h"

static uint8_t timvf_enable_stats;

RTE_LOG_REGISTER(otx_logtype_ssovf, pmd.event.octeontx, NOTICE);

/* SSOPF Mailbox messages */

struct ssovf_mbox_dev_info {
	uint64_t min_deq_timeout_ns;
	uint64_t max_deq_timeout_ns;
	uint32_t max_num_events;
};

static int
ssovf_mbox_dev_info(struct ssovf_mbox_dev_info *info)
{
	struct octeontx_mbox_hdr hdr = {0};
	uint16_t len = sizeof(struct ssovf_mbox_dev_info);

	hdr.coproc = SSO_COPROC;
	hdr.msg = SSO_GET_DEV_INFO;
	hdr.vfid = 0;

	memset(info, 0, len);
	return octeontx_mbox_send(&hdr, NULL, 0, info, len);
}

struct ssovf_mbox_getwork_wait {
	uint64_t wait_ns;
};

static int
ssovf_mbox_getwork_tmo_set(uint32_t timeout_ns)
{
	struct octeontx_mbox_hdr hdr = {0};
	struct ssovf_mbox_getwork_wait tmo_set;
	uint16_t len = sizeof(struct ssovf_mbox_getwork_wait);
	int ret;

	hdr.coproc = SSO_COPROC;
	hdr.msg = SSO_SET_GETWORK_WAIT;
	hdr.vfid = 0;

	tmo_set.wait_ns = timeout_ns;
	ret = octeontx_mbox_send(&hdr, &tmo_set, len, NULL, 0);
	if (ret)
		ssovf_log_err("Failed to set getwork timeout(%d)", ret);

	return ret;
}

struct ssovf_mbox_grp_pri {
	uint8_t vhgrp_id;
	uint8_t wgt_left; /* Read only */
	uint8_t weight;
	uint8_t affinity;
	uint8_t priority;
};

static int
ssovf_mbox_priority_set(uint8_t queue, uint8_t prio)
{
	struct octeontx_mbox_hdr hdr = {0};
	struct ssovf_mbox_grp_pri grp;
	uint16_t len = sizeof(struct ssovf_mbox_grp_pri);
	int ret;

	hdr.coproc = SSO_COPROC;
	hdr.msg = SSO_GRP_SET_PRIORITY;
	hdr.vfid = queue;

	grp.vhgrp_id = queue;
	grp.weight = 0xff;
	grp.affinity = 0xff;
	grp.priority = prio / 32; /* Normalize to 0 to 7 */

	ret = octeontx_mbox_send(&hdr, &grp, len, NULL, 0);
	if (ret)
		ssovf_log_err("Failed to set grp=%d prio=%d", queue, prio);

	return ret;
}

struct ssovf_mbox_convert_ns_getworks_iter {
	uint64_t wait_ns;
	uint32_t getwork_iter;/* Get_work iterations for the given wait_ns */
};

static int
ssovf_mbox_timeout_ticks(uint64_t ns, uint64_t *tmo_ticks)
{
	struct octeontx_mbox_hdr hdr = {0};
	struct ssovf_mbox_convert_ns_getworks_iter ns2iter;
	uint16_t len = sizeof(ns2iter);
	int ret;

	hdr.coproc = SSO_COPROC;
	hdr.msg = SSO_CONVERT_NS_GETWORK_ITER;
	hdr.vfid = 0;

	memset(&ns2iter, 0, len);
	ns2iter.wait_ns = ns;
	ret = octeontx_mbox_send(&hdr, &ns2iter, len, &ns2iter, len);
	if (ret < 0 || (ret != len)) {
		ssovf_log_err("Failed to get tmo ticks ns=%"PRId64"", ns);
		return -EIO;
	}

	*tmo_ticks = ns2iter.getwork_iter;
	return 0;
}

static void
ssovf_info_get(struct rte_eventdev *dev, struct rte_event_dev_info *dev_info)
{
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);

	dev_info->driver_name = RTE_STR(EVENTDEV_NAME_OCTEONTX_PMD);
	dev_info->min_dequeue_timeout_ns = edev->min_deq_timeout_ns;
	dev_info->max_dequeue_timeout_ns = edev->max_deq_timeout_ns;
	dev_info->max_event_queues = edev->max_event_queues;
	dev_info->max_event_queue_flows = (1ULL << 20);
	dev_info->max_event_queue_priority_levels = 8;
	dev_info->max_event_priority_levels = 1;
	dev_info->max_event_ports = edev->max_event_ports;
	dev_info->max_event_port_dequeue_depth = 1;
	dev_info->max_event_port_enqueue_depth = 1;
	dev_info->max_num_events =  edev->max_num_events;
	dev_info->event_dev_cap = RTE_EVENT_DEV_CAP_QUEUE_QOS |
					RTE_EVENT_DEV_CAP_DISTRIBUTED_SCHED |
					RTE_EVENT_DEV_CAP_QUEUE_ALL_TYPES|
					RTE_EVENT_DEV_CAP_RUNTIME_PORT_LINK |
					RTE_EVENT_DEV_CAP_MULTIPLE_QUEUE_PORT |
					RTE_EVENT_DEV_CAP_NONSEQ_MODE |
					RTE_EVENT_DEV_CAP_CARRY_FLOW_ID;

}

static int
ssovf_configure(const struct rte_eventdev *dev)
{
	struct rte_event_dev_config *conf = &dev->data->dev_conf;
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);
	uint64_t deq_tmo_ns;

	ssovf_func_trace();
	deq_tmo_ns = conf->dequeue_timeout_ns;
	if (deq_tmo_ns == 0)
		deq_tmo_ns = edev->min_deq_timeout_ns;

	if (conf->event_dev_cfg & RTE_EVENT_DEV_CFG_PER_DEQUEUE_TIMEOUT) {
		edev->is_timeout_deq = 1;
		deq_tmo_ns = edev->min_deq_timeout_ns;
	}
	edev->nb_event_queues = conf->nb_event_queues;
	edev->nb_event_ports = conf->nb_event_ports;

	return ssovf_mbox_getwork_tmo_set(deq_tmo_ns);
}

static void
ssovf_queue_def_conf(struct rte_eventdev *dev, uint8_t queue_id,
				 struct rte_event_queue_conf *queue_conf)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	queue_conf->nb_atomic_flows = (1ULL << 20);
	queue_conf->nb_atomic_order_sequences = (1ULL << 20);
	queue_conf->event_queue_cfg = RTE_EVENT_QUEUE_CFG_ALL_TYPES;
	queue_conf->priority = RTE_EVENT_DEV_PRIORITY_NORMAL;
}

static void
ssovf_queue_release(struct rte_eventdev *dev, uint8_t queue_id)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);
}

static int
ssovf_queue_setup(struct rte_eventdev *dev, uint8_t queue_id,
			      const struct rte_event_queue_conf *queue_conf)
{
	RTE_SET_USED(dev);
	ssovf_func_trace("queue=%d prio=%d", queue_id, queue_conf->priority);

	return ssovf_mbox_priority_set(queue_id, queue_conf->priority);
}

static void
ssovf_port_def_conf(struct rte_eventdev *dev, uint8_t port_id,
				 struct rte_event_port_conf *port_conf)
{
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);

	RTE_SET_USED(port_id);
	port_conf->new_event_threshold = edev->max_num_events;
	port_conf->dequeue_depth = 1;
	port_conf->enqueue_depth = 1;
	port_conf->event_port_cfg = 0;
}

static void
ssovf_port_release(void *port)
{
	rte_free(port);
}

static int
ssovf_port_setup(struct rte_eventdev *dev, uint8_t port_id,
				const struct rte_event_port_conf *port_conf)
{
	struct ssows *ws;
	uint32_t reg_off;
	uint8_t q;
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);

	ssovf_func_trace("port=%d", port_id);
	RTE_SET_USED(port_conf);

	/* Free memory prior to re-allocation if needed */
	if (dev->data->ports[port_id] != NULL) {
		ssovf_port_release(dev->data->ports[port_id]);
		dev->data->ports[port_id] = NULL;
	}

	/* Allocate event port memory */
	ws = rte_zmalloc_socket("eventdev ssows",
			sizeof(struct ssows), RTE_CACHE_LINE_SIZE,
			dev->data->socket_id);
	if (ws == NULL) {
		ssovf_log_err("Failed to alloc memory for port=%d", port_id);
		return -ENOMEM;
	}

	ws->base = ssovf_bar(OCTEONTX_SSO_HWS, port_id, 0);
	if (ws->base == NULL) {
		rte_free(ws);
		ssovf_log_err("Failed to get hws base addr port=%d", port_id);
		return -EINVAL;
	}

	reg_off = SSOW_VHWS_OP_GET_WORK0;
	reg_off |= 1 << 4; /* Index_ggrp_mask (Use maskset zero) */
	reg_off |= 1 << 16; /* Wait */
	ws->getwork = ws->base + reg_off;
	ws->port = port_id;
	ws->lookup_mem = octeontx_fastpath_lookup_mem_get();

	for (q = 0; q < edev->nb_event_queues; q++) {
		ws->grps[q] = ssovf_bar(OCTEONTX_SSO_GROUP, q, 2);
		if (ws->grps[q] == NULL) {
			rte_free(ws);
			ssovf_log_err("Failed to get grp%d base addr", q);
			return -EINVAL;
		}
	}

	dev->data->ports[port_id] = ws;
	ssovf_log_dbg("port=%d ws=%p", port_id, ws);
	return 0;
}

static int
ssovf_port_link(struct rte_eventdev *dev, void *port, const uint8_t queues[],
		const uint8_t priorities[], uint16_t nb_links)
{
	uint16_t link;
	uint64_t val;
	struct ssows *ws = port;

	ssovf_func_trace("port=%d nb_links=%d", ws->port, nb_links);
	RTE_SET_USED(dev);
	RTE_SET_USED(priorities);

	for (link = 0; link < nb_links; link++) {
		val = queues[link];
		val |= (1ULL << 24); /* Set membership */
		ssovf_write64(val, ws->base + SSOW_VHWS_GRPMSK_CHGX(0));
	}
	return (int)nb_links;
}

static int
ssovf_port_unlink(struct rte_eventdev *dev, void *port, uint8_t queues[],
			uint16_t nb_unlinks)
{
	uint16_t unlink;
	uint64_t val;
	struct ssows *ws = port;

	ssovf_func_trace("port=%d nb_links=%d", ws->port, nb_unlinks);
	RTE_SET_USED(dev);

	for (unlink = 0; unlink < nb_unlinks; unlink++) {
		val = queues[unlink];
		val &= ~(1ULL << 24); /* Clear membership */
		ssovf_write64(val, ws->base + SSOW_VHWS_GRPMSK_CHGX(0));
	}
	return (int)nb_unlinks;
}

static int
ssovf_timeout_ticks(struct rte_eventdev *dev, uint64_t ns, uint64_t *tmo_ticks)
{
	RTE_SET_USED(dev);

	return ssovf_mbox_timeout_ticks(ns, tmo_ticks);
}

static void
ssows_dump(struct ssows *ws, FILE *f)
{
	uint8_t *base = ws->base;
	uint64_t val;

	fprintf(f, "\t---------------port%d---------------\n", ws->port);
	val = ssovf_read64(base + SSOW_VHWS_TAG);
	fprintf(f, "\ttag=0x%x tt=%d head=%d tail=%d grp=%d index=%d tail=%d\n",
		(uint32_t)(val & 0xffffffff), (int)(val >> 32) & 0x3,
		(int)(val >> 34) & 0x1, (int)(val >> 35) & 0x1,
		(int)(val >> 36) & 0x3ff, (int)(val >> 48) & 0x3ff,
		(int)(val >> 63) & 0x1);

	val = ssovf_read64(base + SSOW_VHWS_WQP);
	fprintf(f, "\twqp=0x%"PRIx64"\n", val);

	val = ssovf_read64(base + SSOW_VHWS_LINKS);
	fprintf(f, "\tindex=%d valid=%d revlink=%d tail=%d head=%d grp=%d\n",
		(int)(val & 0x3ff), (int)(val >> 10) & 0x1,
		(int)(val >> 11) & 0x3ff, (int)(val >> 26) & 0x1,
		(int)(val >> 27) & 0x1, (int)(val >> 28) & 0x3ff);

	val = ssovf_read64(base + SSOW_VHWS_PENDTAG);
	fprintf(f, "\tptag=0x%x ptt=%d pgwi=%d pdesc=%d pgw=%d pgww=%d ps=%d\n",
		(uint32_t)(val & 0xffffffff), (int)(val >> 32) & 0x3,
		(int)(val >> 56) & 0x1, (int)(val >> 58) & 0x1,
		(int)(val >> 61) & 0x1, (int)(val >> 62) & 0x1,
		(int)(val >> 63) & 0x1);

	val = ssovf_read64(base + SSOW_VHWS_PENDWQP);
	fprintf(f, "\tpwqp=0x%"PRIx64"\n", val);
}

static int
ssovf_eth_rx_adapter_caps_get(const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int ret;
	RTE_SET_USED(dev);

	ret = strncmp(eth_dev->data->name, "eth_octeontx", 12);
	if (ret)
		*caps = RTE_EVENT_ETH_RX_ADAPTER_SW_CAP;
	else
		*caps = RTE_EVENT_ETH_RX_ADAPTER_CAP_INTERNAL_PORT;

	return 0;
}

static int
ssovf_eth_rx_adapter_queue_add(const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev, int32_t rx_queue_id,
		const struct rte_event_eth_rx_adapter_queue_conf *queue_conf)
{
	const struct octeontx_nic *nic = eth_dev->data->dev_private;
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);
	uint16_t free_idx = UINT16_MAX;
	struct octeontx_rxq *rxq;
	pki_mod_qos_t pki_qos;
	uint8_t found = false;
	int i, ret = 0;
	void *old_ptr;

	ret = strncmp(eth_dev->data->name, "eth_octeontx", 12);
	if (ret)
		return -EINVAL;

	if (queue_conf->ev.sched_type == RTE_SCHED_TYPE_PARALLEL)
		return -ENOTSUP;

	/* eth_octeontx only supports one rq. */
	rx_queue_id = rx_queue_id == -1 ? 0 : rx_queue_id;
	rxq = eth_dev->data->rx_queues[rx_queue_id];
	/* Add rxq pool to list of used pools and reduce available events. */
	for (i = 0; i < edev->rxq_pools; i++) {
		if (edev->rxq_pool_array[i] == (uintptr_t)rxq->pool) {
			edev->rxq_pool_rcnt[i]++;
			found = true;
			break;
		} else if (free_idx == UINT16_MAX &&
			   edev->rxq_pool_array[i] == 0) {
			free_idx = i;
		}
	}

	if (!found) {
		uint16_t idx;

		if (edev->available_events < rxq->pool->size) {
			ssovf_log_err(
				"Max available events %"PRIu32" requested events in rxq pool %"PRIu32"",
				edev->available_events, rxq->pool->size);
			return -ENOMEM;
		}

		if (free_idx != UINT16_MAX) {
			idx = free_idx;
		} else {
			old_ptr = edev->rxq_pool_array;
			edev->rxq_pools++;
			edev->rxq_pool_array = rte_realloc(
				edev->rxq_pool_array,
				sizeof(uint64_t) * edev->rxq_pools, 0);
			if (edev->rxq_pool_array == NULL) {
				edev->rxq_pools--;
				edev->rxq_pool_array = old_ptr;
				return -ENOMEM;
			}

			old_ptr = edev->rxq_pool_rcnt;
			edev->rxq_pool_rcnt = rte_realloc(
				edev->rxq_pool_rcnt,
				sizeof(uint8_t) * edev->rxq_pools, 0);
			if (edev->rxq_pool_rcnt == NULL) {
				edev->rxq_pools--;
				edev->rxq_pool_rcnt = old_ptr;
				return -ENOMEM;
			}
			idx = edev->rxq_pools - 1;
		}

		edev->rxq_pool_array[idx] = (uintptr_t)rxq->pool;
		edev->rxq_pool_rcnt[idx] = 1;
		edev->available_events -= rxq->pool->size;
	}

	memset(&pki_qos, 0, sizeof(pki_mod_qos_t));

	pki_qos.port_type = 0;
	pki_qos.index = 0;
	pki_qos.mmask.f_tag_type = 1;
	pki_qos.mmask.f_port_add = 1;
	pki_qos.mmask.f_grp_ok = 1;
	pki_qos.mmask.f_grp_bad = 1;
	pki_qos.mmask.f_grptag_ok = 1;
	pki_qos.mmask.f_grptag_bad = 1;

	pki_qos.qos_entry.tag_type = queue_conf->ev.sched_type;
	pki_qos.qos_entry.port_add = 0;
	pki_qos.qos_entry.ggrp_ok = queue_conf->ev.queue_id;
	pki_qos.qos_entry.ggrp_bad = queue_conf->ev.queue_id;
	pki_qos.qos_entry.grptag_bad = 0;
	pki_qos.qos_entry.grptag_ok = 0;

	ret = octeontx_pki_port_modify_qos(nic->port_id, &pki_qos);
	if (ret < 0)
		ssovf_log_err("failed to modify QOS, port=%d, q=%d",
				nic->port_id, queue_conf->ev.queue_id);

	edev->rx_offload_flags = nic->rx_offload_flags;
	edev->tx_offload_flags = nic->tx_offload_flags;
	return ret;
}

static int
ssovf_eth_rx_adapter_queue_del(const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev, int32_t rx_queue_id)
{
	const struct octeontx_nic *nic = eth_dev->data->dev_private;
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);
	struct octeontx_rxq *rxq;
	pki_del_qos_t pki_qos;
	uint8_t found = false;
	int i, ret = 0;

	rx_queue_id = rx_queue_id == -1 ? 0 : rx_queue_id;
	rxq = eth_dev->data->rx_queues[rx_queue_id];
	for (i = 0; i < edev->rxq_pools; i++) {
		if (edev->rxq_pool_array[i] == (uintptr_t)rxq->pool) {
			found = true;
			break;
		}
	}

	if (found) {
		edev->rxq_pool_rcnt[i]--;
		if (edev->rxq_pool_rcnt[i] == 0)
			edev->rxq_pool_array[i] = 0;
		edev->available_events += rxq->pool->size;
	}

	ret = strncmp(eth_dev->data->name, "eth_octeontx", 12);
	if (ret)
		return -EINVAL;

	pki_qos.port_type = 0;
	pki_qos.index = 0;
	memset(&pki_qos, 0, sizeof(pki_del_qos_t));
	ret = octeontx_pki_port_delete_qos(nic->port_id, &pki_qos);
	if (ret < 0)
		ssovf_log_err("Failed to delete QOS port=%d, q=%d",
				nic->port_id, rx_queue_id);
	return ret;
}

static int
ssovf_eth_rx_adapter_start(const struct rte_eventdev *dev,
					const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);

	return 0;
}


static int
ssovf_eth_rx_adapter_stop(const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev)
{
	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);

	return 0;
}

static int
ssovf_eth_tx_adapter_caps_get(const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev, uint32_t *caps)
{
	int ret;
	RTE_SET_USED(dev);

	ret = strncmp(eth_dev->data->name, "eth_octeontx", 12);
	if (ret)
		*caps = 0;
	else
		*caps = RTE_EVENT_ETH_TX_ADAPTER_CAP_INTERNAL_PORT;

	return 0;
}

static int
ssovf_eth_tx_adapter_create(uint8_t id, const struct rte_eventdev *dev)
{
	RTE_SET_USED(id);
	RTE_SET_USED(dev);
	return 0;
}

static int
ssovf_eth_tx_adapter_free(uint8_t id, const struct rte_eventdev *dev)
{
	RTE_SET_USED(id);
	RTE_SET_USED(dev);
	return 0;
}

static int
ssovf_eth_tx_adapter_queue_add(uint8_t id, const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev, int32_t tx_queue_id)
{
	RTE_SET_USED(id);
	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);
	RTE_SET_USED(tx_queue_id);
	return 0;
}

static int
ssovf_eth_tx_adapter_queue_del(uint8_t id, const struct rte_eventdev *dev,
		const struct rte_eth_dev *eth_dev, int32_t tx_queue_id)
{
	RTE_SET_USED(id);
	RTE_SET_USED(dev);
	RTE_SET_USED(eth_dev);
	RTE_SET_USED(tx_queue_id);
	return 0;
}

static int
ssovf_eth_tx_adapter_start(uint8_t id, const struct rte_eventdev *dev)
{
	RTE_SET_USED(id);
	RTE_SET_USED(dev);
	return 0;
}

static int
ssovf_eth_tx_adapter_stop(uint8_t id, const struct rte_eventdev *dev)
{
	RTE_SET_USED(id);
	RTE_SET_USED(dev);
	return 0;
}


static void
ssovf_dump(struct rte_eventdev *dev, FILE *f)
{
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);
	uint8_t port;

	/* Dump SSOWVF debug registers */
	for (port = 0; port < edev->nb_event_ports; port++)
		ssows_dump(dev->data->ports[port], f);
}

static int
ssovf_start(struct rte_eventdev *dev)
{
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);
	struct ssows *ws;
	uint8_t *base;
	uint8_t i;

	ssovf_func_trace();
	for (i = 0; i < edev->nb_event_ports; i++) {
		ws = dev->data->ports[i];
		ssows_reset(ws);
		ws->swtag_req = 0;
	}

	for (i = 0; i < edev->nb_event_queues; i++) {
		/* Consume all the events through HWS0 */
		ssows_flush_events(dev->data->ports[0], i, NULL, NULL);

		base = ssovf_bar(OCTEONTX_SSO_GROUP, i, 0);
		base += SSO_VHGRP_QCTL;
		ssovf_write64(1, base); /* Enable SSO group */
	}

	ssovf_fastpath_fns_set(dev);
	return 0;
}

static void
ssows_handle_event(void *arg, struct rte_event event)
{
	struct rte_eventdev *dev = arg;

	if (dev->dev_ops->dev_stop_flush != NULL)
		dev->dev_ops->dev_stop_flush(dev->data->dev_id, event,
					dev->data->dev_stop_flush_arg);
}

static void
ssovf_stop(struct rte_eventdev *dev)
{
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);
	struct ssows *ws;
	uint8_t *base;
	uint8_t i;

	ssovf_func_trace();
	for (i = 0; i < edev->nb_event_ports; i++) {
		ws = dev->data->ports[i];
		ssows_reset(ws);
		ws->swtag_req = 0;
	}

	for (i = 0; i < edev->nb_event_queues; i++) {
		/* Consume all the events through HWS0 */
		ssows_flush_events(dev->data->ports[0], i,
				ssows_handle_event, dev);

		base = ssovf_bar(OCTEONTX_SSO_GROUP, i, 0);
		base += SSO_VHGRP_QCTL;
		ssovf_write64(0, base); /* Disable SSO group */
	}
}

static int
ssovf_close(struct rte_eventdev *dev)
{
	struct ssovf_evdev *edev = ssovf_pmd_priv(dev);
	uint8_t all_queues[RTE_EVENT_MAX_QUEUES_PER_DEV];
	uint8_t i;

	for (i = 0; i < edev->nb_event_queues; i++)
		all_queues[i] = i;

	for (i = 0; i < edev->nb_event_ports; i++)
		ssovf_port_unlink(dev, dev->data->ports[i], all_queues,
			edev->nb_event_queues);
	return 0;
}

static int
ssovf_parsekv(const char *key __rte_unused, const char *value, void *opaque)
{
	int *flag = opaque;
	*flag = !!atoi(value);
	return 0;
}

static int
ssovf_timvf_caps_get(const struct rte_eventdev *dev, uint64_t flags,
		uint32_t *caps, const struct rte_event_timer_adapter_ops **ops)
{
	return timvf_timer_adapter_caps_get(dev, flags, caps, ops,
			timvf_enable_stats);
}

/* Initialize and register event driver with DPDK Application */
static struct rte_eventdev_ops ssovf_ops = {
	.dev_infos_get    = ssovf_info_get,
	.dev_configure    = ssovf_configure,
	.queue_def_conf   = ssovf_queue_def_conf,
	.queue_setup      = ssovf_queue_setup,
	.queue_release    = ssovf_queue_release,
	.port_def_conf    = ssovf_port_def_conf,
	.port_setup       = ssovf_port_setup,
	.port_release     = ssovf_port_release,
	.port_link        = ssovf_port_link,
	.port_unlink      = ssovf_port_unlink,
	.timeout_ticks    = ssovf_timeout_ticks,

	.eth_rx_adapter_caps_get  = ssovf_eth_rx_adapter_caps_get,
	.eth_rx_adapter_queue_add = ssovf_eth_rx_adapter_queue_add,
	.eth_rx_adapter_queue_del = ssovf_eth_rx_adapter_queue_del,
	.eth_rx_adapter_start = ssovf_eth_rx_adapter_start,
	.eth_rx_adapter_stop = ssovf_eth_rx_adapter_stop,

	.eth_tx_adapter_caps_get = ssovf_eth_tx_adapter_caps_get,
	.eth_tx_adapter_create = ssovf_eth_tx_adapter_create,
	.eth_tx_adapter_free = ssovf_eth_tx_adapter_free,
	.eth_tx_adapter_queue_add = ssovf_eth_tx_adapter_queue_add,
	.eth_tx_adapter_queue_del = ssovf_eth_tx_adapter_queue_del,
	.eth_tx_adapter_start = ssovf_eth_tx_adapter_start,
	.eth_tx_adapter_stop = ssovf_eth_tx_adapter_stop,

	.timer_adapter_caps_get = ssovf_timvf_caps_get,

	.dev_selftest = test_eventdev_octeontx,

	.dump             = ssovf_dump,
	.dev_start        = ssovf_start,
	.dev_stop         = ssovf_stop,
	.dev_close        = ssovf_close
};

static int
ssovf_vdev_probe(struct rte_vdev_device *vdev)
{
	struct ssovf_info oinfo;
	struct ssovf_mbox_dev_info info;
	struct ssovf_evdev *edev;
	struct rte_eventdev *eventdev;
	static int ssovf_init_once;
	const char *name;
	const char *params;
	int ret;

	static const char *const args[] = {
		TIMVF_ENABLE_STATS_ARG,
		NULL
	};

	name = rte_vdev_device_name(vdev);
	/* More than one instance is not supported */
	if (ssovf_init_once) {
		ssovf_log_err("Request to create >1 %s instance", name);
		return -EINVAL;
	}

	params = rte_vdev_device_args(vdev);
	if (params != NULL && params[0] != '\0') {
		struct rte_kvargs *kvlist = rte_kvargs_parse(params, args);

		if (!kvlist) {
			ssovf_log_info(
				"Ignoring unsupported params supplied '%s'",
				name);
		} else {
			ret = rte_kvargs_process(kvlist, TIMVF_ENABLE_STATS_ARG,
						 ssovf_parsekv,
						 &timvf_enable_stats);
			if (ret != 0) {
				ssovf_log_err("%s: Error in timvf stats", name);
				rte_kvargs_free(kvlist);
				return ret;
			}
		}

		rte_kvargs_free(kvlist);
	}

	eventdev = rte_event_pmd_vdev_init(name, sizeof(struct ssovf_evdev),
				rte_socket_id());
	if (eventdev == NULL) {
		ssovf_log_err("Failed to create eventdev vdev %s", name);
		return -ENOMEM;
	}
	eventdev->dev_ops = &ssovf_ops;

	timvf_set_eventdevice(eventdev);

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		ssovf_fastpath_fns_set(eventdev);
		return 0;
	}

	octeontx_mbox_init();
	ret = ssovf_info(&oinfo);
	if (ret) {
		ssovf_log_err("Failed to probe and validate ssovfs %d", ret);
		goto error;
	}

	edev = ssovf_pmd_priv(eventdev);
	edev->max_event_ports = oinfo.total_ssowvfs;
	edev->max_event_queues = oinfo.total_ssovfs;
	edev->is_timeout_deq = 0;

	ret = ssovf_mbox_dev_info(&info);
	if (ret < 0 || ret != sizeof(struct ssovf_mbox_dev_info)) {
		ssovf_log_err("Failed to get mbox devinfo %d", ret);
		goto error;
	}

	edev->min_deq_timeout_ns = info.min_deq_timeout_ns;
	edev->max_deq_timeout_ns = info.max_deq_timeout_ns;
	edev->max_num_events =  info.max_num_events;
	edev->available_events = info.max_num_events;

	ssovf_log_dbg("min_deq_tmo=%" PRId64 " max_deq_tmo=%" PRId64
		      " max_evts=%d",
		      info.min_deq_timeout_ns, info.max_deq_timeout_ns,
		      info.max_num_events);

	if (!edev->max_event_ports || !edev->max_event_queues) {
		ssovf_log_err("Not enough eventdev resource queues=%d ports=%d",
			edev->max_event_queues, edev->max_event_ports);
		ret = -ENODEV;
		goto error;
	}

	ssovf_log_info("Initializing %s domain=%d max_queues=%d max_ports=%d",
			name, oinfo.domain, edev->max_event_queues,
			edev->max_event_ports);

	ssovf_init_once = 1;
	return 0;

error:
	rte_event_pmd_vdev_uninit(name);
	return ret;
}

static int
ssovf_vdev_remove(struct rte_vdev_device *vdev)
{
	const char *name;

	name = rte_vdev_device_name(vdev);
	ssovf_log_info("Closing %s", name);
	return rte_event_pmd_vdev_uninit(name);
}

static struct rte_vdev_driver vdev_ssovf_pmd = {
	.probe = ssovf_vdev_probe,
	.remove = ssovf_vdev_remove
};

RTE_PMD_REGISTER_VDEV(EVENTDEV_NAME_OCTEONTX_PMD, vdev_ssovf_pmd);
