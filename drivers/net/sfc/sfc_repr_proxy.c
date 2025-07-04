/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <rte_service.h>
#include <rte_service_component.h>

#include "sfc_log.h"
#include "sfc_service.h"
#include "sfc_repr_proxy.h"
#include "sfc_repr_proxy_api.h"
#include "sfc.h"
#include "sfc_ev.h"
#include "sfc_rx.h"
#include "sfc_tx.h"
#include "sfc_dp_rx.h"

/**
 * Amount of time to wait for the representor proxy routine (which is
 * running on a service core) to handle a request sent via mbox.
 */
#define SFC_REPR_PROXY_MBOX_POLL_TIMEOUT_MS	1000

/**
 * Amount of time to wait for the representor proxy routine (which is
 * running on a service core) to terminate after service core is stopped.
 */
#define SFC_REPR_PROXY_ROUTINE_TERMINATE_TIMEOUT_MS	10000

#define SFC_REPR_INVALID_ROUTE_PORT_ID  (UINT16_MAX)

static struct sfc_repr_proxy *
sfc_repr_proxy_by_adapter(struct sfc_adapter *sa)
{
	return &sa->repr_proxy;
}

static struct sfc_adapter *
sfc_get_adapter_by_pf_port_id(uint16_t pf_port_id)
{
	struct rte_eth_dev *dev;
	struct sfc_adapter *sa;

	SFC_ASSERT(pf_port_id < RTE_MAX_ETHPORTS);

	dev = &rte_eth_devices[pf_port_id];
	sa = sfc_adapter_by_eth_dev(dev);

	return sa;
}

static struct sfc_repr_proxy_port *
sfc_repr_proxy_find_port(struct sfc_repr_proxy *rp, uint16_t repr_id)
{
	struct sfc_repr_proxy_port *port;

	TAILQ_FOREACH(port, &rp->ports, entries) {
		if (port->repr_id == repr_id)
			return port;
	}

	return NULL;
}

static int
sfc_repr_proxy_mbox_send(struct sfc_repr_proxy_mbox *mbox,
			 struct sfc_repr_proxy_port *port,
			 enum sfc_repr_proxy_mbox_op op)
{
	const unsigned int wait_ms = SFC_REPR_PROXY_MBOX_POLL_TIMEOUT_MS;
	unsigned int i;

	mbox->op = op;
	mbox->port = port;
	mbox->ack = false;

	/*
	 * Release ordering enforces marker set after data is populated.
	 * Paired with acquire ordering in sfc_repr_proxy_mbox_handle().
	 */
	__atomic_store_n(&mbox->write_marker, true, __ATOMIC_RELEASE);

	/*
	 * Wait for the representor routine to process the request.
	 * Give up on timeout.
	 */
	for (i = 0; i < wait_ms; i++) {
		/*
		 * Paired with release ordering in sfc_repr_proxy_mbox_handle()
		 * on acknowledge write.
		 */
		if (__atomic_load_n(&mbox->ack, __ATOMIC_ACQUIRE))
			break;

		rte_delay_ms(1);
	}

	if (i == wait_ms) {
		SFC_GENERIC_LOG(ERR,
			"%s() failed to wait for representor proxy routine ack",
			__func__);
		return ETIMEDOUT;
	}

	return 0;
}

static void
sfc_repr_proxy_mbox_handle(struct sfc_repr_proxy *rp)
{
	struct sfc_repr_proxy_mbox *mbox = &rp->mbox;

	/*
	 * Paired with release ordering in sfc_repr_proxy_mbox_send()
	 * on marker set.
	 */
	if (!__atomic_load_n(&mbox->write_marker, __ATOMIC_ACQUIRE))
		return;

	mbox->write_marker = false;

	switch (mbox->op) {
	case SFC_REPR_PROXY_MBOX_ADD_PORT:
		TAILQ_INSERT_TAIL(&rp->ports, mbox->port, entries);
		break;
	case SFC_REPR_PROXY_MBOX_DEL_PORT:
		TAILQ_REMOVE(&rp->ports, mbox->port, entries);
		break;
	case SFC_REPR_PROXY_MBOX_START_PORT:
		mbox->port->started = true;
		break;
	case SFC_REPR_PROXY_MBOX_STOP_PORT:
		mbox->port->started = false;
		break;
	default:
		SFC_ASSERT(0);
		return;
	}

	/*
	 * Paired with acquire ordering in sfc_repr_proxy_mbox_send()
	 * on acknowledge read.
	 */
	__atomic_store_n(&mbox->ack, true, __ATOMIC_RELEASE);
}

static void
sfc_repr_proxy_handle_tx(struct sfc_repr_proxy_dp_txq *rp_txq,
			 struct sfc_repr_proxy_txq *repr_txq)
{
	/*
	 * With multiple representor proxy queues configured it is
	 * possible that not all of the corresponding representor
	 * queues were created. Skip the queues that do not exist.
	 */
	if (repr_txq->ring == NULL)
		return;

	if (rp_txq->available < RTE_DIM(rp_txq->tx_pkts)) {
		rp_txq->available +=
			rte_ring_sc_dequeue_burst(repr_txq->ring,
				(void **)(&rp_txq->tx_pkts[rp_txq->available]),
				RTE_DIM(rp_txq->tx_pkts) - rp_txq->available,
				NULL);

		if (rp_txq->available == rp_txq->transmitted)
			return;
	}

	rp_txq->transmitted += rp_txq->pkt_burst(rp_txq->dp,
				&rp_txq->tx_pkts[rp_txq->transmitted],
				rp_txq->available - rp_txq->transmitted);

