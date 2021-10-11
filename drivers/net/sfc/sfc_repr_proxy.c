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

/**
 * Amount of time to wait for the representor proxy routine (which is
 * running on a service core) to handle a request sent via mbox.
 */
#define SFC_REPR_PROXY_MBOX_POLL_TIMEOUT_MS	1000

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

	sfc_adapter_lock(sa);

	return sa;
}

static void
sfc_put_adapter(struct sfc_adapter *sa)
{
	sfc_adapter_unlock(sa);
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

static int32_t
sfc_repr_proxy_routine(void *arg)
{
	struct sfc_repr_proxy *rp = arg;

	sfc_repr_proxy_mbox_handle(rp);

	return 0;
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

	sfc_log_init(sa, "done");
}

int
sfc_repr_proxy_start(struct sfc_adapter *sa)
{
	struct sfc_adapter_shared * const sas = sfc_sa2shared(sa);
	struct sfc_repr_proxy *rp = &sa->repr_proxy;
	int rc;

	sfc_log_init(sa, "entry");

	/*
	 * The condition to start the proxy is insufficient. It will be
	 * complemented with representor port start/stop support.
	 */
	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return 0;
	}

	rc = sfc_repr_proxy_rxq_start(sa);
	if (rc != 0)
		goto fail_rxq_start;

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

	rp->started = true;

	sfc_log_init(sa, "done");

	return 0;

fail_runstate_set:
	rte_service_component_runstate_set(rp->service_id, 0);

fail_component_runstate_set:
	/* Service lcore may be shared and we never stop it */

fail_start_core:
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
	int rc;

	sfc_log_init(sa, "entry");

	if (!sfc_repr_available(sas)) {
		sfc_log_init(sa, "representors not supported - skip");
		return;
	}

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

	sfc_repr_proxy_rxq_stop(sa);

	rp->started = false;

	sfc_log_init(sa, "done");
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

int
sfc_repr_proxy_add_port(uint16_t pf_port_id, uint16_t repr_id,
			uint16_t rte_port_id, const efx_mport_sel_t *mport_sel)
{
	struct sfc_repr_proxy_port *port;
	struct sfc_repr_proxy *rp;
	struct sfc_adapter *sa;
	int rc;

	sa = sfc_get_adapter_by_pf_port_id(pf_port_id);
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
	sfc_put_adapter(sa);

	return 0;

fail_port_add:
fail_mport_id:
	rte_free(port);
fail_alloc_port:
fail_port_exists:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	sfc_put_adapter(sa);

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

	sfc_put_adapter(sa);

	return 0;

fail_port_remove:
fail_no_port:
	sfc_log_init(sa, "failed: %s", rte_strerror(rc));
	sfc_put_adapter(sa);

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
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		return ENOENT;
	}

	rxq = &port->rxq[queue_id];
	if (rp->dp_rxq[queue_id].mp != NULL && rp->dp_rxq[queue_id].mp != mp) {
		sfc_err(sa, "multiple mempools per queue are not supported");
		sfc_put_adapter(sa);
		return ENOTSUP;
	}

	rxq->ring = rx_ring;
	rxq->mb_pool = mp;
	rp->dp_rxq[queue_id].mp = mp;
	rp->dp_rxq[queue_id].ref_count++;

	sfc_log_init(sa, "done");
	sfc_put_adapter(sa);

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
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		return;
	}

	rxq = &port->rxq[queue_id];

	rxq->ring = NULL;
	rxq->mb_pool = NULL;
	rp->dp_rxq[queue_id].ref_count--;
	if (rp->dp_rxq[queue_id].ref_count == 0)
		rp->dp_rxq[queue_id].mp = NULL;

	sfc_log_init(sa, "done");
	sfc_put_adapter(sa);
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
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		return ENOENT;
	}

	txq = &port->txq[queue_id];

	txq->ring = tx_ring;

	*egress_mport = port->egress_mport;

	sfc_log_init(sa, "done");
	sfc_put_adapter(sa);

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
	rp = sfc_repr_proxy_by_adapter(sa);

	sfc_log_init(sa, "entry");

	port = sfc_repr_proxy_find_port(rp, repr_id);
	if (port == NULL) {
		sfc_err(sa, "%s() failed: no such port", __func__);
		return;
	}

	txq = &port->txq[queue_id];

	txq->ring = NULL;

	sfc_log_init(sa, "done");
	sfc_put_adapter(sa);
}