	if (rp_txq->available == rp_txq->transmitted) {
		rp_txq->available = 0;
		rp_txq->transmitted = 0;
	}
}

static struct sfc_repr_proxy_port *
sfc_repr_proxy_rx_route_mbuf(struct sfc_repr_proxy *rp, struct rte_mbuf *m)
{
	struct sfc_repr_proxy_port *port;
	efx_mport_id_t mport_id;

	mport_id.id = *RTE_MBUF_DYNFIELD(m, sfc_dp_mport_offset,
					 typeof(&((efx_mport_id_t *)0)->id));

	TAILQ_FOREACH(port, &rp->ports, entries) {
		if (port->egress_mport.id == mport_id.id) {
			m->port = port->rte_port_id;
			m->ol_flags &= ~sfc_dp_mport_override;
			return port;
		}
	}

	return NULL;
}

/*
 * Returns true if a packet is encountered which should be forwarded to a
 * port which is different from the one that is currently routed.
 */
static bool
sfc_repr_proxy_rx_route(struct sfc_repr_proxy *rp,
			struct sfc_repr_proxy_dp_rxq *rp_rxq)
{
	unsigned int i;

	for (i = rp_rxq->routed;
	     i < rp_rxq->available && !rp_rxq->stop_route;
	     i++, rp_rxq->routed++) {
		struct sfc_repr_proxy_port *port;
		struct rte_mbuf *m = rp_rxq->pkts[i];

		port = sfc_repr_proxy_rx_route_mbuf(rp, m);
		/* Cannot find destination representor */
		if (port == NULL) {
			/* Effectively drop the packet */
			rp_rxq->forwarded++;
			continue;
		}

		/* Currently routed packets are mapped to a different port */
		if (port->repr_id != rp_rxq->route_port_id &&
		    rp_rxq->route_port_id != SFC_REPR_INVALID_ROUTE_PORT_ID)
			return true;

		rp_rxq->route_port_id = port->repr_id;
	}

	return false;
}

static void
sfc_repr_proxy_rx_forward(struct sfc_repr_proxy *rp,
			  struct sfc_repr_proxy_dp_rxq *rp_rxq)
{
	struct sfc_repr_proxy_port *port;

	if (rp_rxq->route_port_id != SFC_REPR_INVALID_ROUTE_PORT_ID) {
		port = sfc_repr_proxy_find_port(rp, rp_rxq->route_port_id);

		if (port != NULL && port->started) {
			rp_rxq->forwarded +=
			    rte_ring_sp_enqueue_burst(port->rxq[0].ring,
				(void **)(&rp_rxq->pkts[rp_rxq->forwarded]),
				rp_rxq->routed - rp_rxq->forwarded, NULL);
		} else {
			/* Drop all routed packets if the port is not started */
			rp_rxq->forwarded = rp_rxq->routed;
		}
	}

	if (rp_rxq->forwarded == rp_rxq->routed) {
		rp_rxq->route_port_id = SFC_REPR_INVALID_ROUTE_PORT_ID;
		rp_rxq->stop_route = false;
	} else {
		/* Stall packet routing if not all packets were forwarded */
		rp_rxq->stop_route = true;
	}

	if (rp_rxq->available == rp_rxq->forwarded)
		rp_rxq->available = rp_rxq->forwarded = rp_rxq->routed = 0;
}

static void
sfc_repr_proxy_handle_rx(struct sfc_repr_proxy *rp,
			 struct sfc_repr_proxy_dp_rxq *rp_rxq)
{
	bool route_again;

	if (rp_rxq->available < RTE_DIM(rp_rxq->pkts)) {
		rp_rxq->available += rp_rxq->pkt_burst(rp_rxq->dp,
				&rp_rxq->pkts[rp_rxq->available],
				RTE_DIM(rp_rxq->pkts) - rp_rxq->available);
		if (rp_rxq->available == rp_rxq->forwarded)
			return;
	}

	do {
		route_again = sfc_repr_proxy_rx_route(rp, rp_rxq);
		sfc_repr_proxy_rx_forward(rp, rp_rxq);
	} while (route_again && !rp_rxq->stop_route);
}

static int32_t
sfc_repr_proxy_routine(void *arg)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy *rp = arg;
	unsigned int i;

	sfc_repr_proxy_mbox_handle(rp);

	TAILQ_FOREACH(port, &rp->ports, entries) {
		if (!port->started)
			continue;

		for (i = 0; i < rp->nb_txq; i++)
			sfc_repr_proxy_handle_tx(&rp->dp_txq[i], &port->txq[i]);
	}

	for (i = 0; i < rp->nb_rxq; i++)
		sfc_repr_proxy_handle_rx(rp, &rp->dp_rxq[i]);

	return 0;
}

static struct sfc_txq_info *
sfc_repr_proxy_txq_info_get(struct sfc_adapter *sa, unsigned int repr_queue_id)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy_dp_txq *dp_txq;

	SFC_ASSERT(repr_queue_id < sfc_repr_nb_txq(sas));
	dp_txq = &sa->repr_proxy.dp_txq[repr_queue_id];

	return &sas->txq_info[dp_txq->sw_index];
}

static int
sfc_repr_proxy_txq_attach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	unsigned int i;

	sfc_log_init(sa, "entry");

	for (i = 0; i < sfc_repr_nb_txq(sas); i++) {
		sfc_sw_index_t sw_index = sfc_repr_txq_sw_index(sas, i);

		rp->dp_txq[i].sw_index = sw_index;
	}

	sfc_log_init(sa, "done");

	return 0;
}

static void
sfc_repr_proxy_txq_detach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	unsigned int i;

	sfc_log_init(sa, "entry");

	for (i = 0; i < sfc_repr_nb_txq(sas); i++)
		rp->dp_txq[i].sw_index = 0;

	sfc_log_init(sa, "done");
}

int
sfc_repr_proxy_txq_init(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	const struct rte_eth_txconf tx_conf = {
		.tx_free_thresh = SFC_REPR_PROXY_TXQ_FREE_THRESH,
	};
	struct sfc_txq_info *txq_info;
	unsigned int init_i;
	unsigned int i;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return 0;
	}

	for (init_i = 0; init_i < sfc_repr_nb_txq(sas); init_i++) {
		struct sfc_repr_proxy_dp_txq *txq = &rp->dp_txq[init_i];

		txq_info = &sfc_sa2shared(sa)->txq_info[txq->sw_index];
		if (txq_info->state == SFC_TXQ_INITIALIZED) {
			sfc_log_init(sa,
				"representor proxy TxQ %u is already initialized - skip",
				init_i);
			continue;
		}

		sfc_tx_qinit_info(sa, txq->sw_index);

		rc = sfc_tx_qinit(sa, txq->sw_index,
				  SFC_REPR_PROXY_TX_DESC_COUNT, sa->socket_id,
				  &tx_conf);

		if (rc != 0) {
			sfc_err(sa, "failed to init representor proxy TxQ %u",
				init_i);
			goto fail_init;
		}
	}

	sfc_log_init(sa, "done");

	return 0;

fail_init:
	for (i = 0; i < init_i; i++) {
		struct sfc_repr_proxy_dp_txq *txq = &rp->dp_txq[i];

		txq_info = &sfc_sa2shared(sa)->txq_info[txq->sw_index];
		if (txq_info->state == SFC_TXQ_INITIALIZED)
			sfc_tx_qfini(sa, txq->sw_index);
	}
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

void
sfc_repr_proxy_txq_fini(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	struct sfc_txq_info *txq_info;
	unsigned int i;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return;
	}

	for (i = 0; i < sfc_repr_nb_txq(sas); i++) {
		struct sfc_repr_proxy_dp_txq *txq = &rp->dp_txq[i];

		txq_info = &sfc_sa2shared(sa)->txq_info[txq->sw_index];
		if (txq_info->state != SFC_TXQ_INITIALIZED) {
			sfc_log_init(sa,
				"representor proxy TxQ %u is already finalized - skip",
				i);
			continue;
		}

		sfc_tx_qfini(sa, txq->sw_index);
	}

	sfc_log_init(sa, "done");
}

static int
sfc_repr_proxy_txq_start(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	unsigned int i;

	sfc_log_init(sa, "entry");

	for (i = 0; i < sfc_repr_nb_txq(sas); i++) {
		struct sfc_repr_proxy_dp_txq *txq = &rp->dp_txq[i];

		txq->dp = sfc_repr_proxy_txq_info_get(sa, i)->dp;
		txq->pkt_burst = sa->eth_dev->tx_pkt_burst;
		txq->available = 0;
		txq->transmitted = 0;
	}

	sfc_log_init(sa, "done");

	return 0;
}

static void
sfc_repr_proxy_txq_stop(struct sfc_adapter *sa)
{
	sfc_log_init(sa, "entry");
	sfc_log_init(sa, "done");
}

static int
sfc_repr_proxy_rxq_attach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	unsigned int i;

	sfc_log_init(sa, "entry");

	for (i = 0; i < sfc_repr_nb_rxq(sas); i++) {
		sfc_sw_index_t sw_index = sfc_repr_rxq_sw_index(sas, i);

		rp->dp_rxq[i].sw_index = sw_index;
	}

	sfc_log_init(sa, "done");

	return 0;
}

static void
sfc_repr_proxy_rxq_detach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	unsigned int i;

	sfc_log_init(sa, "entry");

	for (i = 0; i < sfc_repr_nb_rxq(sas); i++)
		rp->dp_rxq[i].sw_index = 0;

	sfc_log_init(sa, "done");
}

static struct sfc_rxq_info *
sfc_repr_proxy_rxq_info_get(struct sfc_adapter *sa, unsigned int repr_queue_id)
{
	struct sfc_adapter_shared *sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy_dp_rxq *dp_rxq;

	SFC_ASSERT(repr_queue_id < sfc_repr_nb_rxq(sas));
	dp_rxq = &sa->repr_proxy.dp_rxq[repr_queue_id];

	return &sas->rxq_info[dp_rxq->sw_index];
}

static int
sfc_repr_proxy_rxq_init(struct sfc_adapter *sa,
			struct sfc_repr_proxy_dp_rxq *rxq)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	uint16_t nb_rx_desc = SFC_REPR_PROXY_RX_DESC_COUNT;
	struct sfc_rxq_info *rxq_info;
	struct rte_eth_rxconf rxconf = {
		.rx_free_thresh = SFC_REPR_PROXY_RXQ_REFILL_LEVEL,
		.rx_drop_en = 1,
	};
	int rc;

	sfc_log_init(sa, "entry");

	rxq_info = &sas->rxq_info[rxq->sw_index];
	if (rxq_info->state & SFC_RXQ_INITIALIZED) {
		sfc_log_init(sa, "RxQ is already initialized - skip");
		return 0;
	}

	nb_rx_desc = RTE_MIN(nb_rx_desc, sa->rxq_max_entries);
	nb_rx_desc = RTE_MAX(nb_rx_desc, sa->rxq_min_entries);

	rc = sfc_rx_qinit_info(sa, rxq->sw_index, EFX_RXQ_FLAG_INGRESS_MPORT);
	if (rc != 0) {
		sfc_err(sa, "failed to init representor proxy RxQ info");
		goto fail_repr_rxq_init_info;
	}

	rc = sfc_rx_qinit(sa, rxq->sw_index, nb_rx_desc, sa->socket_id, &rxconf,
			  rxq->mp);
	if (rc != 0) {
		sfc_err(sa, "failed to init representor proxy RxQ");
		goto fail_repr_rxq_init;
	}

	sfc_log_init(sa, "done");

	return 0;

fail_repr_rxq_init:
fail_repr_rxq_init_info:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));

	return rc;
}

static void
sfc_repr_proxy_rxq_fini(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	struct sfc_rxq_info *rxq_info;
	unsigned int i;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return;
	}

	for (i = 0; i < sfc_repr_nb_rxq(sas); i++) {
		struct sfc_repr_proxy_dp_rxq *rxq = &rp->dp_rxq[i];

		rxq_info = &sas->rxq_info[rxq->sw_index];
		if (rxq_info->state != SFC_RXQ_INITIALIZED) {
			sfc_log_init(sa,
				"representor RxQ %u is already finalized - skip",
				i);
			continue;
		}

		sfc_rx_qfini(sa, rxq->sw_index);
	}

	sfc_log_init(sa, "done");
}

static void
sfc_repr_proxy_rxq_stop(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	unsigned int i;

	sfc_log_init(sa, "entry");

	for (i = 0; i < sfc_repr_nb_rxq(sas); i++)
		sfc_rx_qstop(sa, sa->repr_proxy.dp_rxq[i].sw_index);

	sfc_repr_proxy_rxq_fini(sa);

	sfc_log_init(sa, "done");
}

static int
sfc_repr_proxy_rxq_start(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	unsigned int i;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return 0;
	}

	for (i = 0; i < sfc_repr_nb_rxq(sas); i++) {
		struct sfc_repr_proxy_dp_rxq *rxq = &rp->dp_rxq[i];

		rc = sfc_repr_proxy_rxq_init(sa, rxq);
		if (rc != 0) {
			sfc_err(sa, "failed to init representor proxy RxQ %u",
				i);
			goto fail_init;
		}

		rc = sfc_rx_qstart(sa, rxq->sw_index);
		if (rc != 0) {
			sfc_err(sa, "failed to start representor proxy RxQ %u",
				i);
			goto fail_start;
		}

		rxq->dp = sfc_repr_proxy_rxq_info_get(sa, i)->dp;
		rxq->pkt_burst = sa->eth_dev->rx_pkt_burst;
		rxq->available = 0;
		rxq->routed = 0;
		rxq->forwarded = 0;
		rxq->stop_route = false;
		rxq->route_port_id = SFC_REPR_INVALID_ROUTE_PORT_ID;
	}

	sfc_log_init(sa, "done");

	return 0;

fail_start:
fail_init:
	sfc_repr_proxy_rxq_stop(sa);
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	return rc;
}

static int
sfc_repr_proxy_mae_rule_insert(struct sfc_adapter *sa,
			       struct sfc_repr_proxy_port *port)
{
	int rc = EINVAL;

	sfc_log_init(sa, "entry");

	port->mae_rule = sfc_mae_repr_flow_create(sa,
				    SFC_MAE_RULE_PRIO_LOWEST, port->rte_port_id,
				    RTE_FLOW_ACTION_TYPE_PORT_REPRESENTOR,
				    RTE_FLOW_ITEM_TYPE_REPRESENTED_PORT);
	if (port->mae_rule == NULL) {
		sfc_err(sa, "failed to insert MAE rule for repr %u",
			port->repr_id);
		goto fail_rule_add;
	}

	sfc_log_init(sa, "done");

	return 0;

fail_rule_add:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	return rc;
}

static void
sfc_repr_proxy_mae_rule_remove(struct sfc_adapter *sa,
			       struct sfc_repr_proxy_port *port)
{
	sfc_mae_repr_flow_destroy(sa, port->mae_rule);
}

static int
sfc_repr_proxy_mport_filter_insert(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	struct sfc_rxq *rxq_ctrl;
	struct sfc_repr_proxy_filter *filter = &rp->mport_filter;
	efx_mport_sel_t mport_alias_selector;
	static const efx_filter_match_flags_t flags[RTE_DIM(filter->specs)] = {
		EFX_FILTER_MATCH_UNKNOWN_UCAST_DST,
		EFX_FILTER_MATCH_UNKNOWN_MCAST_DST };
	unsigned int i;
	int rc;

	sfc_log_init(sa, "entry");

	if (sfc_repr_nb_rxq(sas) == 1) {
		rxq_ctrl = &sa->rxq_ctrl[rp->dp_rxq[0].sw_index];
	} else {
		sfc_err(sa, "multiple representor proxy RxQs not supported");
		rc = ENOTSUP;
		goto fail_multiple_queues;
	}

	rc = efx_mae_mport_by_id(&rp->mport_alias, &mport_alias_selector);
	if (rc != 0) {
		sfc_err(sa, "failed to get repr proxy mport by ID");
		goto fail_get_selector;
	}

	memset(filter->specs, 0, sizeof(filter->specs));
	for (i = 0; i < RTE_DIM(filter->specs); i++) {
		filter->specs[i].efs_priority = EFX_FILTER_PRI_MANUAL;
		filter->specs[i].efs_flags = EFX_FILTER_FLAG_RX;
		filter->specs[i].efs_dmaq_id = rxq_ctrl->hw_index;
		filter->specs[i].efs_match_flags = flags[i] |
				EFX_FILTER_MATCH_MPORT;
		filter->specs[i].efs_ingress_mport = mport_alias_selector.sel;

		rc = efx_filter_insert(sa->nic, &filter->specs[i]);
		if (rc != 0) {
			sfc_err(sa, "failed to insert repr proxy filter");
			goto fail_insert;
		}
	}

	sfc_log_init(sa, "done");

	return 0;

fail_insert:
	while (i-- > 0)
		efx_filter_remove(sa->nic, &filter->specs[i]);

fail_get_selector:
fail_multiple_queues:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	return rc;
}

static void
sfc_repr_proxy_mport_filter_remove(struct sfc_adapter *sa)
{
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	struct sfc_repr_proxy_filter *filter = &rp->mport_filter;
	unsigned int i;

	for (i = 0; i < RTE_DIM(filter->specs); i++)
		efx_filter_remove(sa->nic, &filter->specs[i]);
}

static int
sfc_repr_proxy_port_rule_insert(struct sfc_adapter *sa,
				struct sfc_repr_proxy_port *port)
{
	int rc;

	rc = sfc_repr_proxy_mae_rule_insert(sa, port);
	if (rc != 0)
		goto fail_mae_rule_insert;

	return 0;

fail_mae_rule_insert:
	return rc;
}

static void
sfc_repr_proxy_port_rule_remove(struct sfc_adapter *sa,
				struct sfc_repr_proxy_port *port)
{
	sfc_repr_proxy_mae_rule_remove(sa, port);
}

static int
sfc_repr_proxy_ports_init(struct sfc_adapter *sa)
{
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	int rc;

	sfc_log_init(sa, "entry");

	rc = efx_mcdi_mport_alloc_alias(sa->nic, &rp->mport_alias, NULL);
	if (rc != 0) {
		sfc_err(sa, "failed to alloc mport alias: %s",
			rte_strerror(rc));
		goto fail_alloc_mport_alias;
	}

	TAILQ_INIT(&rp->ports);

	sfc_log_init(sa, "done");

	return 0;

fail_alloc_mport_alias:

	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	return rc;
}

void
sfc_repr_proxy_pre_detach(struct sfc_adapter *sa)
{
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	bool close_ports[RTE_MAX_ETHPORTS] = {0};
	struct sfc_repr_proxy_port *port;
	unsigned int i;

	SFC_ASSERT(!sfc_adapter_is_locked(sa));

	sfc_adapter_lock(sa);

	if (sfc_repr_available(sfc_sa2shared(sa))) {
		TAILQ_FOREACH(port, &rp->ports, entries)
			close_ports[port->rte_port_id] = true;
	} else {
		sfc_log_init(sa, "representors not supported - skip");
	}

	sfc_adapter_unlock(sa);

	for (i = 0; i < RTE_DIM(close_ports); i++) {
		if (close_ports[i]) {
			rte_eth_dev_stop(i);
			rte_eth_dev_close(i);
		}
	}
}

static void
sfc_repr_proxy_ports_fini(struct sfc_adapter *sa)
{
	struct sfc_repr_proxy *rp = &sa->repr_proxy;

	efx_mae_mport_free(sa->nic, &rp->mport_alias);
}

int
sfc_repr_proxy_attach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	struct rte_service_spec service;
	uint32_t cid;
	uint32_t sid;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return 0;
	}

	rc = sfc_repr_proxy_rxq_attach(sa);
	if (rc != 0)
		goto fail_rxq_attach;

	rc = sfc_repr_proxy_txq_attach(sa);
	if (rc != 0)
		goto fail_txq_attach;

	rc = sfc_repr_proxy_ports_init(sa);
	if (rc != 0)
		goto fail_ports_init;

	cid = sfc_get_service_lcore(sa->socket_id);
	if (cid == RTE_MAX_LCORE && sa->socket_id != SOCKET_ID_ANY) {
		/* Warn and try to allocate on any NUMA node */
		sfc_warn(sa,
			"repr proxy: unable to get service lcore at socket %d",
			sa->socket_id);

		cid = sfc_get_service_lcore(SOCKET_ID_ANY);
	}
	if (cid == RTE_MAX_LCORE) {
		rc = ENOTSUP;
		sfc_err(sa, "repr proxy: failed to get service lcore");
		goto fail_get_service_lcore;
	}

	memset(&service, 0, sizeof(service));
	snprintf(service.name, sizeof(service.name),
		 "net_sfc_%hu_repr_proxy", sfc_sa2shared(sa)->port_id);
	service.socket_id = rte_lcore_to_socket_id(cid);
	service.callback = sfc_repr_proxy_routine;
	service.callback_userdata = rp;

	rc = rte_service_component_register(&service, &sid);
	if (rc != 0) {
		rc = ENOEXEC;
		sfc_err(sa, "repr proxy: failed to register service component");
		goto fail_register;
	}

	rc = rte_service_map_lcore_set(sid, cid, 1);
	if (rc != 0) {
		rc = -rc;
		sfc_err(sa, "repr proxy: failed to map lcore");
		goto fail_map_lcore;
	}

	rp->service_core_id = cid;
	rp->service_id = sid;

	sfc_log_init(sa, "done");

	return 0;

fail_map_lcore:
	rte_service_component_unregister(sid);

fail_register:
	/*
	 * No need to rollback service lcore get since
	 * it just makes socket_id based search and remembers it.
	 */

fail_get_service_lcore:
	sfc_repr_proxy_ports_fini(sa);

fail_ports_init:
	sfc_repr_proxy_txq_detach(sa);

fail_txq_attach:
	sfc_repr_proxy_rxq_detach(sa);

fail_rxq_attach:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	return rc;
}

void
sfc_repr_proxy_detach(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return;
	}

	rte_service_map_lcore_set(rp->service_id, rp->service_core_id, 0);
	rte_service_component_unregister(rp->service_id);
	sfc_repr_proxy_ports_fini(sa);
	sfc_repr_proxy_rxq_detach(sa);
	sfc_repr_proxy_txq_detach(sa);

	sfc_log_init(sa, "done");
}

static int
sfc_repr_proxy_do_start_port(struct sfc_adapter *sa,
			   struct sfc_repr_proxy_port *port)
{
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	int rc;

	rc = sfc_repr_proxy_port_rule_insert(sa, port);
	if (rc != 0)
		goto fail_filter_insert;

	if (rp->started) {
		rc = sfc_repr_proxy_mbox_send(&rp->mbox, port,
					      SFC_REPR_PROXY_MBOX_START_PORT);
		if (rc != 0) {
			sfc_err(sa, "failed to start proxy port %u",
				port->repr_id);
			goto fail_port_start;
		}
	} else {
		port->started = true;
	}

	return 0;

fail_port_start:
	sfc_repr_proxy_port_rule_remove(sa, port);
fail_filter_insert:
	sfc_err(sa, "%s() failed %s", __func__, rte_strerror(rc));

	return rc;
}

static int
sfc_repr_proxy_do_stop_port(struct sfc_adapter *sa,
			  struct sfc_repr_proxy_port *port)

{
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	int rc;

	if (rp->started) {
		rc = sfc_repr_proxy_mbox_send(&rp->mbox, port,
					      SFC_REPR_PROXY_MBOX_STOP_PORT);
		if (rc != 0) {
			sfc_err(sa, "failed to stop proxy port %u: %s",
				port->repr_id, rte_strerror(rc));
			return rc;
		}
	} else {
		port->started = false;
	}

	sfc_repr_proxy_port_rule_remove(sa, port);

	return 0;
}

static bool
sfc_repr_proxy_port_enabled(struct sfc_repr_proxy_port *port)
{
	return port->rte_port_id != RTE_MAX_ETHPORTS && port->enabled;
}

static bool
sfc_repr_proxy_ports_disabled(struct sfc_repr_proxy *rp)
{
	struct sfc_repr_proxy_port *port;

	TAILQ_FOREACH(port, &rp->ports, entries) {
		if (sfc_repr_proxy_port_enabled(port))
			return false;
	}

	return true;
}

int
sfc_repr_proxy_start(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	struct sfc_repr_proxy_port *last_port = NULL;
	struct sfc_repr_proxy_port *port;
	int rc;

	sfc_log_init(sa, "entry");

	/* Representor proxy is not started when no representors are started */
	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return 0;
	}

	if (sfc_repr_proxy_ports_disabled(rp)) {
		sfc_log_init(sa, "no started representor ports - skip");
		return 0;
	}

	rc = sfc_repr_proxy_rxq_start(sa);
	if (rc != 0)
		goto fail_rxq_start;

	rc = sfc_repr_proxy_txq_start(sa);
	if (rc != 0)
		goto fail_txq_start;

	rp->nb_txq = sfc_repr_nb_txq(sas);
	rp->nb_rxq = sfc_repr_nb_rxq(sas);

	/* Service core may be in "stopped" state, start it */
	rc = rte_service_lcore_start(rp->service_core_id);
	if (rc != 0 && rc != -EALREADY) {
		rc = -rc;
		sfc_err(sa, "failed to start service core for %s: %s",
			rte_service_get_name(rp->service_id),
			rte_strerror(rc));
		goto fail_start_core;
	}

	/* Run the service */
	rc = rte_service_component_runstate_set(rp->service_id, 1);
	if (rc < 0) {
		rc = -rc;
		sfc_err(sa, "failed to run %s component: %s",
			rte_service_get_name(rp->service_id),
			rte_strerror(rc));
		goto fail_component_runstate_set;
	}
	rc = rte_service_runstate_set(rp->service_id, 1);
	if (rc < 0) {
		rc = -rc;
		sfc_err(sa, "failed to run %s: %s",
			rte_service_get_name(rp->service_id),
			rte_strerror(rc));
		goto fail_runstate_set;
	}

	TAILQ_FOREACH(port, &rp->ports, entries) {
		if (sfc_repr_proxy_port_enabled(port)) {
			rc = sfc_repr_proxy_do_start_port(sa, port);
			if (rc != 0)
				goto fail_start_id;

			last_port = port;
		}
	}

	rc = sfc_repr_proxy_mport_filter_insert(sa);
	if (rc != 0)
		goto fail_mport_filter_insert;

	rp->started = true;

	sfc_log_init(sa, "done");

	return 0;

fail_mport_filter_insert:
fail_start_id:
	if (last_port != NULL) {
		TAILQ_FOREACH(port, &rp->ports, entries) {
			if (sfc_repr_proxy_port_enabled(port)) {
				(void)sfc_repr_proxy_do_stop_port(sa, port);
				if (port == last_port)
					break;
			}
		}
	}

	rte_service_runstate_set(rp->service_id, 0);

fail_runstate_set:
	rte_service_component_runstate_set(rp->service_id, 0);

fail_component_runstate_set:
	/* Service lcore may be shared and we never stop it */

fail_start_core:
	sfc_repr_proxy_txq_stop(sa);

fail_txq_start:
	sfc_repr_proxy_rxq_stop(sa);

fail_rxq_start:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	return rc;
}

void
sfc_repr_proxy_stop(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	struct sfc_repr_proxy_port *port;
	const unsigned int wait_ms_total =
		SFC_REPR_PROXY_ROUTINE_TERMINATE_TIMEOUT_MS;
	unsigned int i;
	int rc;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return;
	}

	if (sfc_repr_proxy_ports_disabled(rp)) {
		sfc_log_init(sa, "no started representor ports - skip");
		return;
	}

	TAILQ_FOREACH(port, &rp->ports, entries) {
		if (sfc_repr_proxy_port_enabled(port)) {
			rc = sfc_repr_proxy_do_stop_port(sa, port);
			if (rc != 0) {
				sfc_err(sa,
					"failed to stop representor proxy port %u: %s",
					port->repr_id, rte_strerror(rc));
			}
		}
	}

	sfc_repr_proxy_mport_filter_remove(sa);

	rc = rte_service_runstate_set(rp->service_id, 0);
	if (rc < 0) {
		sfc_err(sa, "failed to stop %s: %s",
			rte_service_get_name(rp->service_id),
			rte_strerror(-rc));
	}

	rc = rte_service_component_runstate_set(rp->service_id, 0);
	if (rc < 0) {
		sfc_err(sa, "failed to stop %s component: %s",
			rte_service_get_name(rp->service_id),
			rte_strerror(-rc));
	}

	/* Service lcore may be shared and we never stop it */

	/*
	 * Wait for the representor proxy routine to finish the last iteration.
	 * Give up on timeout.
	 */
	for (i = 0; i < wait_ms_total; i++) {
		if (rte_service_may_be_active(rp->service_id) == 0)
			break;

		rte_delay_ms(1);
	}

	sfc_repr_proxy_rxq_stop(sa);
	sfc_repr_proxy_txq_stop(sa);

	rp->started = false;

	sfc_log_init(sa, "done");
}

int
sfc_repr_proxy_add_port(uint16_t pf_port_id, uint16_t repr_id,
			uint16_t rte_port_id, const efx_mport_sel_t *mport_sel,
			efx_pcie_interface_t intf, uint16_t pf, uint16_t vf)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;
	int rc;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");
	TAILQ_FOREACH(port, &rp->ports, entries) {
		if (port->rte_port_id == rte_port_id) {
			rc = EEXIST;
			sfc_err(sa, "%s() failed: port exists", __func__);
			goto fail_port_exists;
		}
	}

	port = rte_zmalloc("sfc-repr-proxy-port", sizeof(*port),
			   sa->socket_id);
	if (port == NULL) {
		rc = ENOMEM;
		sfc_err(sa, "failed to alloc memory for proxy port");
		goto fail_alloc_port;
	}

	rc = efx_mae_mport_id_by_selector(sa->nic, mport_sel,
					  &port->egress_mport);
	if (rc != 0) {
		sfc_err(sa,
			"failed get MAE mport id by selector (repr_id %u): %s",
			repr_id, rte_strerror(rc));
		goto fail_mport_id;
	}

	port->rte_port_id = rte_port_id;
	port->repr_id = repr_id;

	rc = efx_mcdi_get_client_handle(sa->nic, intf, pf, vf,
					&port->remote_vnic_mcdi_client_handle);
	if (rc != 0) {
		sfc_err(sa, "failed to get the represented VNIC's MCDI handle (repr_id=%u): %s",
			repr_id, rte_strerror(rc));
		goto fail_client_handle;
	}

	if (rp->started) {
		rc = sfc_repr_proxy_mbox_send(&rp->mbox, port,
					      SFC_REPR_PROXY_MBOX_ADD_PORT);
		if (rc != 0) {
			sfc_err(sa, "failed to add proxy port %u",
				port->repr_id);
			goto fail_port_add;
		}
	} else {
		TAILQ_INSERT_TAIL(&rp->ports, port, entries);
	}

	sfc_log_init(sa, "done");
	sfc_adapter_unlock(sa);

	return 0;

fail_port_add:
fail_client_handle:
fail_mport_id:
	rte_free(port);
fail_alloc_port:
fail_port_exists:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	sfc_adapter_unlock(sa);

	return rc;
}

int
sfc_repr_proxy_del_port(uint16_t pf_port_id, uint16_t repr_id)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;
	int rc;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "failed: no such port");
		rc = ENOENT;
		goto fail_no_port;
	}

	if (rp->started) {
		rc = sfc_repr_proxy_mbox_send(&rp->mbox, port,
					      SFC_REPR_PROXY_MBOX_DEL_PORT);
		if (rc != 0) {
			sfc_err(sa, "failed to remove proxy port %u",
				port->repr_id);
			goto fail_port_remove;
		}
	} else {
		TAILQ_REMOVE(&rp->ports, port, entries);
	}

	rte_free(port);

	sfc_log_init(sa, "done");

	sfc_adapter_unlock(sa);

	return 0;

fail_port_remove:
fail_no_port:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	sfc_adapter_unlock(sa);

	return rc;
}

int
sfc_repr_proxy_add_rxq(uint16_t pf_port_id, uint16_t repr_id,
		       uint16_t queue_id, struct rte_ring *rx_ring,
		       struct rte_mempool *mp)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy_rxq *rxq;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		sfc_adapter_unlock(sa);
		return ENOENT;
	}

	rxq = &port->rxq[queue_id];
	if (rp->dp_rxq[queue_id].mp != NULL && rp->dp_rxq[queue_id].mp != mp) {
		sfc_err(sa, "multiple mempools per queue are not supported");
		sfc_adapter_unlock(sa);
		return ENOTSUP;
	}

	rxq->ring = rx_ring;
	rxq->mb_pool = mp;
	rp->dp_rxq[queue_id].mp = mp;
	rp->dp_rxq[queue_id].ref_count++;

	sfc_log_init(sa, "done");
	sfc_adapter_unlock(sa);

	return 0;
}

void
sfc_repr_proxy_del_rxq(uint16_t pf_port_id, uint16_t repr_id,
		       uint16_t queue_id)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy_rxq *rxq;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		sfc_adapter_unlock(sa);
		return;
	}

	rxq = &port->rxq[queue_id];

	rxq->ring = NULL;
	rxq->mb_pool = NULL;
	rp->dp_rxq[queue_id].ref_count--;
	if (rp->dp_rxq[queue_id].ref_count == 0)
		rp->dp_rxq[queue_id].mp = NULL;

	sfc_log_init(sa, "done");
	sfc_adapter_unlock(sa);
}

int
sfc_repr_proxy_add_txq(uint16_t pf_port_id, uint16_t repr_id,
		       uint16_t queue_id, struct rte_ring *tx_ring,
		       efx_mport_id_t *egress_mport)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy_txq *txq;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		sfc_adapter_unlock(sa);
		return ENOENT;
	}

	txq = &port->txq[queue_id];

	txq->ring = tx_ring;

	*egress_mport = port->egress_mport;

	sfc_log_init(sa, "done");
	sfc_adapter_unlock(sa);

	return 0;
}

void
sfc_repr_proxy_del_txq(uint16_t pf_port_id, uint16_t repr_id,
		       uint16_t queue_id)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy_txq *txq;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		sfc_adapter_unlock(sa);
		return;
	}

	txq = &port->txq[queue_id];

	txq->ring = NULL;

	sfc_log_init(sa, "done");
	sfc_adapter_unlock(sa);
}

int
sfc_repr_proxy_start_repr(uint16_t pf_port_id, uint16_t repr_id)
{
	bool proxy_start_required = false;
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;
	int rc;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		rc = ENOENT;
		goto fail_not_found;
	}

	if (port->enabled) {
		rc = EALREADY;
		sfc_err(sa, "failed: repr %u proxy port already started",
			repr_id);
		goto fail_already_started;
	}

	if (sa->state == SFC_ETHDEV_STARTED) {
		if (sfc_repr_proxy_ports_disabled(rp)) {
			proxy_start_required = true;
		} else {
			rc = sfc_repr_proxy_do_start_port(sa, port);
			if (rc != 0) {
				sfc_err(sa,
					"failed to start repr %u proxy port",
					repr_id);
				goto fail_start_id;
			}
		}
	}

	port->enabled = true;

	if (proxy_start_required) {
		rc = sfc_repr_proxy_start(sa);
		if (rc != 0) {
			sfc_err(sa, "failed to start proxy");
			goto fail_proxy_start;
		}
	}

	sfc_log_init(sa, "done");
	sfc_adapter_unlock(sa);

	return 0;

fail_proxy_start:
	port->enabled = false;

fail_start_id:
fail_already_started:
fail_not_found:
	sfc_err(sa, "failed to start repr %u proxy port: %s", repr_id,
		rte_strerror(rc));
	sfc_adapter_unlock(sa);

	return rc;
}

int
sfc_repr_proxy_stop_repr(uint16_t pf_port_id, uint16_t repr_id)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy_port *p;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;
	int rc;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		sfc_adapter_unlock(sa);
		return ENOENT;
	}

	if (!port->enabled) {
		sfc_log_init(sa, "repr %u proxy port is not started - skip",
			     repr_id);
		sfc_adapter_unlock(sa);
		return 0;
	}

	if (sa->state == SFC_ETHDEV_STARTED) {
		bool last_enabled = true;

		TAILQ_FOREACH(p, &rp->ports, entries) {
			if (p == port)
				continue;

			if (sfc_repr_proxy_port_enabled(p)) {
				last_enabled = false;
				break;
			}
		}

		rc = 0;
		if (last_enabled)
			sfc_repr_proxy_stop(sa);
		else
			rc = sfc_repr_proxy_do_stop_port(sa, port);

		if (rc != 0) {
			sfc_err(sa,
				"failed to stop representor proxy TxQ %u: %s",
				repr_id, rte_strerror(rc));
			sfc_adapter_unlock(sa);
			return rc;
		}
	}

	port->enabled = false;

	sfc_log_init(sa, "done");
	sfc_adapter_unlock(sa);

	return 0;
}

int
sfc_repr_proxy_repr_entity_mac_addr_set(uint16_t pf_port_id, uint16_t repr_id,
					const struct rte_ether_addr *mac_addr)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;
	int rc;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port (repr_id=%u)",
			__func__, repr_id);
		sfc_adapter_unlock(sa);
		return ENOENT;
	}

	rc = efx_mcdi_client_mac_addr_set(sa->nic,
					  port->remote_vnic_mcdi_client_handle,
					  mac_addr->addr_bytes);
	if (rc != 0) {
		sfc_err(sa, "%s() failed: cannot set MAC address (repr_id=%u): %s",
			__func__, repr_id, rte_strerror(rc));
	}

	sfc_adapter_unlock(sa);

	return rc;
}

void
sfc_repr_proxy_mport_alias_get(uint16_t pf_port_id, efx_mport_id_t *mport_alias)
{
	const struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
	sfc_adapter_lock(sa);
	rp = sfc_repr_proxy_by_adapter(sa);

	memcpy(mport_alias, &rp->mport_alias, sizeof(*mport_alias));

	sfc_adapter_unlock(sa);
}
